/*
 * Lab 5.2 — Closed-loop air temperature with a relay-driven resistor heater.
 *
 * Architecture: APP -> SRV(controllers) -> ECAL(SerialStdio, DHT) -> MCAL(Arduino) -> HW
 *
 * Tasks (FreeRTOS, all heater-only — no fan, no motor):
 *   acq   1000 ms  : read DHT11, push tempC into shared state
 *   ctrl   250 ms  : compute heater demand (ON-OFF or PID) → control queue
 *   act     50 ms  : drive relay using time-proportional switching
 *   disp   500 ms  : refresh I²C LCD
 *   cmd     80 ms  : parse Serial commands (sscanf-based)
 *
 * Default baud is 115200 (set by SerialStdioInit in main.cpp). Each accepted
 * command is echoed back. Type `help` for the full list.
 */

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <avr/wdt.h>
#include <avr/io.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "Lab5_2_main.h"
#include "Lab5_2_Shared.h"
#include "ctrl_pid.h"
#include "ctrl_onoff_hyst.h"
#include "../sensor/DhtSensorDriver.h"

#if defined(__AVR_ATmega328P__)
#error "Lab5_2 requires Arduino Mega 2560 (ATmega2560). Update board in platformio.ini and wiring."
#endif

// ============================================================================
// Globals
// ============================================================================

Lab52Shared g_lab52;

/* Sensor type comes from LAB5_2_DHT_TYPE in Lab5_2_Shared.h. The two chips
 * share the wire protocol but the library decodes the 5 raw bytes very
 * differently — a mismatch produces checksum-valid garbage (e.g. ≈ -0.4 °C
 * and 14 % when a DHT22 is read as DHT11). Use the `dhtprobe` Serial
 * command to confirm the right type. */
static DhtSensorDriver s_dht(LAB5_2_DHT_PIN, LAB5_2_DHT_TYPE);
static PidController52 s_pid;
static OnOffHysteresisController52 s_onoff;
static LiquidCrystal_I2C s_lcd(LAB5_2_LCD_I2C_ADDR, 16, 2);

/*
 * --- Reset / boot diagnostics ----------------------------------------------
 *
 * If the MCU is reset-looping (brownout, watchdog, USB DTR re-toggling, ...)
 * every reset re-runs `pinMode(D8, OUTPUT); writeRelay(false)` and then the
 * controller commands the relay back ON about a second later. That single
 * transition every reset is enough to make the coil click — and by ear, a
 * bootloop is indistinguishable from real time-proportional cycling.
 *
 * To distinguish, we keep a boot counter in a `.noinit` RAM cell. On a hard
 * power-on / brownout the SRAM is uninitialised — our magic word almost
 * certainly does NOT match, so the counter resets to 1. On a soft reset
 * (watchdog or external reset pulse) the SRAM contents survive, the magic
 * word matches, and we increment. We also snapshot MCUSR (best-effort:
 * main.cpp's wdt_disable() clears WDRF before we get here, but PORF /
 * EXTRF / BORF survive on AVR-libc through wdt_disable).
 */
#define LAB5_2_BOOT_MAGIC 0xC0DEu

static volatile uint16_t s_bootCount __attribute__((section(".noinit")));
static volatile uint16_t s_bootMagic __attribute__((section(".noinit")));
static volatile uint8_t  s_mcusrAtBoot;

static void diagCaptureResetState(void)
{
    s_mcusrAtBoot = MCUSR;
    MCUSR = 0u;

    if (s_bootMagic != LAB5_2_BOOT_MAGIC)
    {
        s_bootCount = 1u;
        s_bootMagic = LAB5_2_BOOT_MAGIC;
    }
    else
    {
        s_bootCount = (uint16_t)(s_bootCount + 1u);
    }
}

static void diagPrintBootBanner(void)
{
    Serial.print(F("[L5.2][BOOT] #"));
    Serial.print(s_bootCount);
    Serial.print(F(" MCUSR=0x"));
    if (s_mcusrAtBoot < 0x10) { Serial.print('0'); }
    Serial.print(s_mcusrAtBoot, HEX);
    Serial.print(F(" ("));
    bool any = false;
    if (s_mcusrAtBoot & (1 << PORF))  { Serial.print(F("POR "));  any = true; }
    if (s_mcusrAtBoot & (1 << EXTRF)) { Serial.print(F("EXT "));  any = true; }
    if (s_mcusrAtBoot & (1 << BORF))  { Serial.print(F("BOR! ")); any = true; }
    if (s_mcusrAtBoot & (1 << WDRF))  { Serial.print(F("WDT! ")); any = true; }
    if (!any) { Serial.print(F("cleared-by-bootloader")); }
    Serial.println(F(")"));

    if (s_bootCount > 1u)
    {
        Serial.print(F("[L5.2][BOOT] reset #"));
        Serial.print(s_bootCount);
        Serial.println(F(" since last cold start — possible reset loop:"));
        Serial.println(F("   EXT every few s = USB cable / port flaky, or DTR re-toggling"));
        Serial.println(F("   BOR             = supply rail dipping below 4.3 V"));
        Serial.println(F("   WDT             = a task ran >15 ms with interrupts off"));
        Serial.println(F("   POR with #>1    = SRAM magic was random by luck; safe to ignore"));
    }
}

static void taskAcquisition(void* pv);
static void taskControl(void* pv);
static void taskActuation(void* pv);
static void taskDisplay(void* pv);
static void taskCommand(void* pv);

// ============================================================================
// Helpers
// ============================================================================

static float clampf(float value, float minV, float maxV)
{
    if (value < minV) { return minV; }
    if (value > maxV) { return maxV; }
    return value;
}

