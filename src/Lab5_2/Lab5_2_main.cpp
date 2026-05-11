/*
 * Lab 5.2 — Closed-loop air-temperature control with a relay-driven fan.
 *
 * Combined APP layer that fuses the two reference labs from the
 * methodological guide (`SI lab 5.1 (6.1 indrumar)` + `SI lab 6.2 indrumar
 * (5.2)`) into a SINGLE binary. The control law is selectable at runtime
 * over the Serial command line:
 *
 *   mode onoff   : ON-OFF with hysteresis (Indrumar §2.4.5 / §2.7.1)
 *   mode pid     : discrete PID with anti-windup (Indrumar §2.4.4 / §2.7.1)
 *
 * Architecture: APP -> SRV(controllers + sensor + actuator) -> ECAL
 *               (Dallas / OneWire / Wire) -> MCAL (Arduino core) -> HW
 *
 * --- FreeRTOS tasks ------------------------------------------------------
 *   acq    1000 ms  : DS18B20 read + saturate → median → weighted average
 *   ctrl    100 ms  : compute duty (ON-OFF or PID), post to queue
 *   fan     100 ms  : time-proportional relay slicing (TPC) from duty
 *   disp    500 ms  : refresh I²C LCD with PV / SP / mode / duty / relay
 *   cmd      80 ms  : parse Serial commands, update config under mutex
 *   report 1000 ms  : human-readable status OR Serial Plotter CSV stream
 *
 * --- Serial command grammar (each accepted line is echoed back) -----------
 *   <number>            shortcut: bare integer = new set-point degC
 *   mode pid            switch to PID controller (TPC duty modulation)
 *   mode onoff          switch to ON-OFF controller (binary relay)
 *   set <C>             set-point degC (saturated to 15..35)
 *   hyst <C>            half hysteresis band (0.5..5), ON-OFF only
 *   kp <v>              proportional gain (0..200)
 *   ki <v>              integral gain (0..50)
 *   kd <v>              derivative gain (0..50)
 *   preset <name>       soft | balanced | aggressive | p
 *   window <ms>         TPC window length (500..10000 ms; default 2000)
 *   polarity <low|high> relay-board polarity (default: low — Wokwi default)
 *   force <on|off|auto> manual override (HW bring-up; bypasses controller)
 *   plotter <on|off>    enable Serial Plotter CSV stream
 *   status              snapshot on LCD for a few seconds
 *   help                print this list
 *
 * --- Style notes ----------------------------------------------------------
 *   - Set Serial Monitor to 115200 baud (matches SerialStdioInit in main.cpp).
 *   - LCD line layout: line 1 = "T:25C SP:25C", line 2 = "PID 067% R+ e+2".
 *     R+ = relay closed (motor on), R- = relay open (motor off).
 *   - All shared state lives behind g_lab52.stateMutex; tasks read a snapshot
 *     and write under the mutex.
 */

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "Lab5_2_main.h"
#include "Lab5_2_Shared.h"
#include "ctrl_onoff_hyst.h"
#include "ctrl_pid.h"
#include "srv_temp_sensor.h"
#include "srv_fan.h"

#if defined(__AVR_ATmega328P__)
#error "Lab5_2 requires Arduino Mega 2560 (ATmega2560). Update board in platformio.ini and wiring."
#endif

// ============================================================================
// Globals
// ============================================================================

Lab52Shared g_lab52;

static PidController52              s_pid;
static OnOffHysteresisController52  s_onoff;
static LiquidCrystal_I2C            s_lcd(LAB5_2_LCD_I2C_ADDR, 16, 2);

/* --- Reset / boot diagnostics ------------------------------------------- *
 * If the MCU reset-loops (brownout, watchdog, USB DTR re-toggling) the
 * relay-style audible click of the previous build was indistinguishable
 * from real time-proportional cycling. The cooling-fan build doesn't have
 * that failure mode but the diagnostic itself is useful — we keep it.
 *
 * On a hard power-on / brownout, SRAM is uninitialised and the magic word
 * almost never matches → we reset the counter to 1. On a soft reset
 * (watchdog, EXT, scheduler crash with WDT enabled), the magic word
 * survives in the .noinit section and we increment.
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
    /* All I/O in this module goes through printf, which is hooked to the UART
     * by SerialStdioInit(...). Same convention as Lab 4.2 and Lab 7.            */
    printf("[L5.2][BOOT] #%u MCUSR=0x%02X (", (unsigned)s_bootCount, (unsigned)s_mcusrAtBoot);
    bool any = false;
    if (s_mcusrAtBoot & (1 << PORF))  { printf("POR ");  any = true; }
    if (s_mcusrAtBoot & (1 << EXTRF)) { printf("EXT ");  any = true; }
    if (s_mcusrAtBoot & (1 << BORF))  { printf("BOR! "); any = true; }
    if (s_mcusrAtBoot & (1 << WDRF))  { printf("WDT! "); any = true; }
    if (!any) { printf("cleared-by-bootloader"); }
    printf(")\n");

    if (s_bootCount > 1u)
    {
        printf("[L5.2][BOOT] reset #%u since cold start — possible loop:\n",
               (unsigned)s_bootCount);
        printf("   EXT every few s = USB cable / DTR flaky\n");
        printf("   BOR             = supply rail dipping below ~4.3 V\n");
        printf("   WDT             = a task hogged the CPU > timeout\n");
    }
}

