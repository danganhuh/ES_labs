#include "Lab3_2_main.h"
#include "Lab3_2_Shared.h"
#include "SignalConditioning32.h"

#include "../Lab3/AlertManager.h"
#include "../sensor/NtcAdcDriver.h"
#include "../sensor/DhtSensorDriver.h"
#include "../sensor/UltrasonicDriver.h"

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef portMAX_DELAY
#define portMAX_DELAY ((TickType_t)0xffff)
#endif

Lab32Shared g_lab32;

static NtcAdcDriver s_ntc(LAB32_NTC_PIN);
static DhtSensorDriver s_dht(LAB32_DHT_PIN);
static UltrasonicDriver s_us(LAB32_US_TRIG_PIN, LAB32_US_ECHO_PIN);

static Lab32SignalConditioner s_ntcConditioner;
static Lab32SignalConditioner s_dhtConditioner;
static Lab32SignalConditioner s_usConditioner;

static AlertManager s_ntcLed(LAB32_LED_NTC_PIN);
static AlertManager s_dhtLed(LAB32_LED_DHT_PIN);
static AlertManager s_usLed(LAB32_LED_US_PIN);

static LiquidCrystal_I2C s_lcd(LAB32_LCD_I2C_ADDR, 16, 2);
static FILE s_lcdStream;
static uint8_t s_lcdCol = 0u;
static uint8_t s_lcdRow = 0u;

static TickType_t s_lastDhtReadTick = 0;
static TickType_t s_lastUsReadTick = 0;
static uint32_t s_lastNtcSeq = 0;
static uint32_t s_lastDhtSeq = 0;
static uint32_t s_lastUsSeq = 0;
static bool s_ntcDebouncedAlert = false;
static bool s_ntcCandidateAlert = false;
static uint16_t s_ntcPending = 0u;
static bool s_dhtDebouncedAlert = false;
static bool s_dhtCandidateAlert = false;
static uint16_t s_dhtPending = 0u;
static bool s_usDebouncedAlert = false;
static bool s_usCandidateAlert = false;
static uint16_t s_usPending = 0u;

static void taskAcquisition(void *pv);
static void taskConditioning(void *pv);
static void taskAlerting(void *pv);
static void taskDisplay(void *pv);
static void taskLcdDisplay(void *pv);

static uint16_t clampU16(uint16_t value, uint16_t minV, uint16_t maxV)
{
    if (value < minV) return minV;
    if (value > maxV) return maxV;
    return value;
}

static const char* fmtFloat(float value, uint8_t width, uint8_t precision, char* outBuf)
{
    dtostrf(value, width, precision, outBuf);
    return outBuf;
}

static int lcd_putchar(char c, FILE *stream)
{
    (void)stream;

    if (c == '\r') return 0;
    if (c == '\n')
    {
        s_lcdRow = static_cast<uint8_t>((s_lcdRow + 1u) % 2u);
        s_lcdCol = 0u;
        s_lcd.setCursor(0, s_lcdRow);
        return 0;
    }

    if (s_lcdCol >= 16u)
    {
        s_lcdRow = static_cast<uint8_t>((s_lcdRow + 1u) % 2u);
        s_lcdCol = 0u;
        s_lcd.setCursor(0, s_lcdRow);
    }

    s_lcd.write(c);
    s_lcdCol++;
    return 0;
}

static void lcd_printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(&s_lcdStream, fmt, args);
    va_end(args);
}

static void lcd_printf_lines(const char* line1, const char* line2)
{
    s_lcd.clear();
    s_lcdRow = 0u;
    s_lcdCol = 0u;
    s_lcd.setCursor(0, 0);
    lcd_printf("%-16.16s\n%-16.16s", line1 ? line1 : "", line2 ? line2 : "");
}

