#include "Lab3_main.h"
#include "Lab3_Shared.h"
#include "SignalConditioning.h"
#include "AlertManager.h"
#include "../sensor/NtcAdcDriver.h"
#include "../sensor/DhtSensorDriver.h"

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
static DhtSensorDriver   s_dht(LAB3_DHT_PIN);
static SignalConditioner s_conditioner;
static AlertManager      s_alert(LAB3_ALERT_LED_PIN);
static AlertManager      s_blueAlert(LAB3_BLUE_LED_PIN);
static LiquidCrystal_I2C s_lcd(LAB3_LCD_I2C_ADDR, 16, 2);
static FILE s_lcdStream;
static uint8_t s_lcdCol = 0u;
static uint8_t s_lcdRow = 0u;
static ConditioningConfig s_ccfg;

static void taskAcquisition(void *pv);
static void taskConditioning(void *pv);
static void taskAlerting(void *pv);
static void taskDisplay(void *pv);
static void taskLcdDisplay(void *pv);

static const TickType_t MUTEX_TIMEOUT_TICKS = pdMS_TO_TICKS(20);
static const TickType_t DHT_MIN_REFRESH_TICKS = pdMS_TO_TICKS(2000);

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
    BaseType_t ioLocked = pdFALSE;
    if ((g_lab3.ioMutex != NULL) && (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED))
    {
        ioLocked = xSemaphoreTake(g_lab3.ioMutex, pdMS_TO_TICKS(20));
    }

    s_lcd.clear();
    s_lcdRow = 0u;
    s_lcdCol = 0u;
    s_lcd.setCursor(0, 0);
    fprintf(&s_lcdStream, "%-16.16s\n%-16.16s", line1 ? line1 : "", line2 ? line2 : "");

    if (ioLocked == pdTRUE)
    {
        xSemaphoreGive(g_lab3.ioMutex);
    }
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
    printf("[Lab3] Enter values now using scanf format (or invalid values for defaults).\\n> ");

    float threshold = cfg->thresholdC;
    float hysteresis = cfg->hysteresisC;
    int sampleMs = cfg->sampleMs;
    int persistMs = cfg->persistenceMs;
    int reportMs = cfg->reportMs;
    float alpha = cfg->alpha;

    const int parsed = scanf("%f %f %d %d %d %f", &threshold, &hysteresis, &sampleMs, &persistMs, &reportMs, &alpha);

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
    s_dht.Init();
    s_alert.Init();
    s_blueAlert.Init();
    Wire.begin();
    s_lcd.init();
    s_lcd.backlight();
    fdev_setup_stream(&s_lcdStream, lcd_putchar, NULL, _FDEV_SETUP_WRITE);
    lcd_printf_lines("Lab3 Starting", "Please wait...");

    s_ccfg.thresholdC = g_lab3.config.thresholdC;
    s_ccfg.hysteresisC = g_lab3.config.hysteresisC;
    s_ccfg.alpha = g_lab3.config.alpha;
    s_ccfg.minTempC = -40.0f;
    s_ccfg.maxTempC = 125.0f;

    uint16_t persistenceSamples = g_lab3.config.persistenceMs / g_lab3.config.sampleMs;
    if (persistenceSamples == 0u)
    {
        persistenceSamples = 1u;
    }
    s_ccfg.persistenceSamples = persistenceSamples;

    s_conditioner.Configure(s_ccfg);

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
    g_lab3.state.dhtTempC = 0.0f;
    g_lab3.state.dhtHumidityPct = 0.0f;
    g_lab3.state.dhtDataValid = false;
    g_lab3.state.sensorDataValid = false;
    g_lab3.state.requestedSource = LAB3_SENSOR_SOURCE_NTC;
    g_lab3.state.activeSource = LAB3_SENSOR_SOURCE_NTC;

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
                printf("[Lab3] LCD mode: row1=NTC, row2=DHT22\n");
        xSemaphoreGive(g_lab3.ioMutex);
    }

    BaseType_t ok;
    ok = xTaskCreate(taskAcquisition, "L3_Acq", 512, (void*)0, 3, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3] FAIL: taskAcquisition\\n"); for(;;){} }

    ok = xTaskCreate(taskConditioning, "L3_Cond", 320, (void*)0, 2, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3] FAIL: taskConditioning\\n"); for(;;){} }

    ok = xTaskCreate(taskAlerting, "L3_Alert", 256, (void*)0, 2, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3] FAIL: taskAlerting\\n"); for(;;){} }

    ok = xTaskCreate(taskDisplay, "L3_Disp", 768, (void*)0, 1, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3] FAIL: taskDisplay\\n"); for(;;){} }

    ok = xTaskCreate(taskLcdDisplay, "L3_LCD", 512, (void*)0, 1, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3] FAIL: taskLcdDisplay\\n"); for(;;){} }

    // Source auto-switch is disabled in dual-row LCD mode.
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
        sample.source = LAB3_SENSOR_SOURCE_NTC;
        sample.humidityPct = 0.0f;
        sample.valid = true;
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
    Lab3SensorSource lastSource = LAB3_SENSOR_SOURCE_NTC;

    for (;;)
    {
        Lab3RawSample sample;
        if (xQueueReceive(g_lab3.rawQueue, &sample, 0) == pdTRUE)
        {
            if (sample.source != lastSource)
            {
                s_conditioner.Configure(s_ccfg);
                lastSource = sample.source;
            }

            if (xSemaphoreTake(g_lab3.stateMutex, MUTEX_TIMEOUT_TICKS) == pdTRUE)
            {
                g_lab3.state.sampleIndex++;
                g_lab3.state.rawAdc = sample.adcRaw;
                g_lab3.state.rawVoltage = sample.voltage;
                g_lab3.state.rawTempC = sample.tempC;
                g_lab3.state.rawHumidityPct = sample.humidityPct;
                g_lab3.state.sensorDataValid = sample.valid;
                g_lab3.state.activeSource = sample.source;
                g_lab3.state.timestamp = sample.timestamp;

                if (sample.valid)
                {
                    ConditioningResult c = s_conditioner.Process(sample.tempC);
                    g_lab3.state.clampedTempC = c.clampedTempC;
                    g_lab3.state.filteredTempC = c.filteredTempC;
                    g_lab3.state.hysteresisCandidate = c.hysteresisCandidate;
                    g_lab3.state.debouncedAlert = c.debouncedAlert;
                    g_lab3.state.persistenceCounter = c.pendingCount;
                }
                xSemaphoreGive(g_lab3.stateMutex);
            }

            if (sample.valid)
            {
                xSemaphoreGive(g_lab3.alertSignal);
            }
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(LAB3_CONDITION_TASK_MS));
    }
}