// Forward decls
static void taskAcquisition(void* pv);
static void taskControl    (void* pv);
static void taskFan        (void* pv);
static void taskDisplay    (void* pv);
static void taskCommand    (void* pv);
static void taskReport     (void* pv);

// ============================================================================
// Helpers
// ============================================================================

static float clampf(float v, float lo, float hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

static int clampi(int v, int lo, int hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

/** Case-insensitive string equality (Arduino has no strcasecmp by default). */
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

    cfg->acqTaskMs      = LAB5_2_ACQ_TASK_MS;
    cfg->ctrlTaskMs     = LAB5_2_CTRL_TASK_MS;
    cfg->fanTaskMs      = LAB5_2_FAN_TASK_MS;
    cfg->dispTaskMs     = LAB5_2_DISP_TASK_MS;
    cfg->cmdTaskMs      = LAB5_2_CMD_TASK_MS;
    cfg->reportTaskMs   = LAB5_2_REPORT_TASK_MS;

    cfg->relayWindowMs  = LAB5_2_DEFAULT_RELAY_WINDOW_MS;
    cfg->relayActiveLow = (LAB5_2_RELAY_ACTIVE_LOW != 0);
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

    /* Line 1: "T:25C SP:25C   " or "T:--C SP:25C   " when the sensor failed. */
    if (state->sensorValid)
    {
        snprintf(line1, sizeof(line1),
                 "T:%2dC SP:%2dC",
                 (int)(state->tempC + 0.5f),
                 (int)(state->setpointC + 0.5f));
    }
    else
    {
        snprintf(line1, sizeof(line1),
                 "T:--C SP:%2dC",
                 (int)(state->setpointC + 0.5f));
    }

    /* Line 2: "PID 067% R+ e+2" / "ONF ON  R+ e+1" / "FRC ON   R+ "
     * R+ ⇒ relay closed (motor running), R- ⇒ relay open (motor stopped).
     * R is what you should compare against the LED on the relay module. */
    const int  errInt   = (int)(state->errorC + (state->errorC >= 0 ? 0.5f : -0.5f));
    const int  dutyInt  = (int)(state->fanPctActual + 0.5f);
    const char relayCh  = state->relayOn ? '+' : '-';

    if (state->forceMode != LAB5_2_FORCE_AUTO)
    {
        snprintf(line2, sizeof(line2),
                 "FRC %-3s R%c    ",
                 (state->forceMode == LAB5_2_FORCE_ON) ? "ON" : "OFF",
                 relayCh);
    }
    else if (state->mode == LAB5_2_MODE_PID)
    {
        snprintf(line2, sizeof(line2),
                 "PID %3d%% R%c e%+1d",
                 dutyInt, relayCh,
                 (errInt > 9) ? 9 : ((errInt < -9) ? -9 : errInt));
    }
    else
    {
        snprintf(line2, sizeof(line2),
                 "ONF %-3s  R%c e%+1d",
                 (dutyInt > 0) ? "ON" : "OFF", relayCh,
                 (errInt > 9) ? 9 : ((errInt < -9) ? -9 : errInt));
    }

    s_lcd.print(line1);
    s_lcd.setCursor(0, 1);
    s_lcd.print(line2);
}

// ============================================================================
// Serial banner / help
// ============================================================================

/** Boot/help banner. Holds ioMutex to avoid interleaving with other tasks. */
static void printCommandsSerial(void)
{
    if (g_lab52.ioMutex == NULL) { return; }
    if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(200)) != pdTRUE) { return; }

    printf("========== Lab 5.2 — DS18B20 + relay-driven fan ==========\n");
    printf("Build: %s %s\n", __DATE__, __TIME__);
    printf("Plant: DS18B20 (D5, 4.7k pull-up) + 5V relay on D8 => DC motor / fan\n");
    printf("Modes: ON-OFF with hysteresis | discrete PID with anti-windup\n");
    printf("Actuation: BINARY (ON-OFF) or TIME-PROPORTIONAL (PID, default 2 s window)\n");
    printf("Polarity: relay assumed ACTIVE-LOW (Wokwi default). Use `polarity high` if your\n");
    printf("          board is wired the other way and the motor runs when it should not.\n");
    printf("\n");
    printf("Commands (each accepted line is echoed back):\n");
    printf("  <number>             shortcut: bare integer = new set-point degC\n");
    printf("  mode pid             continuous PID via TPC (Indrumar Lab 6.2)\n");
    printf("  mode onoff           bang-bang with hysteresis (Indrumar Lab 6.1)\n");
    printf("  set <C>              set-point degC (15..35)\n");
    printf("  hyst <C>             half-band 0.5..5, ON-OFF only\n");
    printf("  kp <v>               proportional gain (0..200)\n");
    printf("  ki <v>               integral gain (0..50)\n");
    printf("  kd <v>               derivative gain (0..50)\n");
    printf("  preset <name>        soft | balanced | aggressive | p\n");
    printf("  window <ms>          TPC window length (500..10000, default 2000)\n");
    printf("  polarity <low|high>  relay-board polarity (low = Wokwi default)\n");
    printf("  force <on|off|auto>  bypass controller for HW bring-up\n");
    printf("  plotter <on|off>     toggle Serial Plotter CSV stream\n");
    printf("  status               snapshot on LCD for a few seconds\n");
    printf("  help                 print this list\n");
    printf("  HW pins: DS18B20=D5, relay IN=D8, LCD I2C=20/21\n");
    printf("  --- Set Serial Monitor to 115200 baud ---\n");
    printf("==========================================================\n");

    xSemaphoreGive(g_lab52.ioMutex);
}

