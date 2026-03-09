#include "Lab3_main.h"
#include "Lab3_Shared.h"
#include "SignalConditioning.h"
#include "AlertManager.h"
#include "../sensor/NtcAdcDriver.h"

#include <Arduino.h>
#include <stdio.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <stdlib.h>
#include <string.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

Lab3Shared g_lab3;

static NtcAdcDriver      s_ntc(LAB3_NTC_PIN);
static SignalConditioner s_conditioner;
static AlertManager      s_alert(LAB3_ALERT_LED_PIN);
static LiquidCrystal_I2C s_lcd(LAB3_LCD_I2C_ADDR, 16, 2);
static FILE s_lcdStream;
static uint8_t s_lcdCol = 0u;
static uint8_t s_lcdRow = 0u;

static void taskAcquisition(void *pv);
static void taskConditioning(void *pv);
static void taskAlerting(void *pv);
static void taskDisplay(void *pv);
static void taskLcdDisplay(void *pv);

static const TickType_t MUTEX_TIMEOUT_TICKS = pdMS_TO_TICKS(20);

static int lcd_putchar(char c, FILE *stream)
{
    (void)stream;

    if (c == '\r')
    {
        return 0;
    }

    if (c == '\n')
    {
        s_lcdRow = (uint8_t)((s_lcdRow + 1u) % 2u);
        s_lcdCol = 0u;
        s_lcd.setCursor(0, s_lcdRow);
        return 0;
    }

    if (s_lcdCol >= 16u)
    {
        s_lcdRow = (uint8_t)((s_lcdRow + 1u) % 2u);
        s_lcdCol = 0u;
        s_lcd.setCursor(0, s_lcdRow);
    }

    s_lcd.write(c);
    s_lcdCol++;
    return 0;
}

static void lcd_printf_lines(const char* line1, const char* line2)
{
    s_lcd.clear();
    s_lcdRow = 0u;
    s_lcdCol = 0u;
    s_lcd.setCursor(0, 0);
    fprintf(&s_lcdStream, "%-16.16s\n%-16.16s", line1 ? line1 : "", line2 ? line2 : "");
}

static bool readLineWithTimeout(char* outBuf, size_t outLen, unsigned long timeoutMs)
{
    if (!outBuf || outLen == 0u)
    {
        return false;
    }

    size_t idx = 0u;
    const unsigned long startMs = millis();

    while ((millis() - startMs) < timeoutMs)
    {
        while (Serial.available() > 0)
        {
            const int rx = Serial.read();
            if (rx < 0)
            {
                continue;
            }

            const char c = (char)rx;
            if (c == '\r' || c == '\n')
            {
                if (idx == 0u)
                {
                    continue;
                }

                outBuf[idx] = '\0';
                return true;
            }

            if (idx < (outLen - 1u))
            {
                outBuf[idx++] = c;
            }
        }

        delay(10);
    }

    outBuf[0] = '\0';
    return false;
}

static const char* fmtFloat(float value, uint8_t width, uint8_t precision, char* outBuf)
{
    dtostrf(value, width, precision, outBuf);
    return outBuf;
}

