#include "Lab4_2_main.h"
#include "Lab4_2_Shared.h"
#include "lib_sig_cond.h"
#include "srv_relay_control.h"
#include "srv_servo_control.h"

#include "../drivers/SerialStdioDriver.h"
#include "../led/LedDriver.h"

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

Lab42Shared g_lab42;

static LiquidCrystal_I2C s_lcd(LAB4_2_LCD_I2C_ADDR, 16, 2);
static LedDriver s_ledGreen(LAB4_2_LED_GREEN_PIN);
static LedDriver s_ledRed(LAB4_2_LED_RED_PIN);
static LedDriver s_ledServoAlert(LAB4_2_LED_SERVO_ALERT_PIN);
static RelayControl42 s_relayCtrl;
static ServoControl42 s_servoCtrl;
static SigCond42State s_sigState;

static void taskCommand(void* pv);
static void taskConditioning(void* pv);
static void taskActuator(void* pv);
static void taskDisplay(void* pv);

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

static void load_defaults(Lab42Config* cfg)
{
    cfg->commandMs = LAB4_2_DEFAULT_CMD_MS;
    cfg->conditionMs = LAB4_2_DEFAULT_COND_MS;
    cfg->controlMs = LAB4_2_DEFAULT_CTRL_MS;
    cfg->reportMs = LAB4_2_DEFAULT_REPORT_MS;
    cfg->relayDebounceMs = LAB4_2_DEFAULT_DEBOUNCE_MS;
    cfg->signalAlpha = LAB4_2_DEFAULT_ALPHA;
    cfg->rampPctPerSec = LAB4_2_DEFAULT_RAMP_PCT_S;
}

static void lcd_render(const Lab42RuntimeState* s)
{
    char line1[17] = {0};
    char line2[17] = {0};

    snprintf(line1, sizeof(line1), "R:%s S:%3ud", s->relayState ? "ON " : "OFF", s->servoDeg);

    snprintf(line2,
             sizeof(line2),
             "P:%3u%% %c%c%c%c",
             (unsigned)(s->potPct + 0.5f),
             s->clampAlert ? 'C' : '-',
             s->limitAlert ? 'L' : '-',
             s->relayDebounceAlert ? 'D' : '-',
             s->servoExtremeAlert ? 'S' : '-');

    s_lcd.clear();
    s_lcd.setCursor(0, 0);
    s_lcd.print(line1);
    s_lcd.setCursor(0, 1);
    s_lcd.print(line2);
}

void lab4_2_setup()
{
    SerialStdioInit(9600);

    load_defaults(&g_lab42.config);

    g_lab42.stateMutex = xSemaphoreCreateMutex();
    g_lab42.ioMutex = xSemaphoreCreateMutex();
    if ((g_lab42.stateMutex == NULL) || (g_lab42.ioMutex == NULL))
    {
        for (;;) {}
    }

    g_lab42.state.relayCommand = false;
    g_lab42.state.rawServoPct = 0.0f;
    g_lab42.state.servoManualMode = false;
    g_lab42.state.potAdc = 0u;
    g_lab42.state.potPct = 0.0f;
    g_lab42.state.clampedServoPct = 0.0f;
    g_lab42.state.medianServoPct = 0.0f;
    g_lab42.state.weightedServoPct = 0.0f;
    g_lab42.state.rampedServoPct = 0.0f;
    g_lab42.state.relayState = false;
    g_lab42.state.servoPct = 0.0f;
    g_lab42.state.servoDeg = 0u;
    g_lab42.state.clampAlert = false;
    g_lab42.state.limitAlert = false;
    g_lab42.state.relayDebounceAlert = false;
    g_lab42.state.servoExtremeAlert = false;
    g_lab42.state.invalidCommandCount = 0u;
    g_lab42.state.commandSeq = 0u;
    g_lab42.state.stateSeq = 0u;
    g_lab42.state.lastCommandMs = millis();
    g_lab42.state.responseLatencyMs = 0u;

    Wire.begin();
    s_lcd.init();
    s_lcd.backlight();
    s_lcd.clear();
    s_lcd.setCursor(0, 0);
    s_lcd.print("Lab4.2 Init");
    s_lcd.setCursor(0, 1);
    s_lcd.print("Relay+Servo");

    s_ledGreen.Init();
    s_ledRed.Init();
    s_ledServoAlert.Init();
    s_ledGreen.Off();
    s_ledRed.On();
    s_ledServoAlert.Off();

    relay42_init(&s_relayCtrl,
                 LAB4_2_RELAY_PIN,
                 (LAB4_2_RELAY_ACTIVE_LOW != 0),
                 g_lab42.config.relayDebounceMs,
                 false);

    servo42_init(&s_servoCtrl, LAB4_2_SERVO_PIN, 0u, 180u);
    servo42_apply_pct(&s_servoCtrl, 0.0f);

    sig42_init(&s_sigState, 0.0f, millis());

    BaseType_t ok;
    ok = xTaskCreate(taskCommand, "L42_CMD", 512, NULL, 1, NULL);
    if (ok != pdPASS) { for (;;) {} }

    ok = xTaskCreate(taskConditioning, "L42_COND", 512, NULL, 2, NULL);
    if (ok != pdPASS) { for (;;) {} }

    ok = xTaskCreate(taskActuator, "L42_ACT", 512, NULL, 2, NULL);
    if (ok != pdPASS) { for (;;) {} }

    ok = xTaskCreate(taskDisplay, "L42_DISP", 768, NULL, 1, NULL);
    if (ok != pdPASS) { for (;;) {} }

}