static void showStatusOnLcd(void)
{
    if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        snprintf(g_lab52.state.lcdBannerL1, sizeof(g_lab52.state.lcdBannerL1),
                 "T:%2dC SP:%2dC",
                 (int)(g_lab52.state.tempC + 0.5f),
                 (int)(g_lab52.state.setpointC + 0.5f));
        snprintf(g_lab52.state.lcdBannerL2, sizeof(g_lab52.state.lcdBannerL2),
                 "%-3s F:%3d%% kp%2d",
                 (g_lab52.state.mode == LAB5_2_MODE_PID) ? "PID" : "ONF",
                 (int)(g_lab52.state.fanPctActual + 0.5f),
                 (int)(g_lab52.config.kp + 0.5f));
        g_lab52.state.lcdBannerUntilMs = millis() + 6000UL;
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
        g_lab52.state.lcdBannerUntilMs = millis() + 8000UL;
        xSemaphoreGive(g_lab52.stateMutex);
    }
}

// ============================================================================
// PID presets (PROGMEM-stored, for cooling-fan plant)
// ============================================================================

typedef struct
{
    const char* name;
    float kp;
    float ki;
    float kd;
} PidPreset;

static const PidPreset PID_PRESETS[] PROGMEM = {
    /* Tuning targets: small DC fan (~1 W) cooling a thermistor in still air. */
    { "soft",       25.0f, 0.20f,  0.0f },
    { "balanced",   50.0f, 0.50f,  0.0f },
    { "aggressive", 80.0f, 1.50f,  5.0f },
    { "p",          50.0f, 0.0f,   0.0f }
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
// Command processing  (sscanf-based, with input sanitisation in taskCommand)
// ============================================================================

/** Apply a new set-point under the state mutex. Centralised so the bare-
 *  integer shortcut and the explicit `set <C>` form go through one path. */
static bool applySetpoint(float requested, float* outClamped)
{
    const float clamped = clampf(requested, LAB5_2_SETPOINT_MIN_C, LAB5_2_SETPOINT_MAX_C);
    if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        g_lab52.config.setpointC = clamped;
        g_lab52.state.setpointC  = clamped;
        xSemaphoreGive(g_lab52.stateMutex);
    }
    s_pid.Reset(0.0f);
    if (outClamped != NULL) { *outClamped = clamped; }
    return (requested >= LAB5_2_SETPOINT_MIN_C) && (requested <= LAB5_2_SETPOINT_MAX_C);
}

