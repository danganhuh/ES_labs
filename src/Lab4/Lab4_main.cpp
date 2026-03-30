#include "Lab4_main.h"
#include "Lab4_Shared.h"
#include "srv_light_control.h"
#include "srv_motor_control.h"

#include "../drivers/SerialStdioDriver.h"
#include "../led/LedDriver.h"

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <stdio.h>
#include <string.h>

Lab4Shared g_lab4;
static LedDriver s_greenLed(LAB4_LED_GREEN_PIN);
static LedDriver s_redLed(LAB4_LED_RED_PIN);

static LiquidCrystal_I2C s_lcd(LAB4_LCD_I2C_ADDR, 16, 2);

static void taskCommand(void* pv);
static void taskConditioning(void* pv);
static void taskActuator(void* pv);
static void taskDisplay(void* pv);

static void loadDefaultConfig(Lab4Config* cfg)
{
    cfg->controlMs = LAB4_DEFAULT_CTRL_MS;
    cfg->conditionMs = LAB4_DEFAULT_COND_MS;
    cfg->reportMs = LAB4_DEFAULT_REPORT_MS;
    cfg->debounceOnMs = LAB4_DEFAULT_DEBOUNCE_ON_MS;
    cfg->debounceOffMs = LAB4_DEFAULT_DEBOUNCE_OFF_MS;
    cfg->motorAlpha = LAB4_DEFAULT_MOTOR_ALPHA;
    cfg->motorRampPctPerSec = LAB4_DEFAULT_MOTOR_RAMP;
}

static void lcd_print_state(const Lab4RuntimeState* state)
{
    char line1[17];
    char line2[17];

    snprintf(line1,
             sizeof(line1),
             "R:%s F:%s",
             (state->relayState == HIGH) ? "ON " : "OFF",
             (state->motorStatePct > 1.0f) ? "ON " : "OFF");

    snprintf(line2,
             sizeof(line2),
             "A:%c%c L:%3ums",
             state->relayDebounceAlert ? 'R' : '-',
             state->motorSaturationAlert ? 'M' : '-',
             state->responseLatencyMs);

    s_lcd.clear();
    s_lcd.setCursor(0, 0);
    s_lcd.print(line1);
    s_lcd.setCursor(0, 1);
    s_lcd.print(line2);
}

void lab4_setup()
{
    SerialStdioInit(9600);

    loadDefaultConfig(&g_lab4.config);

    g_lab4.stateMutex = xSemaphoreCreateMutex();
    g_lab4.ioMutex = xSemaphoreCreateMutex();
    g_lab4.controlMutex = xSemaphoreCreateMutex();
    if ((g_lab4.stateMutex == NULL) || (g_lab4.ioMutex == NULL) || (g_lab4.controlMutex == NULL))
    {
        for (;;) {}
    }

    g_lab4.state.cmdRelayState = LOW;
    g_lab4.state.cmdMotorPct = 0.0f;
    g_lab4.state.invalidCommandCount = 0;
    g_lab4.state.cmdSeq = 0;
    g_lab4.state.lastCommandMs = millis();
    g_lab4.state.fanIsOn = false;
    g_lab4.state.fanOnStartMs = 0;
    g_lab4.state.fanLastOnDurationMs = 0;
    g_lab4.state.fanTotalOnMs = 0;
    g_lab4.state.relayState = LOW;
    g_lab4.state.motorStatePct = 0.0f;
    g_lab4.state.relayDebounceAlert = false;
    g_lab4.state.motorSaturationAlert = false;
    g_lab4.state.relayTransitions = 0;
    g_lab4.state.responseLatencyMs = 0;
    g_lab4.state.stateSeq = 0;

    Wire.begin();
    s_lcd.init();
    s_lcd.backlight();
    s_lcd.clear();
    s_lcd.setCursor(0, 0);
    s_lcd.print("Lab4 Actuator");
    s_lcd.setCursor(0, 1);
    s_lcd.print("Init...");

    s_greenLed.Init();
    s_redLed.Init();
    s_greenLed.Off();
    s_redLed.On();

    if (xSemaphoreTake(g_lab4.controlMutex, portMAX_DELAY) == pdTRUE)
    {
        srv_light_control_init(LAB4_RELAY_PIN,
                               g_lab4.config.debounceOnMs,
                               g_lab4.config.debounceOffMs);
        srv_motor_control_init(LAB4_MOTOR_ENA_PIN,
                               LAB4_MOTOR_IN1_PIN,
                               LAB4_MOTOR_IN2_PIN,
                               g_lab4.config.motorAlpha,
                               g_lab4.config.motorRampPctPerSec);
        xSemaphoreGive(g_lab4.controlMutex);
    }

    BaseType_t ok;
    ok = xTaskCreate(taskCommand, "L4_CMD", 512, NULL, 1, NULL);
    if (ok != pdPASS) { for (;;) {} }

    ok = xTaskCreate(taskConditioning, "L4_COND", 512, NULL, 2, NULL);
    if (ok != pdPASS) { for (;;) {} }

    ok = xTaskCreate(taskActuator, "L4_ACT", 384, NULL, 2, NULL);
    if (ok != pdPASS) { for (;;) {} }

    ok = xTaskCreate(taskDisplay, "L4_DISP", 768, NULL, 1, NULL);
    if (ok != pdPASS) { for (;;) {} }
}