static uint16_t clampU16(uint16_t value, uint16_t minVal, uint16_t maxVal)
{
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

static void loadDefaultConfig(Lab3Config* cfg)
{
    cfg->thresholdC   = LAB3_DEFAULT_THRESHOLD_C;
    cfg->hysteresisC  = LAB3_DEFAULT_HYSTERESIS_C;
    cfg->alpha        = LAB3_DEFAULT_ALPHA;
    cfg->sampleMs     = LAB3_DEFAULT_SAMPLE_MS;
    cfg->persistenceMs = LAB3_DEFAULT_PERSIST_MS;
    cfg->reportMs     = LAB3_DEFAULT_REPORT_MS;
}

static void readConfigFromStdio(Lab3Config* cfg)
{
    printf("\\n[Lab3] Enter config: <thresholdC> <hysteresisC> <sampleMs> <persistMs> <reportMs> <alpha>\\n");
    printf("[Lab3] Example: 25 1 50 100 1000 0.25\\n");
    printf("[Lab3] Waiting 5s for serial input, then defaults are used.\\n> ");

    float threshold = cfg->thresholdC;
    float hysteresis = cfg->hysteresisC;
    int sampleMs = cfg->sampleMs;
    int persistMs = cfg->persistenceMs;
    int reportMs = cfg->reportMs;
    float alpha = cfg->alpha;

    char lineBuf[96];
    int parsed = 0;
    if (readLineWithTimeout(lineBuf, sizeof(lineBuf), 5000UL))
    {
        parsed = sscanf(lineBuf, "%f %f %d %d %d %f", &threshold, &hysteresis, &sampleMs, &persistMs, &reportMs, &alpha);
    }

    if (parsed == 6)
    {
        cfg->thresholdC = threshold;
        cfg->hysteresisC = (hysteresis < 0.1f) ? 0.1f : hysteresis;
        cfg->alpha = alpha;
        if (cfg->alpha < 0.01f) cfg->alpha = 0.01f;
        if (cfg->alpha > 1.0f) cfg->alpha = 1.0f;
        cfg->sampleMs = clampU16((uint16_t)sampleMs, LAB3_MIN_SAMPLE_MS, LAB3_MAX_SAMPLE_MS);
        cfg->persistenceMs = (persistMs < (int)cfg->sampleMs) ? cfg->sampleMs : (uint16_t)persistMs;
        cfg->reportMs = clampU16((uint16_t)reportMs, LAB3_MIN_REPORT_MS, LAB3_MAX_REPORT_MS);
        printf("[Lab3] Custom config accepted.\\n");
    }
    else
    {
        printf("[Lab3] Using default config (no/invalid input).\\n");
    }
}

void lab3_setup()
{
    loadDefaultConfig(&g_lab3.config);
    readConfigFromStdio(&g_lab3.config);

    s_ntc.Init();
    s_alert.Init();
    Wire.begin();
    s_lcd.init();
    s_lcd.backlight();
    fdev_setup_stream(&s_lcdStream, lcd_putchar, NULL, _FDEV_SETUP_WRITE);
    lcd_printf_lines("Lab3 Starting", "Please wait...");

    ConditioningConfig ccfg;
    ccfg.thresholdC = g_lab3.config.thresholdC;
    ccfg.hysteresisC = g_lab3.config.hysteresisC;
    ccfg.alpha = g_lab3.config.alpha;
    ccfg.minTempC = -40.0f;
    ccfg.maxTempC = 125.0f;

    uint16_t persistenceSamples = g_lab3.config.persistenceMs / g_lab3.config.sampleMs;
    if (persistenceSamples == 0u)
    {
        persistenceSamples = 1u;
    }
    ccfg.persistenceSamples = persistenceSamples;

    s_conditioner.Configure(ccfg);

    g_lab3.rawQueue = xQueueCreate(1, sizeof(Lab3RawSample));
    g_lab3.alertSignal = xSemaphoreCreateBinary();
    g_lab3.stateMutex = xSemaphoreCreateMutex();
    g_lab3.ioMutex = xSemaphoreCreateMutex();

    if (!g_lab3.rawQueue || !g_lab3.alertSignal || !g_lab3.stateMutex || !g_lab3.ioMutex)
    {
        printf("[Lab3] FATAL: RTOS object creation failed.\\n");
        for (;;) { }
    }

    g_lab3.state.sampleIndex = 0;
    g_lab3.state.alertTransitions = 0;
    g_lab3.state.persistenceSamples = persistenceSamples;
    g_lab3.state.alertLedState = false;

    if (xSemaphoreTake(g_lab3.ioMutex, portMAX_DELAY) == pdTRUE)
    {
            char thrBuf[16], hystBuf[16], alphaBuf[16];
        printf("[Lab3] FreeRTOS sensor pipeline start\\n");
            printf("[Lab3] cfg: thr=%sC hyst=%sC sample=%ums persist=%ums(%u samples) report=%ums alpha=%s\n",
             fmtFloat(g_lab3.config.thresholdC, 0, 2, thrBuf),
             fmtFloat(g_lab3.config.hysteresisC, 0, 2, hystBuf),
               g_lab3.config.sampleMs,
               g_lab3.config.persistenceMs,
               persistenceSamples,
               g_lab3.config.reportMs,
             fmtFloat(g_lab3.config.alpha, 0, 2, alphaBuf));
        xSemaphoreGive(g_lab3.ioMutex);
    }

    BaseType_t ok;
    ok = xTaskCreate(taskAcquisition, "L3_Acq", 256, (void*)0, 3, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3] FAIL: taskAcquisition\\n"); for(;;){} }

    ok = xTaskCreate(taskConditioning, "L3_Cond", 320, (void*)0, 2, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3] FAIL: taskConditioning\\n"); for(;;){} }

    ok = xTaskCreate(taskAlerting, "L3_Alert", 256, (void*)0, 2, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3] FAIL: taskAlerting\\n"); for(;;){} }

    ok = xTaskCreate(taskDisplay, "L3_Disp", 768, (void*)0, 1, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3] FAIL: taskDisplay\\n"); for(;;){} }

    ok = xTaskCreate(taskLcdDisplay, "L3_LCD", 512, (void*)0, 1, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3] FAIL: taskLcdDisplay\\n"); for(;;){} }
}

void lab3_loop()
{
}

static void taskAcquisition(void *pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        Lab3RawSample sample;
        sample.adcRaw = s_ntc.ReadRaw();
        sample.voltage = s_ntc.RawToVoltage(sample.adcRaw);
        sample.tempC = s_ntc.RawToTemperatureC(sample.adcRaw);
        sample.timestamp = xTaskGetTickCount();

        xQueueOverwrite(g_lab3.rawQueue, &sample);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab3.config.sampleMs));
    }
}