/** Strict bare-integer detection — matches `^[+-]?[0-9]+\s*$`.
 *  Mirrors the reference Indrumar Lab 6.1 `app_lab_6_1_i_ctrl_setpoint`
 *  behaviour where typing `25` is equivalent to `set 25`. */
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
    char  cmd[16] = {0};
    char  arg[16] = {0};
    float fvalue  = 0.0f;

    const int parsedTokens = sscanf(line, "%15s %15s", cmd, arg);
    if (parsedTokens < 1)
    {
        showHelpOnLcd();
        return;
    }

    /* Bare-integer setpoint shortcut: `25` is treated as `set 25`. */
    if (parsedTokens == 1 && isBareInteger(line))
    {
        const int requested = atoi(line);
        float clamped = 0.0f;
        const bool inRange = applySetpoint((float)requested, &clamped);
        if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            if (inRange)
            {
                printf("setpoint=%dC\n", (int)clamped);
            }
            else
            {
                printf("setpoint clamped to %dC (range %d..%d)\n",
                       (int)clamped,
                       (int)LAB5_2_SETPOINT_MIN_C,
                       (int)LAB5_2_SETPOINT_MAX_C);
            }
            xSemaphoreGive(g_lab52.ioMutex);
        }
        return;
    }

    /* String-arg commands first (mode, preset, force, plotter). */
    if (parsedTokens == 2 && iequals(cmd, "mode"))
    {
        if (iequals(arg, "onoff"))
        {
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.state.mode = LAB5_2_MODE_ONOFF;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            s_onoff.Init(false);
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
        if (applyPreset(arg)) { return; }
        if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            printf("(preset?) try: soft | balanced | aggressive | p\n");
            xSemaphoreGive(g_lab52.ioMutex);
        }
        return;
    }

    if (parsedTokens == 2 && iequals(cmd, "force"))
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
            printf("(force?) try: force on | force off | force auto\n");
            xSemaphoreGive(g_lab52.ioMutex);
        }
        return;
    }

    if (parsedTokens == 2 && iequals(cmd, "plotter"))
    {
        const bool wantOn = iequals(arg, "on") || iequals(arg, "1");
        const bool wantOff = iequals(arg, "off") || iequals(arg, "0");
        if (wantOn || wantOff)
        {
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.state.reportMode = wantOn ? LAB5_2_REPORT_MODE_PLOTTER
                                                  : LAB5_2_REPORT_MODE_SERIAL;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            return;
        }
        if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            printf("(plotter?) try: plotter on | plotter off\n");
            xSemaphoreGive(g_lab52.ioMutex);
        }
        return;
    }

    /* `polarity low` / `polarity high` — flip the relay-board polarity at
     * runtime. Critical for hardware bring-up: if the motor runs when the
     * firmware shows duty=0% / R-, you have an active-HIGH board on the
     * default active-LOW build, or vice versa. */
    if (parsedTokens == 2 && iequals(cmd, "polarity"))
    {
        const bool wantLow  = iequals(arg, "low")  || iequals(arg, "0");
        const bool wantHigh = iequals(arg, "high") || iequals(arg, "1");
        if (wantLow || wantHigh)
        {
            const bool activeLow = wantLow;
            Fan52_SetPolarity(activeLow);
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.config.relayActiveLow = activeLow;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                printf("relay polarity = %s\n",
                       activeLow ? "active-LOW (Wokwi/typical)"
                                 : "active-HIGH (raw NPN driver)");
                xSemaphoreGive(g_lab52.ioMutex);
            }
            return;
        }
        if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            printf("(polarity?) try: polarity low | polarity high\n");
            xSemaphoreGive(g_lab52.ioMutex);
        }
        return;
    }

    /* Numeric-arg commands. Re-parse from `line` since the first sscanf
     * grabbed a string into `arg` which may have eaten the number. */
    if (sscanf(line, "%15s %f", cmd, &fvalue) >= 2)
    {
        if (iequals(cmd, "set"))
        {
            float clamped = 0.0f;
            (void)applySetpoint(fvalue, &clamped);
            if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                /* avr-libc printf has no %f by default; format via integer + tenths. */
                const int whole  = (int)clamped;
                const int tenths = (int)((clamped - (float)whole) * 10.0f + (clamped >= 0.0f ? 0.5f : -0.5f));
                printf("setpoint=%d.%dC\n", whole, tenths < 0 ? -tenths : tenths);
                xSemaphoreGive(g_lab52.ioMutex);
            }
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
            fvalue = clampf(fvalue, 0.0f, 200.0f);
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.config.kp = fvalue;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            return;
        }
        if (iequals(cmd, "ki"))
        {
            fvalue = clampf(fvalue, 0.0f, 50.0f);
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.config.ki = fvalue;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            return;
        }
        if (iequals(cmd, "kd"))
        {
            fvalue = clampf(fvalue, 0.0f, 50.0f);
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.config.kd = fvalue;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            return;
        }
        if (iequals(cmd, "window"))
        {
            const long requested = (long)fvalue;
            long w = requested;
            if (w < (long)LAB5_2_RELAY_WINDOW_MIN_MS) { w = (long)LAB5_2_RELAY_WINDOW_MIN_MS; }
            if (w > (long)LAB5_2_RELAY_WINDOW_MAX_MS) { w = (long)LAB5_2_RELAY_WINDOW_MAX_MS; }
            Fan52_SetWindowMs((uint16_t)w);
            if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                g_lab52.config.relayWindowMs = (uint16_t)w;
                xSemaphoreGive(g_lab52.stateMutex);
            }
            if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                printf("relay window = %ld ms\n", w);
                xSemaphoreGive(g_lab52.ioMutex);
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
    if (iequals(cmd, "help"))
    {
        printCommandsSerial();
        showHelpOnLcd();
        return;
    }

    /* Unknown — flash help banner on LCD and tell the user exactly what we
     * parsed, so rogue characters from arrow-key history are obvious. */
    showHelpOnLcd();
    if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(80)) == pdTRUE)
    {
        if (parsedTokens >= 2)
        {
            printf("(unknown) cmd='%s' arg='%s' — type `help`\n", cmd, arg);
        }
        else
        {
            printf("(unknown) cmd='%s' — type `help`\n", cmd);
        }
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
        printf("[lab5_2][FATAL] FreeRTOS object allocation failed\n");
        for (;;) {}
    }

    /* Initial runtime state */
    g_lab52.state.tempC            = 0.0f;
    g_lab52.state.tempRawC         = 0.0f;
    g_lab52.state.sensorValid      = false;
    g_lab52.state.setpointC        = g_lab52.config.setpointC;
    g_lab52.state.hysteresisC      = g_lab52.config.hysteresisC;
    g_lab52.state.errorC           = 0.0f;
    g_lab52.state.pidOutput        = 0.0f;
    g_lab52.state.fanPctDemand     = 0.0f;
    g_lab52.state.fanPctActual     = 0.0f;
    g_lab52.state.relayOn          = false;
    g_lab52.state.mode             = LAB5_2_MODE_PID;
    g_lab52.state.reportMode       = LAB5_2_REPORT_MODE_SERIAL;
    g_lab52.state.forceMode        = LAB5_2_FORCE_AUTO;
    g_lab52.state.lastSampleMs     = 0UL;
    g_lab52.state.controlCycles    = 0UL;
    g_lab52.state.commandCounter   = 0UL;
    g_lab52.state.lcdBannerL1[0]   = '\0';
    g_lab52.state.lcdBannerL2[0]   = '\0';
    g_lab52.state.lcdBannerUntilMs = 0UL;

    /* Hardware bring-up: actuator off before the controller can swing it.
     * Init also applies the boot-time polarity from LAB5_2_RELAY_ACTIVE_LOW. */
    Fan52_Init();
    Fan52_SetWindowMs(g_lab52.config.relayWindowMs);
    Fan52_SetPolarity(g_lab52.config.relayActiveLow);

    /* I²C LCD */
    Wire.begin();
    s_lcd.init();
    s_lcd.backlight();
    s_lcd.clear();
    s_lcd.setCursor(0, 0);
    s_lcd.print(F("Lab5.2 cool fan"));
    s_lcd.setCursor(0, 1);
    s_lcd.print(F("Relay TPC PID"));

    /* Sensor + controllers */
    TempSensor52_Init();
    s_onoff.Init(false);
    s_pid.Init(g_lab52.config.kp,
               g_lab52.config.ki,
               g_lab52.config.kd,
               g_lab52.config.outputLimit,
               0.0f);

    /* Stack budget rationale: any task that calls printf needs ≥384 bytes on
     * the feilipu AVR FreeRTOS port (vprintf alone consumes ~150-200 B). All
     * tasks below DO call printf except taskFan, so they get ≥512 B. taskFan
     * touches only GPIO and the queue → 256 B is safe. taskCommand also runs
     * scanf, which needs the same printf-class stack. */
    BaseType_t ok;
    ok = xTaskCreate(taskAcquisition, "L52_ACQ",   512, NULL, 3, NULL);
    if (ok != pdPASS) { printf("[lab5_2][FATAL] ACQ task\n");  for (;;) {} }

    ok = xTaskCreate(taskControl,     "L52_CTRL",  512, NULL, 3, NULL);
    if (ok != pdPASS) { printf("[lab5_2][FATAL] CTRL task\n"); for (;;) {} }

    ok = xTaskCreate(taskFan,         "L52_FAN",   256, NULL, 2, NULL);
    if (ok != pdPASS) { printf("[lab5_2][FATAL] FAN task\n");  for (;;) {} }

    ok = xTaskCreate(taskDisplay,     "L52_DISP",  512, NULL, 1, NULL);
    if (ok != pdPASS) { printf("[lab5_2][FATAL] DISP task\n"); for (;;) {} }

    ok = xTaskCreate(taskCommand,     "L52_CMD",   640, NULL, 1, NULL);
    if (ok != pdPASS) { printf("[lab5_2][FATAL] CMD task\n");  for (;;) {} }

    ok = xTaskCreate(taskReport,      "L52_RPT",   512, NULL, 1, NULL);
    if (ok != pdPASS) { printf("[lab5_2][FATAL] RPT task\n");  for (;;) {} }

    printCommandsSerial();
    showHelpOnLcd();
}