static void loadDefaultConfig(Lab32Config* cfg)
{
    cfg->sampleMs = LAB32_DEFAULT_SAMPLE_MS;
    cfg->conditionMs = LAB32_DEFAULT_CONDITION_MS;
    cfg->reportMs = LAB32_DEFAULT_REPORT_MS;
    cfg->lcdMs = LAB32_DEFAULT_LCD_MS;
    cfg->dhtMs = 500u;
    cfg->usMs = LAB32_DEFAULT_US_MS;
    cfg->alpha = LAB32_DEFAULT_ALPHA;
    cfg->persistenceSamples = LAB32_DEFAULT_PERSIST_SAMPLES;
    cfg->ntcThrC = LAB32_DEFAULT_NTC_THR_C;
    cfg->ntcHystC = LAB32_DEFAULT_NTC_HYST_C;
    cfg->dhtThrC = LAB32_DEFAULT_DHT_THR_C;
    cfg->dhtHystC = LAB32_DEFAULT_DHT_HYST_C;
    cfg->usAlertCm = LAB32_DEFAULT_US_ALERT_CM;
    cfg->usHystCm = LAB32_DEFAULT_US_HYST_CM;
}

static void readConfigFromStdio(Lab32Config* cfg)
{
    printf("\n[Lab3_2] Enter config:\n");
    printf(" sampleMs reportMs dhtMs usMs alpha persist ntcThr ntcHyst dhtThr dhtHyst usAlertCm usHystCm\n");
    printf(" Example: 50 500 500 100 0.30 2 27 1 30 1 40 4\n> ");

    int sampleMs = cfg->sampleMs;
    int reportMs = cfg->reportMs;
    int dhtMs = cfg->dhtMs;
    int usMs = cfg->usMs;
    float alpha = cfg->alpha;
    int persist = cfg->persistenceSamples;
    float ntcThr = cfg->ntcThrC;
    float ntcHyst = cfg->ntcHystC;
    float dhtThr = cfg->dhtThrC;
    float dhtHyst = cfg->dhtHystC;
    float usAlert = cfg->usAlertCm;
    float usHyst = cfg->usHystCm;

    const int parsed = scanf("%d %d %d %d %f %d %f %f %f %f %f %f",
                             &sampleMs, &reportMs, &dhtMs, &usMs,
                             &alpha, &persist,
                             &ntcThr, &ntcHyst, &dhtThr, &dhtHyst,
                             &usAlert, &usHyst);

    if (parsed == 12)
    {
        cfg->sampleMs = clampU16(static_cast<uint16_t>(sampleMs), LAB32_MIN_SAMPLE_MS, LAB32_MAX_SAMPLE_MS);
        cfg->conditionMs = cfg->sampleMs;
        cfg->reportMs = (reportMs < 200) ? 200u : static_cast<uint16_t>(reportMs);
        cfg->lcdMs = (cfg->reportMs < 300u) ? 300u : cfg->reportMs;
        cfg->dhtMs = (dhtMs < 400) ? 400u : static_cast<uint16_t>(dhtMs);
        cfg->usMs = (usMs < 50) ? 50u : static_cast<uint16_t>(usMs);
        cfg->alpha = (alpha < 0.01f) ? 0.01f : ((alpha > 1.0f) ? 1.0f : alpha);
        cfg->persistenceSamples = (persist < 1) ? 1u : static_cast<uint16_t>(persist);
        cfg->ntcThrC = ntcThr;
        cfg->ntcHystC = (ntcHyst < 0.1f) ? 0.1f : ntcHyst;
        cfg->dhtThrC = dhtThr;
        cfg->dhtHystC = (dhtHyst < 0.1f) ? 0.1f : dhtHyst;
        cfg->usAlertCm = (usAlert < 5.0f) ? 5.0f : usAlert;
        cfg->usHystCm = (usHyst < 1.0f) ? 1.0f : usHyst;
        printf("[Lab3_2] Custom config accepted.\n");
    }
    else
    {
        printf("[Lab3_2] Using default config.\n");
    }
}

