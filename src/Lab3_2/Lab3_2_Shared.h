#ifndef LAB3_2_SHARED_H
#define LAB3_2_SHARED_H

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>

#define LAB32_NTC_PIN               A0
#define LAB32_DHT_PIN               7
#define LAB32_US_TRIG_PIN           8
#define LAB32_US_ECHO_PIN           9

#define LAB32_LED_NTC_PIN           13
#define LAB32_LED_DHT_PIN           12
#define LAB32_LED_US_PIN            11

#define LAB32_LCD_I2C_ADDR          0x27

#define LAB32_DEFAULT_SAMPLE_MS         50u
#define LAB32_DEFAULT_CONDITION_MS      50u
#define LAB32_DEFAULT_REPORT_MS         500u
#define LAB32_DEFAULT_LCD_MS            500u
#define LAB32_DEFAULT_DHT_MS            2000u
#define LAB32_DEFAULT_US_MS             100u

#define LAB32_DEFAULT_ALPHA             0.30f
#define LAB32_DEFAULT_PERSIST_SAMPLES   2u

#define LAB32_DEFAULT_NTC_THR_C         27.0f
#define LAB32_DEFAULT_NTC_HYST_C        1.0f
#define LAB32_DEFAULT_DHT_THR_C         30.0f
#define LAB32_DEFAULT_DHT_HYST_C        1.0f
#define LAB32_DEFAULT_US_ALERT_CM       40.0f
#define LAB32_DEFAULT_US_HYST_CM        4.0f

#define LAB32_MIN_SAMPLE_MS             20u
#define LAB32_MAX_SAMPLE_MS             100u

typedef enum
{
    LAB32_STATUS_OK = 0,
    LAB32_STATUS_WARN = 1,
    LAB32_STATUS_ALERT = 2,
    LAB32_STATUS_SENSOR_FAULT = 3
} Lab32SystemStatus;

typedef struct
{
    uint16_t sampleMs;
    uint16_t conditionMs;
    uint16_t reportMs;
    uint16_t lcdMs;
    uint16_t dhtMs;
    uint16_t usMs;

    float alpha;
    uint16_t persistenceSamples;

    float ntcThrC;
    float ntcHystC;
    float dhtThrC;
    float dhtHystC;
    float usAlertCm;
    float usHystCm;
} Lab32Config;

typedef struct
{
    uint32_t sampleIndex;
    TickType_t timestamp;

    uint16_t ntcRawAdc;
    float ntcRawTempC;
    float ntcRawVoltage;
    bool ntcValid;
    uint32_t ntcSeq;
    float ntcClampedC;
    float ntcMedianC;
    float ntcFilteredC;
    bool ntcAlert;
    uint16_t ntcPending;

    float dhtRawTempC;
    float dhtRawHumidityPct;
    bool dhtValid;
    uint32_t dhtSeq;
    float dhtClampedC;
    float dhtMedianC;
    float dhtFilteredC;
    bool dhtAlert;
    uint16_t dhtPending;

    float usRawCm;
    bool usValid;
    uint32_t usSeq;
    float usClampedCm;
    float usMedianCm;
    float usFilteredCm;
    bool usAlert;
    uint16_t usPending;

    Lab32SystemStatus status;
} Lab32RuntimeState;

typedef struct
{
    SemaphoreHandle_t stateMutex;
    SemaphoreHandle_t ioMutex;
    Lab32Config config;
    Lab32RuntimeState state;
} Lab32Shared;

extern Lab32Shared g_lab32;

#endif