/** Drive the relay coil according to compile-time polarity. */
static inline void writeRelay(bool on)
{
#if LAB5_2_RELAY_ACTIVE_LOW
    digitalWrite(LAB5_2_HEAT_RELAY_PIN, on ? LOW : HIGH);
#else
    digitalWrite(LAB5_2_HEAT_RELAY_PIN, on ? HIGH : LOW);
#endif
}

static bool iequals(const char* a, const char* b)
{
    while ((*a != '\0') && (*b != '\0'))
    {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
        {
            return false;
        }
        ++a;
        ++b;
    }
    return (*a == '\0') && (*b == '\0');
}

static void loadDefaults(Lab52Config* cfg)
{
    cfg->setpointC      = LAB5_2_DEFAULT_SETPOINT_C;
    cfg->hysteresisC    = LAB5_2_DEFAULT_HYST_C;
    cfg->kp             = LAB5_2_DEFAULT_KP;
    cfg->ki             = LAB5_2_DEFAULT_KI;
    cfg->kd             = LAB5_2_DEFAULT_KD;
    cfg->outputLimit    = LAB5_2_DEFAULT_OUTPUT_LIMIT;
    cfg->heatDutyPct    = LAB5_2_DEFAULT_HEAT_PCT;
    cfg->relayWindowMs  = LAB5_2_DEFAULT_RELAY_WINDOW_MS;

    cfg->acqTaskMs   = LAB5_2_ACQ_TASK_MS;
    cfg->ctrlTaskMs  = LAB5_2_CTRL_TASK_MS;
    cfg->actTaskMs   = LAB5_2_ACT_TASK_MS;
    cfg->dispTaskMs  = LAB5_2_DISP_TASK_MS;
    cfg->cmdTaskMs   = LAB5_2_CMD_TASK_MS;
}

// ============================================================================
// LCD rendering
// ============================================================================

static void lcdRender(const Lab52RuntimeState* state)
{
    const unsigned long nowMs = millis();
    const bool showBanner = (state->lcdBannerUntilMs != 0UL) && (nowMs < state->lcdBannerUntilMs);

    s_lcd.clear();
    s_lcd.setCursor(0, 0);
    if (showBanner)
    {
        s_lcd.print(state->lcdBannerL1);
        s_lcd.setCursor(0, 1);
        s_lcd.print(state->lcdBannerL2);
        return;
    }

    char line1[17] = {0};
    char line2[17] = {0};

    /* Line 1: "SP:28C PV:24C R+" or "SP:28C PV:--C R-" when sensor invalid. */
    if (state->sensorValid)
    {
        snprintf(line1,
                 sizeof(line1),
                 "SP:%2dC PV:%2dC R%c",
                 (int)(state->setpointC + 0.5f),
                 (int)(state->tempC + 0.5f),
                 state->relayOn ? '+' : '-');
    }
    else
    {
        snprintf(line1,
                 sizeof(line1),
                 "SP:%2dC PV:--C R%c",
                 (int)(state->setpointC + 0.5f),
                 state->relayOn ? '+' : '-');
    }

    /* Line 2 mode prefix: PID / ONF / FRC (forced) */
    const char* modeStr;
    if (state->forceMode != LAB5_2_FORCE_AUTO)
    {
        modeStr = "FRC";
    }
    else if (state->mode == LAB5_2_MODE_PID)
    {
        modeStr = "PID";
    }
    else
    {
        modeStr = "ONF";
    }

    if (state->forceMode != LAB5_2_FORCE_AUTO)
    {
        snprintf(line2,
                 sizeof(line2),
                 "FRC relay:%s",
                 (state->forceMode == LAB5_2_FORCE_ON) ? "ON" : "OFF");
    }
    else if (state->mode == LAB5_2_MODE_PID)
    {
        snprintf(line2,
                 sizeof(line2),
                 "%s H:%3d%% e%+3d",
                 modeStr,
                 (int)(state->heatPct + 0.5f),
                 (int)(state->errorC + (state->errorC >= 0 ? 0.5f : -0.5f)));
    }
    else
    {
        snprintf(line2,
                 sizeof(line2),
                 "%s H:%-3s  e%+3d",
                 modeStr,
                 state->onoffHeating ? "ON" : "OFF",
                 (int)(state->errorC + (state->errorC >= 0 ? 0.5f : -0.5f)));
    }

    s_lcd.print(line1);
    s_lcd.setCursor(0, 1);
    s_lcd.print(line2);
}

// ============================================================================
// Serial banner / help
// ============================================================================