static void taskConditioning(void *pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        Lab3RawSample sample;
        if (xQueueReceive(g_lab3.rawQueue, &sample, 0) == pdTRUE)
        {
            ConditioningResult c = s_conditioner.Process(sample.tempC);

            if (xSemaphoreTake(g_lab3.stateMutex, MUTEX_TIMEOUT_TICKS) == pdTRUE)
            {
                g_lab3.state.sampleIndex++;
                g_lab3.state.rawAdc = sample.adcRaw;
                g_lab3.state.rawVoltage = sample.voltage;
                g_lab3.state.rawTempC = sample.tempC;
                g_lab3.state.clampedTempC = c.clampedTempC;
                g_lab3.state.filteredTempC = c.filteredTempC;
                g_lab3.state.hysteresisCandidate = c.hysteresisCandidate;
                g_lab3.state.debouncedAlert = c.debouncedAlert;
                g_lab3.state.persistenceCounter = c.pendingCount;
                g_lab3.state.timestamp = sample.timestamp;
                xSemaphoreGive(g_lab3.stateMutex);
            }

            xSemaphoreGive(g_lab3.alertSignal);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(LAB3_CONDITION_TASK_MS));
    }
}

static void taskAlerting(void *pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        if (xSemaphoreTake(g_lab3.alertSignal, 0) == pdTRUE)
        {
            float filteredTemp = 0.0f;

            if (xSemaphoreTake(g_lab3.stateMutex, MUTEX_TIMEOUT_TICKS) == pdTRUE)
            {
                filteredTemp = g_lab3.state.filteredTempC;
                xSemaphoreGive(g_lab3.stateMutex);
            }

            static TickType_t lastBlinkTick = 0;
            static bool blinkState = false;
            bool desiredLedState = false;

            if (filteredTemp >= LAB3_LED_BLINK_C)
            {
                TickType_t now = xTaskGetTickCount();
                if ((now - lastBlinkTick) >= pdMS_TO_TICKS(LAB3_LED_BLINK_MS))
                {
                    blinkState = !blinkState;
                    lastBlinkTick = now;
                }
                desiredLedState = blinkState;
            }
            else if (filteredTemp >= LAB3_LED_ON_C)
            {
                blinkState = true;
                desiredLedState = true;
            }
            else
            {
                blinkState = false;
                desiredLedState = false;
            }

            const bool prevLedState = s_alert.IsActive();
            s_alert.ApplyState(desiredLedState);

            if (xSemaphoreTake(g_lab3.stateMutex, MUTEX_TIMEOUT_TICKS) == pdTRUE)
            {
                g_lab3.state.alertLedState = s_alert.IsActive();
                if (prevLedState != g_lab3.state.alertLedState)
                {
                    g_lab3.state.alertTransitions++;
                }
                xSemaphoreGive(g_lab3.stateMutex);
            }
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(LAB3_ALERT_TASK_MS));
    }
}