static bool sensor_read_ntc(uint16_t* outAdc, float* outVoltage, float* outTempC)
{
    if ((outAdc == NULL) || (outVoltage == NULL) || (outTempC == NULL))
    {
        return false;
    }

    *outAdc = s_ntc.ReadRaw();
    *outVoltage = s_ntc.RawToVoltage(*outAdc);
    *outTempC = s_ntc.RawToTemperatureC(*outAdc);
    return true;
}

static bool sensor_read_dht(float* outTempC, float* outHumidityPct)
{
    return s_dht.Read(outTempC, outHumidityPct);
}

static bool sensor_read_ultrasonic(float* outDistanceCm)
{
    return s_us.ReadDistanceCm(outDistanceCm);
}

void lab3_2_setup()
{
    loadDefaultConfig(&g_lab32.config);
    readConfigFromStdio(&g_lab32.config);

    s_ntc.Init();
    s_dht.Init();
    s_us.Init();
    s_ntcLed.Init();
    s_dhtLed.Init();
    s_usLed.Init();
    s_ntcLed.ApplyState(false);
    s_dhtLed.ApplyState(false);
    s_usLed.ApplyState(false);

    Wire.begin();
    s_lcd.init();
    s_lcd.backlight();
    fdev_setup_stream(&s_lcdStream, lcd_putchar, NULL, _FDEV_SETUP_WRITE);
    lcd_printf_lines("Lab3_2 Start", "Config loaded");

    g_lab32.stateMutex = xSemaphoreCreateMutex();
    g_lab32.ioMutex = xSemaphoreCreateMutex();
    if ((g_lab32.stateMutex == NULL) || (g_lab32.ioMutex == NULL))
    {
        printf("[Lab3_2] FATAL: mutex creation failed.\n");
        for (;;) {}
    }

    g_lab32.state.sampleIndex = 0;
    g_lab32.state.ntcValid = false;
    g_lab32.state.dhtValid = false;
    g_lab32.state.usValid = false;
    g_lab32.state.status = LAB32_STATUS_SENSOR_FAULT;

    s_lastDhtReadTick = xTaskGetTickCount() - pdMS_TO_TICKS(g_lab32.config.dhtMs);
    s_lastUsReadTick = xTaskGetTickCount() - pdMS_TO_TICKS(g_lab32.config.usMs);

    Lab32ConditioningConfig ntcCfg{};
    ntcCfg.alpha = g_lab32.config.alpha;
    ntcCfg.minValue = -40.0f;
    ntcCfg.maxValue = 125.0f;
    s_ntcConditioner.Configure(ntcCfg);

    Lab32ConditioningConfig dhtCfg{};
    dhtCfg.alpha = g_lab32.config.alpha;
    dhtCfg.minValue = -40.0f;
    dhtCfg.maxValue = 85.0f;
    s_dhtConditioner.Configure(dhtCfg);

    Lab32ConditioningConfig usCfg{};
    usCfg.alpha = g_lab32.config.alpha;
    usCfg.minValue = 2.0f;
    usCfg.maxValue = 400.0f;
    s_usConditioner.Configure(usCfg);

    printf("[Lab3_2] FreeRTOS monitoring start\n");

    BaseType_t ok;
    ok = xTaskCreate(taskAcquisition, "L32_Acq", 768, (void*)0, 3, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3_2] FAIL taskAcquisition\n"); for (;;) {} }
    ok = xTaskCreate(taskConditioning, "L32_Cond", 768, (void*)0, 2, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3_2] FAIL taskConditioning\n"); for (;;) {} }
    ok = xTaskCreate(taskAlerting, "L32_Alert", 320, (void*)0, 2, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3_2] FAIL taskAlerting\n"); for (;;) {} }
    ok = xTaskCreate(taskDisplay, "L32_Disp", 896, (void*)0, 1, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3_2] FAIL taskDisplay\n"); for (;;) {} }
    ok = xTaskCreate(taskLcdDisplay, "L32_LCD", 512, (void*)0, 1, (TaskHandle_t*)0);
    if (ok != pdPASS) { printf("[Lab3_2] FAIL taskLcdDisplay\n"); for (;;) {} }
}