/** Full command reference on Serial (boot + help). Holds ioMutex to avoid interleaving. */
static void printCommandsSerial(void)
{
    if (g_lab52.ioMutex == NULL)
    {
        return;
    }
    if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(200)) != pdTRUE)
    {
        return;
    }
    Serial.println(F("========== Lab 5.2 — heater + relay closed loop =========="));
    Serial.print  (F("Build: "));
    Serial.print  (F(__DATE__));
    Serial.print  (F(" "));
    Serial.print  (F(__TIME__));
    Serial.print  (F("  (sensor=DHT"));
    Serial.print  (LAB5_2_DHT_TYPE);
    Serial.println(F(", relay-on-D8, refresh=2200ms, verbose DHT trace)"));
    Serial.println(F("Plant: DHT air temperature, 3 resistors in parallel as heater,"));
    Serial.println(F("       switched by a 5 V relay on pin 8 (active LOW). Cooling is passive."));
    Serial.println(F(""));
    Serial.println(F("Commands (each accepted line is echoed back):"));
    Serial.println(F("  <number>              shortcut: bare integer = new set-point degC"));
    Serial.println(F("  mode pid              continuous PID -> time-proportional relay"));
    Serial.println(F("  mode onoff            bang-bang with hysteresis"));
    Serial.println(F("  set <C>               set-point degC (15..45)"));
    Serial.println(F("  hyst <C>              half hysteresis band (0.5..5), ON-OFF only"));
    Serial.println(F("  kp <v>                proportional gain (0..50)"));
    Serial.println(F("  ki <v>                integral gain (0..10)"));
    Serial.println(F("  kd <v>                derivative gain (0..10)"));
    Serial.println(F("  heat <0..100>         max heater duty cap (PID only)"));
    Serial.println(F("  window <ms>           relay time-prop window (500..10000), PID only"));
    Serial.println(F("  preset <name>         soft | balanced | aggressive | p | pd"));
    Serial.println(F("  relay <on|off|auto>   manual override (HW bring-up; bypasses controller)"));
    Serial.println(F("  dhtprobe              read sensor as DHT11 AND DHT22, prints both"));
    Serial.println(F("  status                snapshot on LCD for a few seconds"));
    Serial.println(F("  help                  print this list"));
    Serial.println(F("  HW: DHT11=2, relay IN=8 (active LOW), LCD I2C=20/21"));
    Serial.println(F("  --- Set Serial Monitor to 115200 baud ---"));
    Serial.println(F("=========================================================="));
    xSemaphoreGive(g_lab52.ioMutex);
}

static void showStatusOnLcd(void)
{
    if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        snprintf(g_lab52.state.lcdBannerL1,
                 sizeof(g_lab52.state.lcdBannerL1),
                 "SP:%2dC PV:%2dC",
                 (int)(g_lab52.state.setpointC + 0.5f),
                 (int)(g_lab52.state.tempC + 0.5f));
        snprintf(g_lab52.state.lcdBannerL2,
                 sizeof(g_lab52.state.lcdBannerL2),
                 "%-3s H:%3d%%  R%c",
                 (g_lab52.state.mode == LAB5_2_MODE_PID) ? "PID" : "ONF",
                 (int)(g_lab52.state.heatPct + 0.5f),
                 g_lab52.state.relayOn ? '+' : '-');
        g_lab52.state.lcdBannerUntilMs = millis() + 8000UL;
        xSemaphoreGive(g_lab52.stateMutex);
    }
}

static void showHelpOnLcd(void)
{
    if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        strncpy(g_lab52.state.lcdBannerL1, "mode set hyst kp", sizeof(g_lab52.state.lcdBannerL1) - 1);
        g_lab52.state.lcdBannerL1[sizeof(g_lab52.state.lcdBannerL1) - 1] = '\0';
        strncpy(g_lab52.state.lcdBannerL2, "ki kd preset hlp", sizeof(g_lab52.state.lcdBannerL2) - 1);
        g_lab52.state.lcdBannerL2[sizeof(g_lab52.state.lcdBannerL2) - 1] = '\0';
        g_lab52.state.lcdBannerUntilMs = millis() + 10000UL;
        xSemaphoreGive(g_lab52.stateMutex);
    }
}

// ============================================================================
// PID presets
// ============================================================================

typedef struct
{
    const char* name;
    float kp;
    float ki;
    float kd;
} PidPreset;

static const PidPreset PID_PRESETS[] PROGMEM = {
    /* Small relay-driven heater (~1 W) + DHT11, 250 ms loop, 2 s window. */
    { "soft",       10.0f, 0.20f,  5.0f },
    { "balanced",   20.0f, 0.40f,  8.0f },
    { "aggressive", 30.0f, 1.00f, 10.0f },
    { "p",          25.0f, 0.0f,   0.0f },   /* pure P — shows steady-state offset */
    { "pd",         25.0f, 0.0f,  12.0f }    /* PD — no offset elimination either  */
};
static const uint8_t PID_PRESET_COUNT = sizeof(PID_PRESETS) / sizeof(PID_PRESETS[0]);

static bool applyPreset(const char* name)
{
    for (uint8_t i = 0; i < PID_PRESET_COUNT; ++i)
    {
        const char* presetName = (const char*)pgm_read_ptr(&PID_PRESETS[i].name);
        if (iequals(name, presetName))
        {
            const float kp = pgm_read_float(&PID_PRESETS[i].kp);
            const float ki = pgm_read_float(&PID_PRESETS[i].ki);
            const float kd = pgm_read_float(&PID_PRESETS[i].kd);
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.config.kp = kp;
                g_lab52.config.ki = ki;
                g_lab52.config.kd = kd;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            s_pid.Reset(0.0f);
            return true;
        }
    }
    return false;
}

// ============================================================================
// Command processing  (sscanf for parsing — printf-family is the right tool here)
// ============================================================================

/**
 * Apply a new set-point under the state mutex. Centralised so both the
 * `set <C>` command and the bare-number shortcut go through the same path.
 *
 * Returns true if the value was inside the [15..45] band before clamping.
 */
static bool applySetpoint(float requested, float* outClamped)
{
    const float clamped = clampf(requested, 15.0f, 45.0f);
    if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        g_lab52.config.setpointC = clamped;
        g_lab52.state.setpointC  = clamped;
        xSemaphoreGive(g_lab52.stateMutex);
    }
    if (outClamped != NULL) { *outClamped = clamped; }
    return (requested >= 15.0f) && (requested <= 45.0f);
}

/**
 * True if `text` starts with an optional sign followed by at least one digit
 * and contains nothing else (modulo trailing whitespace). Mirrors the bare-
 * integer setpoint shortcut used by app_lab_6_1_i_ctrl_setpoint_task in the
 * reference solution: typing `25` is equivalent to `set 25`.
 */
