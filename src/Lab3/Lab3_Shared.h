#ifndef LAB3_SHARED_H
#define LAB3_SHARED_H

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <queue.h>
#include <semphr.h>

#define LAB3_NTC_PIN             A0
#define LAB3_ALERT_LED_PIN       13

#define LAB3_LCD_I2C_ADDR        0x27

#define LAB3_DEFAULT_SAMPLE_MS      50u
#define LAB3_DEFAULT_REPORT_MS      1000u
#define LAB3_DEFAULT_PERSIST_MS     100u
#define LAB3_DEFAULT_THRESHOLD_C    25.0f
#define LAB3_DEFAULT_HYSTERESIS_C   1.0f
#define LAB3_DEFAULT_ALPHA          0.25f

#define LAB3_LED_ON_C               25.0f
#define LAB3_LED_BLINK_C            30.0f
#define LAB3_LED_BLINK_MS           300u

#define LAB3_CONDITION_TASK_MS      50u
#define LAB3_ALERT_TASK_MS          50u
#define LAB3_LCD_TASK_MS            500u

#define LAB3_MIN_SAMPLE_MS          20u
#define LAB3_MAX_SAMPLE_MS          100u
#define LAB3_MIN_REPORT_MS          200u
#define LAB3_MAX_REPORT_MS          2000u

typedef struct
{
    uint16_t adcRaw;
    float voltage;
    float tempC;
    TickType_t timestamp;
} Lab3RawSample;

typedef struct
{
    float thresholdC;
    float hysteresisC;
    float alpha;
    uint16_t sampleMs;
    uint16_t persistenceMs;
    uint16_t reportMs;
} Lab3Config;

typedef struct
{
    uint32_t sampleIndex;
    uint16_t rawAdc;
    float rawVoltage;
    float rawTempC;
    float clampedTempC;
    float filteredTempC;
    bool hysteresisCandidate;
    bool debouncedAlert;
    bool alertLedState;
    uint16_t persistenceCounter;
    uint16_t persistenceSamples;
    uint32_t alertTransitions;
    TickType_t timestamp;
} Lab3RuntimeState;

typedef struct
{
    QueueHandle_t rawQueue;
    SemaphoreHandle_t alertSignal;
    SemaphoreHandle_t stateMutex;
    SemaphoreHandle_t ioMutex;
    Lab3Config config;
    Lab3RuntimeState state;
} Lab3Shared;

extern Lab3Shared g_lab3;

#endif