void lab3_2_loop()
{
}

static void taskAcquisition(void *pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        uint16_t adc = 0u;
        float voltage = 0.0f;
        float ntcTemp = 0.0f;
        const bool ntcOk = sensor_read_ntc(&adc, &voltage, &ntcTemp);

        float dhtTemp = 0.0f;
        float dhtHum = 0.0f;
        bool dhtUpdated = false;
        bool dhtOk = false;

        const TickType_t nowTick = xTaskGetTickCount();
        if ((nowTick - s_lastDhtReadTick) >= pdMS_TO_TICKS(g_lab32.config.dhtMs))
        {
            dhtOk = sensor_read_dht(&dhtTemp, &dhtHum);
            dhtUpdated = true;
            s_lastDhtReadTick = nowTick;
        }

        float usCm = 0.0f;
        bool usUpdated = false;
        bool usOk = false;
        if ((nowTick - s_lastUsReadTick) >= pdMS_TO_TICKS(g_lab32.config.usMs))
        {
            usOk = sensor_read_ultrasonic(&usCm);
            usUpdated = true;
            s_lastUsReadTick = nowTick;
        }

        if (xSemaphoreTake(g_lab32.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            g_lab32.state.sampleIndex++;
            g_lab32.state.timestamp = nowTick;

            g_lab32.state.ntcRawAdc = adc;
            g_lab32.state.ntcRawVoltage = voltage;
            g_lab32.state.ntcRawTempC = ntcTemp;
            g_lab32.state.ntcValid = ntcOk;
            g_lab32.state.ntcSeq++;

            if (dhtUpdated)
            {
                g_lab32.state.dhtValid = dhtOk;
                if (dhtOk)
                {
                    g_lab32.state.dhtRawTempC = dhtTemp;
                    g_lab32.state.dhtRawHumidityPct = dhtHum;
                }
                g_lab32.state.dhtSeq++;
            }

            if (usUpdated)
            {
                g_lab32.state.usValid = usOk;
                if (usOk)
                {
                    g_lab32.state.usRawCm = usCm;
                }
                g_lab32.state.usSeq++;
            }

            xSemaphoreGive(g_lab32.stateMutex);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab32.config.sampleMs));
    }
}