static bool isBareInteger(const char* text)
{
    if (text == NULL || *text == '\0') { return false; }
    if (*text == '+' || *text == '-')  { ++text; }
    if (!isdigit((unsigned char)*text)) { return false; }
    while (isdigit((unsigned char)*text)) { ++text; }
    while (*text == ' ' || *text == '\t') { ++text; }
    return *text == '\0';
}

static void processCommand(const char* line)
{
    char cmd[16] = {0};
    char arg[16] = {0};
    float fvalue = 0.0f;
    int   ivalue = 0;

    const int parsedTokens = sscanf(line, "%15s %15s", cmd, arg);
    if (parsedTokens < 1)
    {
        showHelpOnLcd();
        return;
    }

    /* Bare-integer setpoint shortcut. `25` is treated as `set 25`. */
    if (parsedTokens == 1 && isBareInteger(line))
    {
        const int requested = atoi(line);
        float clamped = 0.0f;
        const bool inRange = applySetpoint((float)requested, &clamped);
        if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            if (inRange)
            {
                Serial.print(F("setpoint=")); Serial.print((int)clamped); Serial.println(F("C"));
            }
            else
            {
                Serial.print(F("setpoint clamped to "));
                Serial.print((int)clamped); Serial.println(F("C (range 15..45)"));
            }
            xSemaphoreGive(g_lab52.ioMutex);
        }
        return;
    }

    /* String-arg commands first (mode, preset). */
    if (parsedTokens == 2 && iequals(cmd, "mode"))
    {
        if (iequals(arg, "onoff"))
        {
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.state.mode = LAB5_2_MODE_ONOFF;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            return;
        }
        if (iequals(arg, "pid"))
        {
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.state.mode = LAB5_2_MODE_PID;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            s_pid.Reset(0.0f);
            return;
        }
    }

    if (parsedTokens == 2 && iequals(cmd, "preset"))
    {
        if (applyPreset(arg))
        {
            return;
        }
        if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            Serial.println(F("(preset?) try: soft | balanced | aggressive | p | pd"));
            xSemaphoreGive(g_lab52.ioMutex);
        }
        return;
    }

    /* Manual relay override (hardware bring-up). Bypasses the entire controller. */
    if (parsedTokens == 2 && iequals(cmd, "relay"))
    {
        Lab52ForceMode m = LAB5_2_FORCE_AUTO;
        bool known = true;
        if      (iequals(arg, "on"))   { m = LAB5_2_FORCE_ON;  }
        else if (iequals(arg, "off"))  { m = LAB5_2_FORCE_OFF; }
        else if (iequals(arg, "auto")) { m = LAB5_2_FORCE_AUTO;}
        else                           { known = false; }

        if (known)
        {
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.state.forceMode = m;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            return;
        }

        if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            Serial.println(F("(relay?) try: relay on | relay off | relay auto"));
            xSemaphoreGive(g_lab52.ioMutex);
        }
        return;
    }

    /* Numeric-arg commands. Parse fresh because the first sscanf grabbed a string. */
    if (sscanf(line, "%15s %f", cmd, &fvalue) >= 2)
    {
        if (iequals(cmd, "set"))
        {
            (void)applySetpoint(fvalue, NULL);
            return;
        }
        if (iequals(cmd, "hyst"))
        {
            fvalue = clampf(fvalue, 0.5f, 5.0f);
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.config.hysteresisC = fvalue;
                g_lab52.state.hysteresisC  = fvalue;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            return;
        }
        if (iequals(cmd, "kp"))
        {
            fvalue = clampf(fvalue, 0.0f, 50.0f);
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.config.kp = fvalue;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            return;
        }
        if (iequals(cmd, "ki"))
        {
            fvalue = clampf(fvalue, 0.0f, 10.0f);
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.config.ki = fvalue;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            return;
        }
        if (iequals(cmd, "kd"))
        {
            fvalue = clampf(fvalue, 0.0f, 10.0f);
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.config.kd = fvalue;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            return;
        }
        if (iequals(cmd, "heat"))
        {
            fvalue = clampf(fvalue, 0.0f, 100.0f);
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.config.heatDutyPct = fvalue;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            return;
        }
    }

    if (sscanf(line, "%15s %d", cmd, &ivalue) >= 2)
    {
        if (iequals(cmd, "window"))
        {
            if (ivalue < 500)  { ivalue = 500; }
            if (ivalue > 10000){ ivalue = 10000; }
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.config.relayWindowMs = (uint16_t)ivalue;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            return;
        }
    }

    /* Argument-less commands. */
    if (iequals(cmd, "status"))
    {
        showStatusOnLcd();
        return;
    }
    if (iequals(cmd, "dhtprobe"))
    {
        /* Decode the same physical sensor as both DHT11 and DHT22. The reading
         * that lands on plausible numbers (positive T, sane H) reveals the
         * actual hardware. Two bus transactions ~2.2 s apart. */
        DHT probe11(LAB5_2_DHT_PIN, DHT11);
        DHT probe22(LAB5_2_DHT_PIN, DHT22);
        probe11.begin();
        probe22.begin();

        const float t11 = probe11.readTemperature(false, true);
        const float h11 = probe11.readHumidity(true);
        vTaskDelay(pdMS_TO_TICKS(2300));
        const float t22 = probe22.readTemperature(false, true);
        const float h22 = probe22.readHumidity(true);

        if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            Serial.println(F("[dhtprobe] same sensor decoded both ways:"));
            Serial.print  (F("  as DHT11: T=")); Serial.print(t11, 1); Serial.print(F("C  H=")); Serial.print(h11, 1); Serial.println(F("%"));
            Serial.print  (F("  as DHT22: T=")); Serial.print(t22, 1); Serial.print(F("C  H=")); Serial.print(h22, 1); Serial.println(F("%"));
            Serial.print  (F("  configured: DHT")); Serial.println(LAB5_2_DHT_TYPE);
            Serial.println(F("  Pick the row whose T matches the room. If it is not the"));
            Serial.println(F("  configured one, change LAB5_2_DHT_TYPE in Lab5_2_Shared.h."));
            xSemaphoreGive(g_lab52.ioMutex);
        }
        return;
    }
    if (iequals(cmd, "help"))
    {
        printCommandsSerial();
        showHelpOnLcd();
        return;
    }

    /* Unknown — flash help banner and tell the user exactly what we saw,
     * so they can spot rogue characters from arrow-key history etc. */
    showHelpOnLcd();
    if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(80)) == pdTRUE)
    {
        Serial.print(F("(unknown) cmd='"));
        Serial.print(cmd);
        if (parsedTokens >= 2)
        {
            Serial.print(F("' arg='"));
            Serial.print(arg);
        }
        Serial.println(F("' — type `help`"));
        xSemaphoreGive(g_lab52.ioMutex);
    }
}