static void taskAlerting(void *pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();
    static TickType_t redLastBlinkTick = 0;
    static bool redBlinkState = false;
    static TickType_t blueLastBlinkTick = 0;
    static bool blueBlinkState = false;

    for (;;)
    {
        float ntcTemp = 0.0f;
        bool ntcValid = false;
        float dhtTemp = 0.0f;
        bool dhtValid = false;

        if (xSemaphoreTake(g_lab3.stateMutex, MUTEX_TIMEOUT_TICKS) == pdTRUE)
        {
            ntcTemp = g_lab3.state.filteredTempC;
            ntcValid = g_lab3.state.sensorDataValid;
            dhtTemp = g_lab3.state.dhtTempC;
            dhtValid = g_lab3.state.dhtDataValid;
            xSemaphoreGive(g_lab3.stateMutex);
        }

        bool desiredRedLedState = false;

        if (!ntcValid)
        {
            redBlinkState = false;
            desiredRedLedState = false;
        }
        else if (ntcTemp >= LAB3_LED_BLINK_C)
        {
            TickType_t now = xTaskGetTickCount();
            if ((now - redLastBlinkTick) >= pdMS_TO_TICKS(LAB3_LED_BLINK_MS))
            {
                redBlinkState = !redBlinkState;
                redLastBlinkTick = now;
            }
            desiredRedLedState = redBlinkState;
        }
        else if (ntcTemp >= LAB3_LED_ON_C)
        {
            redBlinkState = true;
            desiredRedLedState = true;
        }
        else
        {
            redBlinkState = false;
            desiredRedLedState = false;
        }

        s_alert.ApplyState(desiredRedLedState);

        bool desiredLedState = false;

        if (!dhtValid)
        {
            blueBlinkState = false;
            desiredLedState = false;
        }
        else if (dhtTemp >= LAB3_LED_BLINK_C)
        {
            TickType_t now = xTaskGetTickCount();
            if ((now - blueLastBlinkTick) >= pdMS_TO_TICKS(LAB3_LED_BLINK_MS))
            {
                blueBlinkState = !blueBlinkState;
                blueLastBlinkTick = now;
            }
            desiredLedState = blueBlinkState;
        }
        else if (dhtTemp >= LAB3_LED_ON_C)
        {
            blueBlinkState = true;
            desiredLedState = true;
        }
        else
        {
            blueBlinkState = false;
            desiredLedState = false;
        }

        const bool prevLedState = s_blueAlert.IsActive();
        s_blueAlert.ApplyState(desiredLedState);

        if (xSemaphoreTake(g_lab3.stateMutex, MUTEX_TIMEOUT_TICKS) == pdTRUE)
        {
            g_lab3.state.alertLedState = s_blueAlert.IsActive();
            if (prevLedState != g_lab3.state.alertLedState)
            {
                g_lab3.state.alertTransitions++;
            }
            xSemaphoreGive(g_lab3.stateMutex);
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
            char filtViewBuf[16], rawViewBuf[16], clampViewBuf[16], dhtTViewBuf[16], dhtHViewBuf[16];
            const char* ntcLedMode = (!snap.sensorDataValid) ? "OFF" : ((snap.filteredTempC >= LAB3_LED_BLINK_C) ? "BLINK" : ((snap.filteredTempC >= LAB3_LED_ON_C) ? "ON" : "OFF"));
            const char* dhtLedMode = (!snap.dhtDataValid) ? "OFF" : ((snap.dhtTempC >= LAB3_LED_BLINK_C) ? "BLINK" : ((snap.dhtTempC >= LAB3_LED_ON_C) ? "ON" : "OFF"));
               const char* stateName = snap.debouncedAlert ? "ALERT" : "NORMAL";

            printf("[L3][NTC] i=%lu v=%s raw=%sC adc=%u clamp=%sC filt=%sC st=%s mode=%s p=%u/%u tr=%lu\r\n",
                   (unsigned long)snap.sampleIndex,
                   snap.sensorDataValid ? "OK" : "WAIT",
                   fmtFloat(snap.rawTempC, 0, 2, rawViewBuf),
                   snap.rawAdc,
                   fmtFloat(snap.clampedTempC, 0, 2, clampViewBuf),
                   fmtFloat(snap.filteredTempC, 0, 2, filtViewBuf),
                   stateName,
                   ntcLedMode,
                   snap.persistenceCounter,
                   snap.persistenceSamples,
                   (unsigned long)snap.alertTransitions);

            printf("[L3][DHT] i=%lu v=%s t=%sC h=%s%% mode=%s led=%u\r\n",
                   (unsigned long)snap.sampleIndex,
                   snap.dhtDataValid ? "OK" : "WAIT",
                   fmtFloat(snap.dhtTempC, 0, 2, dhtTViewBuf),
                   fmtFloat(snap.dhtHumidityPct, 0, 1, dhtHViewBuf),
                   dhtLedMode,
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
    TickType_t lastDhtReadTick = 0;
    float dhtTempC = 0.0f;
    float dhtHumidity = 0.0f;
    bool dhtValid = false;

    for (;;)
    {
        const TickType_t nowTick = xTaskGetTickCount();
        if ((nowTick - lastDhtReadTick) >= DHT_MIN_REFRESH_TICKS)
        {
            float t = 0.0f;
            float h = 0.0f;
            dhtValid = s_dht.Read(&t, &h);
            if (dhtValid)
            {
                dhtTempC = t;
                dhtHumidity = h;
            }
            lastDhtReadTick = nowTick;

            if (xSemaphoreTake(g_lab3.stateMutex, MUTEX_TIMEOUT_TICKS) == pdTRUE)
            {
                g_lab3.state.dhtDataValid = dhtValid;
                if (dhtValid)
                {
                    g_lab3.state.dhtTempC = dhtTempC;
                    g_lab3.state.dhtHumidityPct = dhtHumidity;
                }
                xSemaphoreGive(g_lab3.stateMutex);
            }
        }

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
            char ntcBuf[8] = {0};
            char dhtTBuf[8] = {0};
            char dhtHBuf[8] = {0};
            const char* mode = "OFF";

            if (snap.dhtDataValid && snap.dhtTempC >= LAB3_LED_BLINK_C)
            {
                mode = "BLINK";
            }
            else if (snap.dhtDataValid && snap.dhtTempC >= LAB3_LED_ON_C)
            {
                mode = "ON";
            }

            snprintf(line1, sizeof(line1), "NTC %s T:%s", mode, fmtFloat(snap.filteredTempC, 0, 1, ntcBuf));

            if (dhtValid)
            {
                snprintf(line2, sizeof(line2), "DHT T:%s H:%s", fmtFloat(dhtTempC, 0, 1, dhtTBuf), fmtFloat(dhtHumidity, 0, 0, dhtHBuf));
            }
            else
            {
                snprintf(line2, sizeof(line2), "DHT WAITING...");
            }

            lcd_printf_lines(line1, line2);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(LAB3_LCD_TASK_MS));
    }
}