void lab5_2_loop()
{
    /* Empty: scheduler runs everything. */
}

// ============================================================================
// Acquisition task — DS18B20 + conditioning pipeline
// ============================================================================

static void taskAcquisition(void* pv)
{
    (void)pv;

    /* Small offset so DallasTemperature has a moment after Wire.begin()
     * before the first conversion request. The reference uses a similar
     * staggered offset on each task. */
    vTaskDelay(pdMS_TO_TICKS(800));

    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        const bool ok = TempSensor52_Loop();
        const float tempC    = TempSensor52_GetTempC();
        const float tempRaw  = TempSensor52_GetRawTempC();

        if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            g_lab52.state.tempC        = tempC;
            g_lab52.state.tempRawC     = tempRaw;
            g_lab52.state.sensorValid  = ok;
            g_lab52.state.lastSampleMs = millis();
            xSemaphoreGive(g_lab52.stateMutex);
        }

        if (!ok && xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            printf("[L5.2][ACQ] DS18B20 read FAILED — check D5 wiring + 4.7k pull-up\n");
            xSemaphoreGive(g_lab52.ioMutex);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab52.config.acqTaskMs));
    }
}

// ============================================================================
// Control task — runs the active controller, posts to the fan queue
// ============================================================================

static void taskControl(void* pv)
{
    (void)pv;

    /* Stagger after the acquisition task so the first control cycle has
     * something real to work with. */
    vTaskDelay(pdMS_TO_TICKS(1100));

    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        Lab52Mode  mode;
        Lab52ForceMode force;
        float setpointC;
        float hysteresisC;
        float tempC;
        float kp, ki, kd, limit;
        bool  sensorValid;

        if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            mode        = g_lab52.state.mode;
            force       = g_lab52.state.forceMode;
            setpointC   = g_lab52.config.setpointC;
            hysteresisC = g_lab52.config.hysteresisC;
            tempC       = g_lab52.state.tempC;
            kp          = g_lab52.config.kp;
            ki          = g_lab52.config.ki;
            kd          = g_lab52.config.kd;
            limit       = g_lab52.config.outputLimit;
            sensorValid = g_lab52.state.sensorValid;
            xSemaphoreGive(g_lab52.stateMutex);
        }
        else
        {
            vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab52.config.ctrlTaskMs));
            continue;
        }

        /* Re-arm gains every cycle (cheap; lets `kp 50` take effect on the
         * very next iteration without an explicit re-init from the parser). */
        s_pid.Configure(kp, ki, kd, limit);

        Lab52ControlOutput out;
        out.mode        = mode;
        out.valid       = sensorValid;
        out.timestampMs = millis();
        out.errorC      = sensorValid ? (tempC - setpointC) : 0.0f;
        out.pidOutput   = 0.0f;
        out.fanPctTarget = 0.0f;

        if (force == LAB5_2_FORCE_ON)
        {
            out.fanPctTarget = (float)LAB5_2_FORCE_ON_PCT;
        }
        else if (force == LAB5_2_FORCE_OFF)
        {
            out.fanPctTarget = 0.0f;
        }
        else if (!sensorValid)
        {
            /* Fail-safe: no sensor ⇒ no control. Fan goes off. */
            out.fanPctTarget = 0.0f;
        }
        else if (mode == LAB5_2_MODE_PID)
        {
            const float dt  = (float)g_lab52.config.ctrlTaskMs / 1000.0f;
            const float raw = s_pid.Step(out.errorC, dt);
            out.pidOutput   = raw;

            /* Cooling-only: discard negative pull (fan can't reverse-cool).
             * Then clamp to [0, limit]. */
            float demand = raw;
            if (demand < 0.0f) { demand = 0.0f; }
            if (demand > limit) { demand = limit; }
            out.fanPctTarget = demand;
        }
        else /* LAB5_2_MODE_ONOFF */
        {
            const bool fanOn = s_onoff.Step(tempC, setpointC, hysteresisC);
            out.fanPctTarget = fanOn ? 100.0f : 0.0f;
        }

        /* Single-slot queue: overwrite is the right semantics — the fan
         * task only cares about the most recent demand. */
        xQueueOverwrite(g_lab52.qControl, &out);

        if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            g_lab52.state.errorC        = out.errorC;
            g_lab52.state.pidOutput     = out.pidOutput;
            g_lab52.state.fanPctDemand  = out.fanPctTarget;
            g_lab52.state.controlCycles = g_lab52.state.controlCycles + 1UL;
            xSemaphoreGive(g_lab52.stateMutex);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab52.config.ctrlTaskMs));
    }
}