static void taskDisplay(void *pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        Lab3RuntimeState snap;

        if (xSemaphoreTake(g_lab3.stateMutex, MUTEX_TIMEOUT_TICKS) == pdTRUE)
        {
            snap = g_lab3.state;
            xSemaphoreGive(g_lab3.stateMutex);
        }
        else
        {
            vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab3.config.reportMs));
            continue;
        }

        if (xSemaphoreTake(g_lab3.ioMutex, MUTEX_TIMEOUT_TICKS) == pdTRUE)
        {
               char plotRawBuf[16], plotFiltBuf[16], plotHighBuf[16], plotLowBuf[16], plotAdcBuf[16];
               char filtViewBuf[16], rawViewBuf[16], clampViewBuf[16];
               const char* ledMode = (snap.filteredTempC >= LAB3_LED_BLINK_C) ? "BLINK" : ((snap.filteredTempC >= LAB3_LED_ON_C) ? "ON" : "OFF");
               const char* stateName = snap.debouncedAlert ? "ALERT" : "NORMAL";

             const float highThreshold = g_lab3.config.thresholdC + g_lab3.config.hysteresisC;
             const float lowThreshold = g_lab3.config.thresholdC - g_lab3.config.hysteresisC;

                 printf("[L3] i=%lu raw=%sC clamp=%sC filt=%sC st=%s mode=%s led=%u p=%u/%u tr=%lu\r\n",
                      (unsigned long)snap.sampleIndex,
                      fmtFloat(snap.rawTempC, 0, 2, rawViewBuf),
                      fmtFloat(snap.clampedTempC, 0, 2, clampViewBuf),
                      fmtFloat(snap.filteredTempC, 0, 2, filtViewBuf),
                      stateName,
                      ledMode,
                      snap.alertLedState ? 1u : 0u,
                      snap.persistenceCounter,
                      snap.persistenceSamples,
                      (unsigned long)snap.alertTransitions);

                         printf("PLOT,%s,%s,%s,%s,%s,%u\n",
                 fmtFloat(snap.rawTempC, 0, 3, plotRawBuf),
                 fmtFloat(snap.filteredTempC, 0, 3, plotFiltBuf),
                 fmtFloat(highThreshold, 0, 3, plotHighBuf),
                 fmtFloat(lowThreshold, 0, 3, plotLowBuf),
                 fmtFloat((float)snap.rawAdc, 0, 3, plotAdcBuf),
                                 snap.alertLedState ? 1u : 0u);

            xSemaphoreGive(g_lab3.ioMutex);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab3.config.reportMs));
    }
}

static void taskLcdDisplay(void *pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        Lab3RuntimeState snap;
        if (xSemaphoreTake(g_lab3.stateMutex, MUTEX_TIMEOUT_TICKS) == pdTRUE)
        {
            snap = g_lab3.state;
            xSemaphoreGive(g_lab3.stateMutex);
        }
        else
        {
            vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(LAB3_LCD_TASK_MS));
            continue;
        }

        {
            char line1[17] = {0};
            char line2[17] = {0};
            char tfBuf[8] = {0};
            const char* mode = "OFF";
            if (snap.filteredTempC >= LAB3_LED_BLINK_C)
            {
                mode = "BLINK";
            }
            else if (snap.filteredTempC >= LAB3_LED_ON_C)
            {
                mode = "ON";
            }

            snprintf(line1, sizeof(line1), "LED:%s T:%s", mode, fmtFloat(snap.filteredTempC, 0, 1, tfBuf));
            if (snap.filteredTempC >= LAB3_LED_BLINK_C)
            {
                snprintf(line2, sizeof(line2), "Blink > 30.0C");
            }
            else if (snap.filteredTempC >= LAB3_LED_ON_C)
            {
                snprintf(line2, sizeof(line2), "On >= 20.0C");
            }
            else
            {
                snprintf(line2, sizeof(line2), "Below 20.0C");
            }

            lcd_printf_lines(line1, line2);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(LAB3_LCD_TASK_MS));
    }
}