static void taskConditioning(void *pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        if (xSemaphoreTake(g_lab32.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            if ((g_lab32.state.ntcSeq != s_lastNtcSeq) && g_lab32.state.ntcValid)
            {
                const Lab32ConditioningResult c = s_ntcConditioner.Process(g_lab32.state.ntcRawTempC);
                g_lab32.state.ntcClampedC = c.clampedValue;
                g_lab32.state.ntcMedianC = c.medianValue;
                g_lab32.state.ntcFilteredC = c.filteredValue;

                if (s_ntcDebouncedAlert)
                {
                    s_ntcCandidateAlert = (g_lab32.state.ntcFilteredC >= (g_lab32.config.ntcThrC - g_lab32.config.ntcHystC));
                }
                else
                {
                    s_ntcCandidateAlert = (g_lab32.state.ntcFilteredC >= g_lab32.config.ntcThrC);
                }

                if (s_ntcCandidateAlert != s_ntcDebouncedAlert)
                {
                    s_ntcPending++;
                    if (s_ntcPending >= g_lab32.config.persistenceSamples)
                    {
                        s_ntcDebouncedAlert = s_ntcCandidateAlert;
                        s_ntcPending = 0u;
                    }
                }
                else
                {
                    s_ntcPending = 0u;
                }

                g_lab32.state.ntcAlert = s_ntcDebouncedAlert;
                g_lab32.state.ntcPending = s_ntcPending;
                s_lastNtcSeq = g_lab32.state.ntcSeq;
            }

            if ((g_lab32.state.dhtSeq != s_lastDhtSeq) && g_lab32.state.dhtValid)
            {
                const Lab32ConditioningResult c = s_dhtConditioner.Process(g_lab32.state.dhtRawTempC);
                g_lab32.state.dhtClampedC = c.clampedValue;
                g_lab32.state.dhtMedianC = c.medianValue;
                g_lab32.state.dhtFilteredC = c.filteredValue;

                if (s_dhtDebouncedAlert)
                {
                    s_dhtCandidateAlert = (g_lab32.state.dhtFilteredC >= (g_lab32.config.dhtThrC - g_lab32.config.dhtHystC));
                }
                else
                {
                    s_dhtCandidateAlert = (g_lab32.state.dhtFilteredC >= g_lab32.config.dhtThrC);
                }

                if (s_dhtCandidateAlert != s_dhtDebouncedAlert)
                {
                    s_dhtPending++;
                    if (s_dhtPending >= g_lab32.config.persistenceSamples)
                    {
                        s_dhtDebouncedAlert = s_dhtCandidateAlert;
                        s_dhtPending = 0u;
                    }
                }
                else
                {
                    s_dhtPending = 0u;
                }

                g_lab32.state.dhtAlert = s_dhtDebouncedAlert;
                g_lab32.state.dhtPending = s_dhtPending;
                s_lastDhtSeq = g_lab32.state.dhtSeq;
            }

            if ((g_lab32.state.usSeq != s_lastUsSeq) && g_lab32.state.usValid)
            {
                const Lab32ConditioningResult c = s_usConditioner.Process(g_lab32.state.usRawCm);
                g_lab32.state.usClampedCm = c.clampedValue;
                g_lab32.state.usMedianCm = c.medianValue;
                g_lab32.state.usFilteredCm = c.filteredValue;

                if (s_usDebouncedAlert)
                {
                    s_usCandidateAlert = (g_lab32.state.usFilteredCm <= (g_lab32.config.usAlertCm + g_lab32.config.usHystCm));
                }
                else
                {
                    s_usCandidateAlert = (g_lab32.state.usFilteredCm <= g_lab32.config.usAlertCm);
                }

                if (s_usCandidateAlert != s_usDebouncedAlert)
                {
                    s_usPending++;
                    if (s_usPending >= g_lab32.config.persistenceSamples)
                    {
                        s_usDebouncedAlert = s_usCandidateAlert;
                        s_usPending = 0u;
                    }
                }
                else
                {
                    s_usPending = 0u;
                }

                g_lab32.state.usAlert = s_usDebouncedAlert;
                g_lab32.state.usPending = s_usPending;
                s_lastUsSeq = g_lab32.state.usSeq;
            }

            if (!g_lab32.state.ntcValid || !g_lab32.state.dhtValid || !g_lab32.state.usValid)
            {
                g_lab32.state.status = LAB32_STATUS_SENSOR_FAULT;
            }
            else if (g_lab32.state.ntcAlert || g_lab32.state.dhtAlert || g_lab32.state.usAlert)
            {
                g_lab32.state.status = LAB32_STATUS_ALERT;
            }
            else if ((g_lab32.state.ntcFilteredC >= (g_lab32.config.ntcThrC - g_lab32.config.ntcHystC))
                     || (g_lab32.state.dhtFilteredC >= (g_lab32.config.dhtThrC - g_lab32.config.dhtHystC))
                     || (g_lab32.state.usFilteredCm <= (g_lab32.config.usAlertCm + g_lab32.config.usHystCm)))
            {
                g_lab32.state.status = LAB32_STATUS_WARN;
            }
            else
            {
                g_lab32.state.status = LAB32_STATUS_OK;
            }

            xSemaphoreGive(g_lab32.stateMutex);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab32.config.conditionMs));
    }
}