void lab4_2_loop()
{
}

static void taskCommand(void* pv)
{
    (void)pv;

    for (;;)
    {
        char line[48] = {0};
        if (scanf(" %47[^\r\n]", line) == 1)
        {
            bool known = false;
            bool relayChange = false;
            bool relayValue = false;
            bool servoChange = false;
            float servoValue = 0.0f;
            char arg1[20] = {0};
            char arg2[20] = {0};
            int number = 0;

            if (sscanf(line, "%19s %19s", arg1, arg2) == 2)
            {
                if (iequals(arg1, "relay") && iequals(arg2, "on"))
                {
                    known = true;
                    relayChange = true;
                    relayValue = true;
                }
                else if (iequals(arg1, "relay") && iequals(arg2, "off"))
                {
                    known = true;
                    relayChange = true;
                    relayValue = false;
                }
            }

            if (!known && (sscanf(line, "servo %d", &number) == 1))
            {
                known = true;
                servoChange = true;
                servoValue = (float)number;
            }

            if (!known && (sscanf(line, "servo_pct %d", &number) == 1))
            {
                known = true;
                servoChange = true;
                servoValue = (float)number;
            }

            if (!known && (sscanf(line, "%19s %19s", arg1, arg2) == 2) && iequals(arg1, "servo") && iequals(arg2, "min"))
            {
                known = true;
                servoChange = true;
                servoValue = 0.0f;
            }

            if (!known && (sscanf(line, "%19s %19s", arg1, arg2) == 2) && iequals(arg1, "servo") && iequals(arg2, "max"))
            {
                known = true;
                servoChange = true;
                servoValue = 100.0f;
            }

            bool servoPotMode = false;
            if (!known && (sscanf(line, "%19s %19s", arg1, arg2) == 2) && iequals(arg1, "servo") && iequals(arg2, "pot"))
            {
                known = true;
                servoPotMode = true;
            }

            if (!known && (sscanf(line, "%19s", arg1) == 1) && iequals(arg1, "status"))
            {
                known = true;
            }

            if (known)
            {
                if (xSemaphoreTake(g_lab42.stateMutex, pdMS_TO_TICKS(30)) == pdTRUE)
                {
                    if (relayChange)
                    {
                        g_lab42.state.relayCommand = relayValue;
                    }
                    if (servoChange)
                    {
                        g_lab42.state.rawServoPct = servoValue;
                        g_lab42.state.servoManualMode = true;
                    }
                    if (servoPotMode)
                    {
                        g_lab42.state.servoManualMode = false;
                    }
                    g_lab42.state.commandSeq++;
                    g_lab42.state.lastCommandMs = millis();
                    xSemaphoreGive(g_lab42.stateMutex);
                }

                if (xSemaphoreTake(g_lab42.ioMutex, pdMS_TO_TICKS(30)) == pdTRUE)
                {
                    printf("%s\n", line);
                    xSemaphoreGive(g_lab42.ioMutex);
                }
            }
            else
            {
                if (xSemaphoreTake(g_lab42.stateMutex, pdMS_TO_TICKS(30)) == pdTRUE)
                {
                    g_lab42.state.invalidCommandCount++;
                    xSemaphoreGive(g_lab42.stateMutex);
                }

                if (xSemaphoreTake(g_lab42.ioMutex, pdMS_TO_TICKS(30)) == pdTRUE)
                {
                    printf("%s\n", line);
                    xSemaphoreGive(g_lab42.ioMutex);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(g_lab42.config.commandMs));
    }
}

static void taskConditioning(void* pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        float raw = 0.0f;

        if (xSemaphoreTake(g_lab42.stateMutex, pdMS_TO_TICKS(30)) == pdTRUE)
        {
            raw = g_lab42.state.rawServoPct;
            xSemaphoreGive(g_lab42.stateMutex);
        }

        float clamped = 0.0f;
        float median = 0.0f;
        float weighted = 0.0f;
        float ramped = 0.0f;
        bool clampAlert = false;
        bool limitAlert = false;

        sig42_step(&s_sigState,
                   raw,
                   millis(),
                   g_lab42.config.signalAlpha,
                   g_lab42.config.rampPctPerSec,
                   &clamped,
                   &median,
                   &weighted,
                   &ramped,
                   &clampAlert,
                   &limitAlert);

        if (xSemaphoreTake(g_lab42.stateMutex, pdMS_TO_TICKS(30)) == pdTRUE)
        {
            g_lab42.state.clampedServoPct = clamped;
            g_lab42.state.medianServoPct = median;
            g_lab42.state.weightedServoPct = weighted;
            g_lab42.state.rampedServoPct = ramped;
            g_lab42.state.clampAlert = clampAlert;
            g_lab42.state.limitAlert = limitAlert;
            xSemaphoreGive(g_lab42.stateMutex);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab42.config.conditionMs));
    }
}