// ============================================================================
// Setup
// ============================================================================

void lab5_2_setup()
{
    diagCaptureResetState();
    diagPrintBootBanner();

    loadDefaults(&g_lab52.config);

    g_lab52.stateMutex = xSemaphoreCreateMutex();
    g_lab52.ioMutex    = xSemaphoreCreateMutex();
    g_lab52.qControl   = xQueueCreate(1, sizeof(Lab52ControlOutput));

    if ((g_lab52.stateMutex == NULL) ||
        (g_lab52.ioMutex == NULL) ||
        (g_lab52.qControl == NULL))
    {
        Serial.println(F("[lab5_2][FATAL] FreeRTOS object allocation failed"));
        for (;;) {}
    }

    /* Initial runtime state */
    g_lab52.state.tempC            = 0.0f;
    g_lab52.state.humidityPct      = 0.0f;
    g_lab52.state.sensorValid      = false;
    g_lab52.state.setpointC        = g_lab52.config.setpointC;
    g_lab52.state.hysteresisC      = g_lab52.config.hysteresisC;
    g_lab52.state.errorC           = 0.0f;
    g_lab52.state.pidOutput        = 0.0f;
    g_lab52.state.heatPct          = 0.0f;
    g_lab52.state.mode             = LAB5_2_MODE_PID;
    g_lab52.state.relayOn          = false;
    g_lab52.state.onoffHeating     = false;
    g_lab52.state.forceMode        = LAB5_2_FORCE_AUTO;
    g_lab52.state.lastSampleMs     = 0UL;
    g_lab52.state.controlCycles    = 0UL;
    g_lab52.state.commandCounter   = 0UL;
    g_lab52.state.lcdBannerL1[0]   = '\0';
    g_lab52.state.lcdBannerL2[0]   = '\0';
    g_lab52.state.lcdBannerUntilMs = 0UL;

    /* Heater relay: drive OFF before anything else can swing it. */
    pinMode(LAB5_2_HEAT_RELAY_PIN, OUTPUT);
    writeRelay(false);

    Wire.begin();
    s_lcd.init();
    s_lcd.backlight();
    s_lcd.clear();
    s_lcd.setCursor(0, 0);
    s_lcd.print(F("Lab5.2 heater"));
    s_lcd.setCursor(0, 1);
    s_lcd.print(F("PID + relay"));

    s_dht.Init();
    s_onoff.Init(false);
    s_pid.Init(g_lab52.config.kp,
               g_lab52.config.ki,
               g_lab52.config.kd,
               g_lab52.config.outputLimit,
               0.0f);

    BaseType_t ok;
    ok = xTaskCreate(taskAcquisition, "L52_ACQ",  480, NULL, 3, NULL);
    if (ok != pdPASS) { Serial.println(F("[lab5_2][FATAL] ACQ task")); for (;;) {} }

    ok = xTaskCreate(taskControl,     "L52_CTRL", 512, NULL, 3, NULL);
    if (ok != pdPASS) { Serial.println(F("[lab5_2][FATAL] CTRL task")); for (;;) {} }

    ok = xTaskCreate(taskActuation,   "L52_ACT",  256, NULL, 2, NULL);
    if (ok != pdPASS) { Serial.println(F("[lab5_2][FATAL] ACT task")); for (;;) {} }

    ok = xTaskCreate(taskDisplay,     "L52_DISP", 640, NULL, 1, NULL);
    if (ok != pdPASS) { Serial.println(F("[lab5_2][FATAL] DISP task")); for (;;) {} }

    ok = xTaskCreate(taskCommand,     "L52_CMD",  768, NULL, 1, NULL);
    if (ok != pdPASS) { Serial.println(F("[lab5_2][FATAL] CMD task")); for (;;) {} }

    printCommandsSerial();
    showHelpOnLcd();
}

void lab5_2_loop()
{
    /* Empty: scheduler runs everything. */
}

// ============================================================================
// Acquisition task — DHT
// ============================================================================

/**
 * Median of 5 floats (in-place, ~30 comparisons). Used to reject a single
 * bad reading without smearing the dynamic response — the same role the
 * `lib_cond_median_filter` plays in the reference solution's
 * `dd_sns_temperature_loop()` pipeline (saturate → median → weighted avg).
 *
 * Median is preferable to plain averaging for DHT data because the
 * pathological readings tend to be one-bit decode glitches that produce
 * a single outlier — the median throws those away cleanly while a mean
 * would propagate half their amplitude into the controller.
 */