// ============================================================================
// Fan task — receive demand, run conditioning + actuation
// ============================================================================

static void taskFan(void* pv)
{
    (void)pv;

    vTaskDelay(pdMS_TO_TICKS(1300));

    TickType_t lastWake = xTaskGetTickCount();

    Lab52ControlOutput out;
    out.fanPctTarget = 0.0f;
    out.valid        = false;

    for (;;)
    {
        /* Drain the queue without blocking — `xQueueReceive` with timeout 0
         * matches the "use latest known demand" semantics we want. */
        Lab52ControlOutput latest;
        if (xQueueReceive(g_lab52.qControl, &latest, 0) == pdTRUE)
        {
            out = latest;
        }

        const int demand = (int)(out.fanPctTarget + 0.5f);
        Fan52_SetDemandPct(demand);
        const int  actual   = Fan52_Loop();
        const bool relayNow = Fan52_IsRelayOn();

        if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            g_lab52.state.fanPctActual = (float)actual;
            g_lab52.state.relayOn      = relayNow;
            xSemaphoreGive(g_lab52.stateMutex);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab52.config.fanTaskMs));
    }
}

// ============================================================================
// Display task — drive the LCD
// ============================================================================

static void taskDisplay(void* pv)
{
    (void)pv;

    vTaskDelay(pdMS_TO_TICKS(2000));

    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        Lab52RuntimeState snapshot;
        if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            snapshot = g_lab52.state;
            xSemaphoreGive(g_lab52.stateMutex);
            lcdRender(&snapshot);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab52.config.dispTaskMs));
    }
}