static void taskAlerting(void *pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();
    static TickType_t redLastBlinkTick = 0;
    static TickType_t blueLastBlinkTick = 0;
    static TickType_t yellowLastBlinkTick = 0;
    static bool redBlinkState = false;
    static bool blueBlinkState = false;
    static bool yellowBlinkState = false;
    const TickType_t blinkPeriod = pdMS_TO_TICKS(300);

    for (;;)
    {
        bool ntcValid = false;
        bool dhtValid = false;
        bool usValid = false;
        float ntcTempC = 0.0f;
        float dhtTempC = 0.0f;
        float usDistanceCm = 0.0f;

        if (xSemaphoreTake(g_lab32.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            ntcValid = g_lab32.state.ntcValid;
            dhtValid = g_lab32.state.dhtValid;
            usValid = g_lab32.state.usValid;
            ntcTempC = g_lab32.state.ntcFilteredC;
            dhtTempC = g_lab32.state.dhtFilteredC;
            usDistanceCm = g_lab32.state.usFilteredCm;
            xSemaphoreGive(g_lab32.stateMutex);
        }

        bool redDesired = false;
        bool blueDesired = false;
        bool yellowDesired = false;
        const TickType_t now = xTaskGetTickCount();

        if (!ntcValid)
        {
            redBlinkState = false;
            redDesired = false;
        }
        else if (ntcTempC >= 30.0f)
        {
            if ((now - redLastBlinkTick) >= blinkPeriod)
            {
                redBlinkState = !redBlinkState;
                redLastBlinkTick = now;
            }
            redDesired = redBlinkState;
        }
        else if (ntcTempC >= 25.0f)
        {
            redBlinkState = true;
            redDesired = true;
        }
        else
        {
            redBlinkState = false;
            redDesired = false;
        }

        if (!dhtValid)
        {
            blueBlinkState = false;
            blueDesired = false;
        }
        else if (dhtTempC >= 30.0f)
        {
            if ((now - blueLastBlinkTick) >= blinkPeriod)
            {
                blueBlinkState = !blueBlinkState;
                blueLastBlinkTick = now;
            }
            blueDesired = blueBlinkState;
        }
        else if (dhtTempC >= 25.0f)
        {
            blueBlinkState = true;
            blueDesired = true;
        }
        else
        {
            blueBlinkState = false;
            blueDesired = false;
        }

        if (!usValid)
        {
            yellowBlinkState = false;
            yellowDesired = false;
        }
        else if (usDistanceCm < 10.0f)
        {
            if ((now - yellowLastBlinkTick) >= blinkPeriod)
            {
                yellowBlinkState = !yellowBlinkState;
                yellowLastBlinkTick = now;
            }
            yellowDesired = yellowBlinkState;
        }
        else if (usDistanceCm < 50.0f)
        {
            yellowBlinkState = true;
            yellowDesired = true;
        }
        else
        {
            yellowBlinkState = false;
            yellowDesired = false;
        }

        s_ntcLed.ApplyState(redDesired);
        s_dhtLed.ApplyState(blueDesired);
        s_usLed.ApplyState(yellowDesired);

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(50));
    }
}