static float median5(const float* samples)
{
    float a[5];
    for (uint8_t i = 0; i < 5; ++i) { a[i] = samples[i]; }
    /* Selection sort up to index 2 — enough to find the median. */
    for (uint8_t i = 0; i <= 2; ++i)
    {
        uint8_t minIdx = i;
        for (uint8_t j = i + 1; j < 5; ++j)
        {
            if (a[j] < a[minIdx]) { minIdx = j; }
        }
        if (minIdx != i)
        {
            const float tmp = a[i];
            a[i] = a[minIdx];
            a[minIdx] = tmp;
        }
    }
    return a[2];
}

static void taskAcquisition(void* pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();
    unsigned long lastReadMs = 0UL;
    uint16_t consecFails = 0u;

    /* Rolling 5-tap window for median filtering. seeded = how many samples
     * we have actually taken; until it reaches 5, the controller sees the
     * latest raw value (no filtering would over-attenuate the warm-up). */
    float window[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint8_t windowHead = 0u;
    uint8_t seeded = 0u;

    for (;;)
    {
        const unsigned long nowMs = millis();

        if ((nowMs - lastReadMs) >= LAB5_2_DHT_MIN_REFRESH_MS)
        {
            float tempC = 0.0f;
            float humidity = 0.0f;
            const bool valid = s_dht.Read(&tempC, &humidity);
            lastReadMs = nowMs;

            float reportedTemp = tempC;
            if (valid)
            {
                window[windowHead] = tempC;
                windowHead = (windowHead + 1u) % 5u;
                if (seeded < 5u) { seeded++; }
                reportedTemp = (seeded >= 5u) ? median5(window) : tempC;
            }

            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
            {
                if (valid)
                {
                    g_lab52.state.tempC        = reportedTemp;
                    g_lab52.state.humidityPct  = humidity;
                    g_lab52.state.sensorValid  = true;
                    g_lab52.state.lastSampleMs = nowMs;
                }
                else
                {
                    g_lab52.state.sensorValid = false;
                }
                xSemaphoreGive(g_lab52.stateMutex);
            }

            if (valid) { consecFails = 0u; }
            else if (consecFails < 0xFFFFu) { consecFails++; }

            /* Verbose, every-read trace. With reads every 2.2 s this is ~27 lines/min,
             * fine for diagnosis. Comment out the Serial.print()s below if it's too
             * chatty once the system is working. */
            if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                Serial.print(F("[L5.2][DHT @"));
                Serial.print(nowMs);
                Serial.print(F("ms] "));
                if (valid)
                {
                    Serial.print(F("OK  T="));
                    Serial.print(tempC, 1);
                    Serial.print(F("C (med="));
                    Serial.print(reportedTemp, 1);
                    Serial.print(F("C)  H="));
                    Serial.print(humidity, 0);
                    Serial.println(F("%"));
                }
                else
                {
                    Serial.print(F("FAIL (consecutive="));
                    Serial.print(consecFails);
                    Serial.println(F(") — check: sensor type (DHT11 vs DHT22), wiring (VCC/DATA/GND), pull-up (4.7-10kOhm DATA->VCC if bare chip), power"));
                }
                xSemaphoreGive(g_lab52.ioMutex);
            }
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab52.config.acqTaskMs));
    }
}

// ============================================================================
// Control task — chooses ON-OFF or PID branch, posts heat demand to the queue
// ============================================================================

static void taskControl(void* pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();
    unsigned long prevMs = millis();
    Lab52Mode prevMode   = LAB5_2_MODE_PID;

    for (;;)
    {
        /* Snapshot config + measurement under the mutex (kept short on purpose). */
        float setpoint   = g_lab52.config.setpointC;
        float hysteresis = g_lab52.config.hysteresisC;
        float kp         = g_lab52.config.kp;
        float ki         = g_lab52.config.ki;
        float kd         = g_lab52.config.kd;
        float outLimit   = g_lab52.config.outputLimit;
        float heatCap    = g_lab52.config.heatDutyPct;
        Lab52Mode mode   = LAB5_2_MODE_PID;
        float tempC      = 0.0f;
        bool  valid      = false;

        if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(25)) == pdTRUE)
        {
            setpoint   = g_lab52.config.setpointC;
            hysteresis = g_lab52.config.hysteresisC;
            kp         = g_lab52.config.kp;
            ki         = g_lab52.config.ki;
            kd         = g_lab52.config.kd;
            outLimit   = g_lab52.config.outputLimit;
            heatCap    = g_lab52.config.heatDutyPct;
            mode       = g_lab52.state.mode;
            tempC      = g_lab52.state.tempC;
            valid      = g_lab52.state.sensorValid;
            xSemaphoreGive(g_lab52.stateMutex);
        }

        s_pid.Configure(kp, ki, kd, outLimit);

        const unsigned long nowMs = millis();
        float dtSec = (float)(nowMs - prevMs) / 1000.0f;
        if (dtSec < 0.001f) { dtSec = 0.001f; }
        prevMs = nowMs;

        float errorC      = 0.0f;
        float pidOut      = 0.0f;
        float heatPct     = 0.0f;
        bool  onoffActive = false;

        if (valid)
        {
            errorC = setpoint - tempC;        /* positive ⇒ we are colder than SP ⇒ heat */

            if (mode == LAB5_2_MODE_ONOFF)
            {
                onoffActive = s_onoff.Step(tempC, setpoint, hysteresis);
                heatPct = onoffActive ? 100.0f : 0.0f;
            }
            else /* PID */
            {
                /* Reset accumulated state when freshly entering PID mode. */
                if (prevMode == LAB5_2_MODE_ONOFF)
                {
                    s_pid.Reset(0.0f);
                }

                pidOut  = s_pid.Step(errorC, dtSec);
                heatPct = clampf(pidOut, 0.0f, heatCap);
            }
        }
        else
        {
            /* Sensor lost: hold heater OFF (fail-safe). */
            heatPct = 0.0f;
            onoffActive = false;
        }

        /* Publish to actuator. */
        Lab52ControlOutput out;
        out.heatPct     = heatPct;
        out.pidOutput   = pidOut;
        out.errorC      = errorC;
        out.mode        = mode;
        out.valid       = valid;
        out.timestampMs = nowMs;
        xQueueOverwrite(g_lab52.qControl, &out);

        /* Mirror to shared state for the LCD task. */
        if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            g_lab52.state.errorC        = errorC;
            g_lab52.state.pidOutput     = pidOut;
            g_lab52.state.heatPct       = heatPct;
            g_lab52.state.onoffHeating  = onoffActive;
            g_lab52.state.controlCycles++;
            xSemaphoreGive(g_lab52.stateMutex);
        }

        prevMode = mode;