// ============================================================================
// Command task — read Serial, sanitise, dispatch
// ============================================================================

/** Sanitise a freshly-read line in place: strip ANSI escape sequences (e.g.
 *  the `\x1b[A` an arrow-key history sends), drop other non-printables,
 *  trim leading/trailing whitespace. Returns the new strlen. */
static int sanitiseLine(char* buf)
{
    /* --- Pass 1: copy printable chars only, eat ANSI escapes --- */
    int dst = 0;
    for (int src = 0; buf[src] != '\0'; ++src)
    {
        const unsigned char c = (unsigned char)buf[src];

        if (c == 0x1B) /* ESC: skip the whole CSI sequence (e.g. "[A") */
        {
            if (buf[src + 1] == '[')
            {
                src += 2;
                while (buf[src] != '\0' && !(buf[src] >= 0x40 && buf[src] <= 0x7E))
                {
                    ++src;
                }
                if (buf[src] == '\0') { break; }
            }
            continue;
        }
        if (c < 0x20 || c > 0x7E) { continue; } /* drop other non-printables */

        buf[dst++] = (char)c;
    }
    buf[dst] = '\0';

    /* --- Pass 2: trim leading whitespace --- */
    int start = 0;
    while (buf[start] == ' ' || buf[start] == '\t') { ++start; }
    if (start > 0)
    {
        memmove(buf, buf + start, (size_t)(dst - start + 1));
        dst -= start;
    }

    /* --- Pass 3: trim trailing whitespace --- */
    while (dst > 0 && (buf[dst - 1] == ' ' || buf[dst - 1] == '\t'))
    {
        buf[--dst] = '\0';
    }

    return dst;
}

