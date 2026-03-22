#ifndef LAB4_SHARED_H
#define LAB4_SHARED_H

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>

#define LAB4_RELAY_PIN                 12
#define LAB4_MOTOR_ENA_PIN             10
#define LAB4_MOTOR_IN1_PIN             9
#define LAB4_MOTOR_IN2_PIN             8

#define LAB4_LCD_I2C_ADDR              0x27

#define LAB4_DEFAULT_CTRL_MS           50u
#define LAB4_DEFAULT_COND_MS           50u
#define LAB4_DEFAULT_REPORT_MS         500u
#define LAB4_DEFAULT_DEBOUNCE_ON_MS    80u
#define LAB4_DEFAULT_DEBOUNCE_OFF_MS   80u
#define LAB4_DEFAULT_MOTOR_ALPHA       0.30f
#define LAB4_DEFAULT_MOTOR_RAMP        250.0f

#define LAB4_CMD_TASK_MS               20u
#define LAB4_MIN_CTRL_MS               20u
#define LAB4_MAX_CTRL_MS               100u
#define LAB4_MIN_REPORT_MS             200u
#define LAB4_MAX_REPORT_MS             2000u

typedef struct
{
    uint16_t controlMs;
    uint16_t conditionMs;
    uint16_t reportMs;
    uint16_t debounceOnMs;
    uint16_t debounceOffMs;
    float motorAlpha;
    float motorRampPctPerSec;
} Lab4Config;

typedef struct
{
    int cmdRelayState;
    float cmdMotorPct;
    uint32_t invalidCommandCount;
    uint32_t cmdSeq;
    unsigned long lastCommandMs;
    bool fanIsOn;
    unsigned long fanOnStartMs;
    unsigned long fanLastOnDurationMs;
    unsigned long fanTotalOnMs;

    int relayState;
    float motorStatePct;
    bool relayDebounceAlert;
    bool motorSaturationAlert;
    uint32_t relayTransitions;

    uint16_t responseLatencyMs;
    uint32_t stateSeq;
} Lab4RuntimeState;

typedef struct
{
    SemaphoreHandle_t stateMutex;
    SemaphoreHandle_t ioMutex;
    SemaphoreHandle_t controlMutex;
    Lab4Config config;
    Lab4RuntimeState state;
} Lab4Shared;

extern Lab4Shared g_lab4;

#endif