static void taskDisplay(void *pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        Lab32RuntimeState snap{};
        if (xSemaphoreTake(g_lab32.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            snap = g_lab32.state;
            xSemaphoreGive(g_lab32.stateMutex);
        }

        if (xSemaphoreTake(g_lab32.ioMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            const char* statusName = "OK";
            if (snap.status == LAB32_STATUS_WARN) statusName = "WARN";
            else if (snap.status == LAB32_STATUS_ALERT) statusName = "ALERT";
            else if (snap.status == LAB32_STATUS_SENSOR_FAULT) statusName = "SENSOR_FAULT";

            char ntcRaw[16], ntcM[16], ntcF[16], dhtRaw[16], dhtH[16], dhtF[16], usRaw[16], usF[16];

            printf("[L3_2][NTC] i=%lu v=%s adc=%u raw=%sC med=%sC filt=%sC alert=%u p=%u\r\n",
                   (unsigned long)snap.sampleIndex,
                   snap.ntcValid ? "OK" : "WAIT",
                   snap.ntcRawAdc,
                   fmtFloat(snap.ntcRawTempC, 0, 2, ntcRaw),
                   fmtFloat(snap.ntcMedianC, 0, 2, ntcM),
                   fmtFloat(snap.ntcFilteredC, 0, 2, ntcF),
                   snap.ntcAlert ? 1u : 0u,
                   snap.ntcPending);

            printf("[L3_2][DHT] v=%s raw=%sC hum=%s%% filt=%sC alert=%u p=%u\r\n",
                   snap.dhtValid ? "OK" : "WAIT",
                   fmtFloat(snap.dhtRawTempC, 0, 2, dhtRaw),
                   fmtFloat(snap.dhtRawHumidityPct, 0, 1, dhtH),
                   fmtFloat(snap.dhtFilteredC, 0, 2, dhtF),
                   snap.dhtAlert ? 1u : 0u,
                   snap.dhtPending);

            printf("[L3_2][US ] v=%s raw=%scm filt=%scm alert=%u p=%u status=%s\r\n",
                   snap.usValid ? "OK" : "WAIT",
                   fmtFloat(snap.usRawCm, 0, 1, usRaw),
                   fmtFloat(snap.usFilteredCm, 0, 1, usF),
                   snap.usAlert ? 1u : 0u,
                   snap.usPending,
                   statusName);

            xSemaphoreGive(g_lab32.ioMutex);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab32.config.reportMs));
    }
}

static void taskLcdDisplay(void *pv)
{
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();
    const float lcdTempAlertC = 30.0f;
    const float lcdUsAlertCm = 50.0f;
    const float lcdUsCriticalCm = 10.0f;

    for (;;)
    {
        Lab32RuntimeState snap{};
        if (xSemaphoreTake(g_lab32.stateMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            snap = g_lab32.state;
            xSemaphoreGive(g_lab32.stateMutex);
        }

        char line1[17] = {0};
        char line2[17] = {0};

        const bool ntcHigh = snap.ntcValid && (snap.ntcFilteredC >= lcdTempAlertC);
        const bool dhtHigh = snap.dhtValid && (snap.dhtFilteredC >= lcdTempAlertC);
        const bool usCritical = snap.usValid && (snap.usFilteredCm < lcdUsCriticalCm);
        const bool usNear = snap.usValid && (snap.usFilteredCm < lcdUsAlertCm);
        const bool hasAlert = ntcHigh || dhtHigh || usNear;

        if (hasAlert)
        {
            if (ntcHigh && dhtHigh)
            {
                snprintf(line1, sizeof(line1), "ALERT NTC+DHT");
            }
            else if (ntcHigh)
            {
                snprintf(line1, sizeof(line1), "ALERT NTC >30C");
            }
            else if (dhtHigh)
            {
                snprintf(line1, sizeof(line1), "ALERT DHT >30C");
            }
            else
            {
                snprintf(line1, sizeof(line1), "ALERT ULTRASONIC");
            }

            if (usCritical)
            {
                snprintf(line2, sizeof(line2), "US CRITICAL <10cm");
            }
            else if (usNear)
            {
                snprintf(line2, sizeof(line2), "OBJECT NEARBY");
            }
            else
            {
                snprintf(line2, sizeof(line2), "CHECK TEMPERATURE");
            }
        }
        else
        {
            snprintf(line1, sizeof(line1), "SYSTEM NORMAL");
            snprintf(line2, sizeof(line2), "NO ACTIVE ALERT");
        }

        lcd_printf_lines(line1, line2);

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(g_lab32.config.lcdMs));
    }
}