static void taskCommand(void* pv)
{
    (void)pv;

    /* Stagger after the other tasks so the boot banner is fully printed
     * before the first command can interleave with it. */
    vTaskDelay(pdMS_TO_TICKS(1500));

    char line[48];

    for (;;)
    {
        /* Read one line at a time via stdio. The conversion specifier
         *   " %47[^\r\n]"
         * skips leading whitespace, then captures up to 47 non-newline
         * characters into `line`. Same pattern as Lab 4.2's command task.
         *
         * scanf() blocks until a complete line arrives, so we don't need
         * a periodic vTaskDelay at the bottom of the loop — between
         * commands the task is parked inside UartGetChar(). */
        line[0] = '\0';
        if (scanf(" %47[^\r\n]", line) != 1)
        {
            /* No real line — yield and try again to avoid CPU monopoly
             * if scanf returned EOF / mismatch on a stray byte. */
            vTaskDelay(pdMS_TO_TICKS(g_lab52.config.cmdTaskMs));
            continue;
        }

        /* sanitiseLine() still useful — arrow-key escapes (ESC [ A etc.)
         * pass right through %[^\r\n]; we strip them after the fact. */
        if (sanitiseLine(line) == 0)
        {
            continue;
        }

        if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            printf("> %s\n", line);
            xSemaphoreGive(g_lab52.ioMutex);
        }

        processCommand(line);

        if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            g_lab52.state.commandCounter = g_lab52.state.commandCounter + 1UL;
            xSemaphoreGive(g_lab52.stateMutex);
        }
    }
}

// ============================================================================
// Report task — Serial Plotter CSV stream OR human-readable status
// ============================================================================

static void taskReport(void* pv)
{
    (void)pv;

    vTaskDelay(pdMS_TO_TICKS(2500));

    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        Lab52RuntimeState s;
        if (xSemaphoreTake(g_lab52.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            s = g_lab52.state;
            xSemaphoreGive(g_lab52.stateMutex);
        }
        else
        {
            vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab52.config.reportTaskMs));
            continue;
        }

        const int curTemp   = (int)(s.tempC + 0.5f);
        const int setpoint  = (int)(s.setpointC + 0.5f);
        const int errorInt  = (int)(s.errorC + (s.errorC >= 0 ? 0.5f : -0.5f));
        const int duty      = (int)(s.fanPctActual + 0.5f);
        const int upper     = (int)(s.setpointC + s.hysteresisC + 0.5f);
        const int lower     = (int)(s.setpointC - s.hysteresisC + 0.5f);

        if (xSemaphoreTake(g_lab52.ioMutex, pdMS_TO_TICKS(80)) != pdTRUE)
        {
            vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab52.config.reportTaskMs));
            continue;
        }

        const int relayBinary = s.relayOn ? 1 : 0;

        if (s.reportMode == LAB5_2_REPORT_MODE_PLOTTER)
        {
            /* Format that the badlogicgames Serial Plotter VS-Code extension
             * (and Arduino's built-in plotter) recognise. Mirrors the report
             * format used in the reference Lab 6.1 + 6.2. Relay is plotted
             * as 0/1 so you can correlate TPC slices with PV oscillations. */
            printf(">Temp:%d,SetPoint:%d,Upper:%d,Lower:%d,Error:%d,Fan:%d,Relay:%d\n",
                   curTemp, setpoint, upper, lower, errorInt, duty, relayBinary);
        }
        else if (s.reportMode == LAB5_2_REPORT_MODE_SERIAL)
        {
            const char* modeStr   = (s.mode == LAB5_2_MODE_PID) ? "PID" : "ONF";
            const char* relayStr  = s.relayOn ? "ON " : "off";
            const char* errSign   = (errorInt > 0) ? "+" : "";

            if (s.sensorValid)
            {
                printf("[L5.2] T=%dC SP=%dC err=%s%dC duty=%d%% R=%s mode=%s",
                       curTemp, setpoint, errSign, errorInt, duty, relayStr, modeStr);
            }
            else
            {
                printf("[L5.2] T=??C SP=%dC err=%s%dC duty=%d%% R=%s mode=%s",
                       setpoint, errSign, errorInt, duty, relayStr, modeStr);
            }

            if (s.forceMode != LAB5_2_FORCE_AUTO)
            {
                printf(" force=%s",
                       (s.forceMode == LAB5_2_FORCE_ON) ? "on" : "off");
            }
            printf("\n");
        }
        /* LAB5_2_REPORT_MODE_LCD ⇒ silent on Serial, LCD already updates. */

        xSemaphoreGive(g_lab52.ioMutex);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab52.config.reportTaskMs));
    }
}