#if LAB5_2_SERIAL_PLOTTER
        /* Plotter: SetPoint, measured value, output % — must be one line per cycle. */
        if (valid && (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(4)) == pdTRUE))
        {
            Serial.print(F("SetPoint:"));
            Serial.print(setpoint, 2);
            Serial.print(F(",Value:"));
            Serial.print(tempC, 2);
            Serial.print(F(",Output:"));
            Serial.println(heatPct, 2);
            xSemaphoreGive(g_lab52.ioMutex);
        }
#endif

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab52.config.ctrlTaskMs));
    }
}

// ============================================================================
// Actuation task — time-proportional relay switching
// ============================================================================

static void taskActuation(void* pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    Lab52ControlOutput latest;
    latest.heatPct     = 0.0f;
    latest.pidOutput   = 0.0f;
    latest.errorC      = 0.0f;
    latest.mode        = LAB5_2_MODE_PID;
    latest.valid       = false;
    latest.timestampMs = millis();

    unsigned long windowStartMs = millis();

    /* Edge-triggered logging: every actual relay state change is announced
     * on Serial. If the user hears a click, there must be a corresponding
     * line. Conversely, audible clicks with NO log line ⇒ the firmware is
     * NOT toggling the pin and the click is purely electromechanical
     * (coil drop-out from rail droop, faulty contacts, bootloop, etc.). */
    bool prevRelayOn = false;
    bool prevValid   = true;          /* first invalid will print one line */
    unsigned long lastEdgeMs = millis();
    uint16_t edgeCount = 0u;

    for (;;)
    {
        Lab52ControlOutput incoming;
        if (xQueueReceive(g_lab52.qControl, &incoming, 0) == pdTRUE)
        {
            latest = incoming;
        }

        /* Snapshot the manual override flag (set by the `relay` serial command). */
        Lab52ForceMode forceMode = LAB5_2_FORCE_AUTO;
        if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            forceMode = g_lab52.state.forceMode;
            xSemaphoreGive(g_lab52.stateMutex);
        }

        const float heatPct = clampf(latest.heatPct, 0.0f, 100.0f);

        const unsigned long nowMs = millis();
        const unsigned long windowMs = (g_lab52.config.relayWindowMs == 0) ?
                                       LAB5_2_DEFAULT_RELAY_WINDOW_MS :
                                       g_lab52.config.relayWindowMs;
        if ((nowMs - windowStartMs) >= windowMs)
        {
            windowStartMs += windowMs;
            /* Resync if we drifted more than a full window (e.g. after a long delay). */
            if ((nowMs - windowStartMs) >= windowMs)
            {
                windowStartMs = nowMs;
            }
        }

        const unsigned long elapsed = nowMs - windowStartMs;
        const unsigned long onMs = (unsigned long)((heatPct / 100.0f) * (float)windowMs + 0.5f);

        bool wantRelayOn;
        if (forceMode == LAB5_2_FORCE_ON)
        {
            wantRelayOn = true;     /* manual: relay closed (heater on) */
        }
        else if (forceMode == LAB5_2_FORCE_OFF)
        {
            wantRelayOn = false;    /* manual: relay open (heater off) */
        }
        else
        {
            wantRelayOn = (heatPct >= LAB5_2_PID_MIN_PCT) && (elapsed < onMs);
        }

        writeRelay(wantRelayOn);

        /* Log every actual transition. The dwell-time printed alongside is
         * how long the previous state was held — a long dwell means the
         * firmware was steady and any clicks during that dwell are NOT our
         * doing. A short dwell (< 200 ms) means we are commanding chatter. */
        if (wantRelayOn != prevRelayOn || latest.valid != prevValid)
        {
            const unsigned long dwellMs = nowMs - lastEdgeMs;
            edgeCount++;
            if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(20)) == pdTRUE)
            {
                Serial.print(F("[L5.2][RLY @"));
                Serial.print(nowMs);
                Serial.print(F("ms] "));
                Serial.print(wantRelayOn ? F("ON ") : F("OFF"));
                Serial.print(F("  prev_dwell="));
                Serial.print(dwellMs);
                Serial.print(F("ms  edges="));
                Serial.print(edgeCount);
                Serial.print(F("  heat="));
                Serial.print((int)(heatPct + 0.5f));
                Serial.print(F("%  valid="));
                Serial.print(latest.valid ? '1' : '0');
                Serial.print(F("  force="));
                Serial.print((forceMode == LAB5_2_FORCE_AUTO) ? F("auto")
                            : (forceMode == LAB5_2_FORCE_ON) ? F("ON")
                            : F("OFF"));
                Serial.println();
                xSemaphoreGive(g_lab52.ioMutex);
            }
            prevRelayOn = wantRelayOn;
            prevValid   = latest.valid;
            lastEdgeMs  = nowMs;
        }

        if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            g_lab52.state.relayOn = wantRelayOn;
            xSemaphoreGive(g_lab52.stateMutex);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab52.config.actTaskMs));
    }
}

