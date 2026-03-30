#ifndef LAB4_2_SHARED_H
#define LAB4_2_SHARED_H

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>

#define LAB4_2_RELAY_PIN               12
#define LAB4_2_RELAY_ACTIVE_LOW        1
#define LAB4_2_LED_GREEN_PIN           6
#define LAB4_2_LED_RED_PIN             7
#define LAB4_2_LED_SERVO_ALERT_PIN     4
#define LAB4_2_SERVO_PIN               5
#define LAB4_2_SERVO_POT_PIN           A0

#define LAB4_2_LCD_I2C_ADDR            0x27

#define LAB4_2_DEFAULT_CMD_MS          20u
#define LAB4_2_DEFAULT_COND_MS         50u
#define LAB4_2_DEFAULT_CTRL_MS         50u
#define LAB4_2_DEFAULT_REPORT_MS       500u
#define LAB4_2_DEFAULT_DEBOUNCE_MS     80u
#define LAB4_2_DEFAULT_ALPHA           0.35f
#define LAB4_2_DEFAULT_RAMP_PCT_S      120.0f

typedef struct
{
    uint16_t commandMs;
    uint16_t conditionMs;
    uint16_t controlMs;
    uint16_t reportMs;
    uint16_t relayDebounceMs;
    float signalAlpha;
    float rampPctPerSec;
} Lab42Config;

typedef struct
{
    bool relayCommand;
    float rawServoPct;
    bool servoManualMode;

    float clampedServoPct;
    float medianServoPct;
    float weightedServoPct;
    float rampedServoPct;

    bool relayState;
    float servoPct;
    uint16_t servoDeg;

    bool clampAlert;
    bool limitAlert;
    bool relayDebounceAlert;
    bool servoExtremeAlert;

    uint32_t invalidCommandCount;
    uint32_t commandSeq;
    uint32_t stateSeq;

    unsigned long lastCommandMs;
    uint16_t responseLatencyMs;
} Lab42RuntimeState;

typedef struct
{
    SemaphoreHandle_t stateMutex;
    SemaphoreHandle_t ioMutex;
    Lab42Config config;
    Lab42RuntimeState state;
} Lab42Shared;

extern Lab42Shared g_lab42;

#endif