void lab4_loop()
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
            char action[20] = {0};
            char target[20] = {0};
            const int tokens = sscanf(line, "%19s %19s", action, target);

            bool known = true;

            if ((tokens >= 2) &&
                ((strcmp(action, "on") == 0) || (strcmp(action, "ON") == 0)) &&
                ((strcmp(target, "relay") == 0) || (strcmp(target, "RELAY") == 0)))
            {
                if (xSemaphoreTake(g_lab4.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
                {
                    if (g_lab4.state.cmdRelayState == LOW)
                    {
                        g_lab4.state.fanOnStartMs = millis();
                        g_lab4.state.fanIsOn = true;
                    }
                    g_lab4.state.cmdRelayState = HIGH;
                    g_lab4.state.cmdMotorPct = 100.0f;
                    g_lab4.state.lastCommandMs = millis();
                    g_lab4.state.cmdSeq++;
                    xSemaphoreGive(g_lab4.stateMutex);
                }

                if (xSemaphoreTake(g_lab4.ioMutex, pdMS_TO_TICKS(20)) == pdTRUE)
                {
                    printf("on relay command detected. Response of the actuators: relay=ON, fan=ON\n");
                    xSemaphoreGive(g_lab4.ioMutex);
                }
            }
            else if ((tokens >= 2) &&
                     ((strcmp(action, "off") == 0) || (strcmp(action, "OFF") == 0)) &&
                     ((strcmp(target, "relay") == 0) || (strcmp(target, "RELAY") == 0)))
            {
                if (xSemaphoreTake(g_lab4.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
                {
                    if ((g_lab4.state.cmdRelayState == HIGH) && g_lab4.state.fanIsOn)
                    {
                        const unsigned long nowMs = millis();
                        const unsigned long elapsedMs = nowMs - g_lab4.state.fanOnStartMs;
                        g_lab4.state.fanLastOnDurationMs = elapsedMs;
                        g_lab4.state.fanTotalOnMs += elapsedMs;
                        g_lab4.state.fanIsOn = false;
                    }
                    g_lab4.state.cmdRelayState = LOW;
                    g_lab4.state.cmdMotorPct = 0.0f;
                    g_lab4.state.lastCommandMs = millis();
                    g_lab4.state.cmdSeq++;
                    xSemaphoreGive(g_lab4.stateMutex);
                }

                if (xSemaphoreTake(g_lab4.ioMutex, pdMS_TO_TICKS(20)) == pdTRUE)
                {
                    printf("off relay command detected. Response of the actuators: relay=OFF, fan=OFF\n");
                    xSemaphoreGive(g_lab4.ioMutex);
                }
            }
            else if ((tokens >= 2) &&
                     ((strcmp(action, "time") == 0) || (strcmp(action, "TIME") == 0)) &&
                     ((strcmp(target, "fan") == 0) || (strcmp(target, "FAN") == 0)))
            {
                unsigned long lastMs = 0;
                unsigned long totalMs = 0;
                unsigned long currentMs = 0;
                bool isOn = false;

                if (xSemaphoreTake(g_lab4.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
                {
                    lastMs = g_lab4.state.fanLastOnDurationMs;
                    totalMs = g_lab4.state.fanTotalOnMs;
                    isOn = g_lab4.state.fanIsOn;
                    if (isOn)
                    {
                        currentMs = millis() - g_lab4.state.fanOnStartMs;
                    }
                    xSemaphoreGive(g_lab4.stateMutex);
                }

                if (xSemaphoreTake(g_lab4.ioMutex, pdMS_TO_TICKS(20)) == pdTRUE)
                {
                    printf("time fan command detected. Response: last_on=%lu ms, current_on=%lu ms, total_on=%lu ms\n",
                           lastMs,
                           currentMs,
                           totalMs + currentMs);
                    xSemaphoreGive(g_lab4.ioMutex);
                }
            }
            else
            {
                known = false;
            }

            if (!known)
            {
                if (xSemaphoreTake(g_lab4.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
                {
                    g_lab4.state.invalidCommandCount++;
                    xSemaphoreGive(g_lab4.stateMutex);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(LAB4_CMD_TASK_MS));
    }
}

static void taskConditioning(void* pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        int cmdRelay = LOW;
        float cmdMotor = 0.0f;
        unsigned long cmdTs = millis();

        if (xSemaphoreTake(g_lab4.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            cmdRelay = g_lab4.state.cmdRelayState;
            cmdMotor = g_lab4.state.cmdMotorPct;
            cmdTs = g_lab4.state.lastCommandMs;
            xSemaphoreGive(g_lab4.stateMutex);
        }

        if (cmdRelay == HIGH)
        {
            cmdMotor = 100.0f;
        }
        else
        {
            cmdMotor = 0.0f;
        }

        unsigned long nowMs = millis();
        bool lightChanged = false;
        int relayState = LOW;
        float motorState = 0.0f;
        bool relayAlert = false;
        bool motorAlert = false;
        uint32_t transitions = 0;

        if (xSemaphoreTake(g_lab4.controlMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            srv_light_control_set_state(cmdRelay);
            srv_motor_control_set_speed(cmdMotor);

            srv_light_control_condition(nowMs);
            srv_motor_control_condition(g_lab4.config.conditionMs / 1000.0f);

            relayState = srv_light_control_get_state();
            motorState = srv_motor_control_get_state();
            relayAlert = srv_light_control_get_alert();
            motorAlert = srv_motor_control_get_saturation_alert();
            transitions = srv_light_control_get_transitions();
            lightChanged = srv_light_control_get_state_changed();

            xSemaphoreGive(g_lab4.controlMutex);
        }

        if (xSemaphoreTake(g_lab4.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            g_lab4.state.cmdMotorPct = cmdMotor;
            g_lab4.state.relayState = relayState;
            g_lab4.state.motorStatePct = motorState;
            g_lab4.state.relayDebounceAlert = relayAlert;
            g_lab4.state.motorSaturationAlert = motorAlert;
            g_lab4.state.relayTransitions = transitions;
            g_lab4.state.stateSeq++;

            if (lightChanged)
            {
                const unsigned long latency = nowMs - cmdTs;
                g_lab4.state.responseLatencyMs = static_cast<uint16_t>((latency > 65535UL) ? 65535UL : latency);
            }
            xSemaphoreGive(g_lab4.stateMutex);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab4.config.conditionMs));
    }
}

static void taskActuator(void* pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        if (xSemaphoreTake(g_lab4.controlMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            srv_light_control_apply();
            srv_motor_control_apply();

            const int relayState = srv_light_control_get_state();
            if (relayState == HIGH)
            {
                s_greenLed.On();
                s_redLed.Off();
            }
            else
            {
                s_greenLed.Off();
                s_redLed.On();
            }

            xSemaphoreGive(g_lab4.controlMutex);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab4.config.controlMs));
    }
}

static void taskDisplay(void* pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        Lab4RuntimeState snap;
        bool haveState = false;

        if (xSemaphoreTake(g_lab4.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            snap = g_lab4.state;
            xSemaphoreGive(g_lab4.stateMutex);
            haveState = true;
        }

        if (haveState)
        {
            if (xSemaphoreTake(g_lab4.ioMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                lcd_print_state(&snap);
                xSemaphoreGive(g_lab4.ioMutex);
            }
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab4.config.reportMs));
    }
}