// ============================================================================
// Display task — refresh LCD
// ============================================================================

static void taskDisplay(void* pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            lcdRender(&g_lab52.state);
            xSemaphoreGive(g_lab52.stateMutex);
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab52.config.dispTaskMs));
    }
}

// ============================================================================
// Command task — collect a line, then sscanf-parse via processCommand()
// ============================================================================

/*
 * Robust line collector.
 *
 * Many Serial Monitors do unhelpful things to the byte stream we receive:
 *   - Up/Down/Left/Right arrow keys send ANSI CSI escapes ("ESC [ A" etc.)
 *     which were the actual reason `mode onoff` was rejected after pressing
 *     Up-Arrow in earlier sessions: the line arrived as "\x1b[Amode onoff",
 *     sscanf tokenised "\x1b[Amode" as the verb, no match, silently dropped.
 *   - Pasted text from web pages can carry a UTF-8 BOM or NBSP bytes.
 *   - Some terminals emit DEL (0x7F) for backspace, others 0x08.
 *
 * We strip all of that here so the parser only sees printable ASCII plus
 * spaces. We also keep buffering across an overflow so a long line is
 * dropped in one piece (with a clear message) rather than silently chopped.
 */
typedef enum
{
    LINE_FILTER_NORMAL = 0,
    LINE_FILTER_AFTER_ESC,        /* saw ESC, waiting for '[' or one-char seq */
    LINE_FILTER_IN_CSI            /* inside ESC [ ... <final> */
} LineFilterState;

static void taskCommand(void* pv)
{
    (void)pv;
    char cmdLine[48] = {0};
    uint8_t cmdLen = 0u;
    bool dropping = false;        /* true = swallow until next \r or \n */
    LineFilterState filter = LINE_FILTER_NORMAL;

    for (;;)
    {
        while (Serial.available() > 0)
        {
            const uint8_t raw = (uint8_t)Serial.read();

            /* --- ANSI CSI escape filter. Drop "ESC [ ... <final>". --- */
            if (filter == LINE_FILTER_AFTER_ESC)
            {
                filter = (raw == '[') ? LINE_FILTER_IN_CSI : LINE_FILTER_NORMAL;
                continue;
            }
            if (filter == LINE_FILTER_IN_CSI)
            {
                /* CSI final byte is in the range 0x40..0x7E. */
                if (raw >= 0x40 && raw <= 0x7E) { filter = LINE_FILTER_NORMAL; }
                continue;
            }
            if (raw == 0x1B) { filter = LINE_FILTER_AFTER_ESC; continue; }

            const char ch = (char)raw;

            /* End-of-line: process whatever we have. */
            if ((ch == '\r') || (ch == '\n'))
            {
                if (dropping)
                {
                    if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
                    {
                        Serial.println(F("(input too long — line discarded; max 47 chars)"));
                        xSemaphoreGive(g_lab52.ioMutex);
                    }
                    cmdLen = 0u;
                    dropping = false;
                    continue;
                }

                if (cmdLen == 0u) { continue; }

                /* Trim trailing whitespace. */
                while (cmdLen > 0u && (cmdLine[cmdLen - 1u] == ' ' ||
                                       cmdLine[cmdLen - 1u] == '\t'))
                {
                    cmdLen--;
                }
                cmdLine[cmdLen] = '\0';

                /* Find first non-whitespace char (leading-space trim). */
                const char* trimmed = cmdLine;
                while (*trimmed == ' ' || *trimmed == '\t') { ++trimmed; }
                if (*trimmed == '\0') { cmdLen = 0u; continue; }

                processCommand(trimmed);

                if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
                {
                    Serial.print(F("> "));
                    Serial.println(trimmed);
                    xSemaphoreGive(g_lab52.ioMutex);
                }
                cmdLen = 0u;
                if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
                {
                    g_lab52.state.commandCounter++;
                    xSemaphoreGive(g_lab52.stateMutex);
                }
                continue;
            }

            /* Backspace / DEL — drop the previous char. */
            if (ch == 0x08 || ch == 0x7F)
            {
                if (cmdLen > 0u) { cmdLen--; }
                continue;
            }

            /* Drop other control codes and any 8-bit byte. We accept only
             * printable ASCII (0x20..0x7E) plus tab. Tabs are normalised to
             * spaces so sscanf tokenises them like the user expects. */
            const bool printable = (raw >= 0x20 && raw <= 0x7E);
            if (!printable && ch != '\t') { continue; }
            const char effective = (ch == '\t') ? ' ' : ch;

            /* Skip leading spaces silently — keeps `   set 25` clean. */
            if (cmdLen == 0u && effective == ' ') { continue; }

            if (dropping) { continue; }

            if ((cmdLen + 1u) < (uint8_t)sizeof(cmdLine))
            {
                cmdLine[cmdLen++] = effective;
            }
            else
            {
                /* Overflow: keep dropping until end-of-line, then report. */
                dropping = true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(g_lab52.config.cmdTaskMs));
    }
}