static void taskActuator(void* pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        const int rawAdcVal = analogRead(LAB4_2_SERVO_POT_PIN);
        const float potPct = (float)rawAdcVal / 1023.0f * 100.0f;

        bool relayCmd = false;
        float servoPct = 0.0f;
        unsigned long cmdMs = millis();
        bool servoManualMode = false;

        if (xSemaphoreTake(g_lab42.stateMutex, pdMS_TO_TICKS(30)) == pdTRUE)
        {
            relayCmd = g_lab42.state.relayCommand;
            const float cmdServo = g_lab42.state.rampedServoPct;
            servoManualMode = g_lab42.state.servoManualMode;
            servoPct = servoManualMode ? cmdServo : potPct;
            cmdMs = g_lab42.state.lastCommandMs;
            xSemaphoreGive(g_lab42.stateMutex);
        }

        const unsigned long nowMs = millis();
        relay42_set_command(&s_relayCtrl, relayCmd, nowMs);
        relay42_update(&s_relayCtrl, nowMs);

        const bool relayState = relay42_get_state(&s_relayCtrl);
        if (relayState)
        {
            servo42_apply_pct(&s_servoCtrl, servoPct);
        }

        const bool debounceAlert = relay42_consume_debounce_alert(&s_relayCtrl);
        const uint16_t servoDeg = relayState ? servo42_get_deg(&s_servoCtrl) : 0u;
        const float servoPctApplied = relayState ? servo42_get_pct(&s_servoCtrl) : 0.0f;
        const bool servoExtremeAlert = relayState && ((servoPctApplied <= 0.1f) || (servoPctApplied >= 99.9f));

        if (relayState)
        {
            s_ledGreen.On();
            s_ledRed.Off();
        }
        else
        {
            s_ledGreen.Off();
            s_ledRed.On();
        }

        if (servoExtremeAlert)
        {
            s_ledServoAlert.On();
        }
        else
        {
            s_ledServoAlert.Off();
        }

        if (xSemaphoreTake(g_lab42.stateMutex, pdMS_TO_TICKS(30)) == pdTRUE)
        {
            g_lab42.state.potAdc = (uint16_t)rawAdcVal;
            g_lab42.state.potPct = potPct;
            g_lab42.state.relayState = relayState;
            g_lab42.state.servoDeg = servoDeg;
            g_lab42.state.servoPct = servoPctApplied;
            if (debounceAlert)
            {
                g_lab42.state.relayDebounceAlert = true;
            }
            g_lab42.state.servoExtremeAlert = servoExtremeAlert;
            const unsigned long latency = nowMs - cmdMs;
            g_lab42.state.responseLatencyMs = (latency > 65535ul) ? 65535u : (uint16_t)latency;
            g_lab42.state.stateSeq++;
            xSemaphoreGive(g_lab42.stateMutex);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab42.config.controlMs));
    }
}

static void taskDisplay(void* pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        Lab42RuntimeState snapshot;

        if (xSemaphoreTake(g_lab42.stateMutex, pdMS_TO_TICKS(30)) == pdTRUE)
        {
            snapshot = g_lab42.state;
            xSemaphoreGive(g_lab42.stateMutex);

            if (xSemaphoreTake(g_lab42.ioMutex, pdMS_TO_TICKS(30)) == pdTRUE)
            {
                lcd_render(&snapshot);
                xSemaphoreGive(g_lab42.ioMutex);
            }
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab42.config.reportMs));
    }
}
