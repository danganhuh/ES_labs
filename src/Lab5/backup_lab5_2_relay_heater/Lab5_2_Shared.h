#ifndef LAB5_2_SHARED_H
#define LAB5_2_SHARED_H

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <DHT.h>

/*
 * Lab 5.2 — Closed-loop air temperature with a relay-driven heater.
 *
 * Hardware (single-channel, no motor / no fan):
 *   - DHT11 on LAB5_2_DHT_PIN          (process value: ambient °C)
 *   - 5 V relay module on LAB5_2_HEAT_RELAY_PIN (active-LOW input)
 *   - Heater = 3 resistors wired in parallel between relay-NO and GND
 *     (powered from a separate 5 V rail, common GND with the Arduino)
 *   - I²C 16x2 LCD at LAB5_2_LCD_I2C_ADDR (Mega: SDA=20, SCL=21)
 *
 * Two control modes are selectable at runtime over Serial:
 *   - mode onoff : bang-bang with hysteresis around the set-point.
 *   - mode pid   : continuous PID converted to a relay duty cycle by
 *                  TIME-PROPORTIONAL switching across LAB5_2_RELAY_WINDOW_MS.
 *
 * Cooling is purely passive — the resistors lose heat to ambient when the
 * relay opens. The plant is therefore asymmetric (heating fast, cooling
 * slow), which is reflected in the PID defaults below.
 */

// ============================================================================
// Pin Configuration
// ============================================================================

/** DHT 1-wire data pin. The blue 3-pin module already has its own pull-up;
 *  for a bare 4-pin chip, add an external 4.7 kΩ–10 kΩ resistor from DATA to VCC. */
#define LAB5_2_DHT_PIN               2

/**
 * Sensor variant. The Adafruit DHT library decodes the same 5 raw bytes
 * differently per type, so a mismatch produces wildly wrong but checksum-valid
 * numbers (typical symptom: T near 0 °C and H ≈ 14 % when the sensor is in
 * fact a DHT22 read as DHT11). If you don't know which one you have, send
 * `dhtprobe` over Serial and compare the two decodings — only one will land
 * on plausible numbers.
 *
 * Allowed values: DHT11, DHT12, DHT21 / AM2301, DHT22.
 */
#define LAB5_2_DHT_TYPE              DHT22

/** 5 V relay module signal input. Wokwi-style relay module: IN=LOW closes COM-NO. */
#define LAB5_2_HEAT_RELAY_PIN        8

/** Polarity of the relay module input. 1 = active-LOW (Wokwi default), 0 = active-HIGH. */
#define LAB5_2_RELAY_ACTIVE_LOW      1

// ============================================================================
// LCD configuration
// ============================================================================

#define LAB5_2_LCD_I2C_ADDR          0x27

// ============================================================================
// Control defaults
// ============================================================================

/** Default set-point. Pick a few °C above your typical room temperature. */
#define LAB5_2_DEFAULT_SETPOINT_C    28.0f

/** Half-band hysteresis used in ON-OFF mode (relay opens at SP+H, closes at SP-H). */
#define LAB5_2_DEFAULT_HYST_C        1.0f

/*
 * Default PID gains tuned for a small (~1 W) resistor heater + DHT11 plant
 * with a 250 ms loop and a 2 s time-proportional relay window. Profile:
 * BALANCED — ~2 min rise, <1 °C overshoot, no steady-state error.
 *
 * For other heater wattages or different sensor placements:
 *   Soft / safe (slow but no overshoot):     Kp = 10, Ki = 0.20, Kd = 5
 *   Balanced (default — recommended start):  Kp = 20, Ki = 0.40, Kd = 8
 *   Aggressive (fast, may overshoot ~2 °C):  Kp = 30, Ki = 1.00, Kd = 10
 *   Pure P (lab demo of steady-state error): Kp = 25, Ki = 0,    Kd = 0
 *   PD (no integral; allows offset, no SS):  Kp = 25, Ki = 0,    Kd = 12
 */
#define LAB5_2_DEFAULT_KP            20.0f
#define LAB5_2_DEFAULT_KI            0.40f
#define LAB5_2_DEFAULT_KD            8.0f

/** PID output saturation, in % heater duty (0..100). */
#define LAB5_2_DEFAULT_OUTPUT_LIMIT  100.0f

/** Maximum heater duty cap (PID never asks for more than this). 100 = use full power. */
#define LAB5_2_DEFAULT_HEAT_PCT      100.0f

/** Time-proportional window for the mechanical relay (ms). Longer = less wear. */
#define LAB5_2_DEFAULT_RELAY_WINDOW_MS  2000u

/** Below this duty% the relay is held off (avoids brief 1-sample blips that wear the contacts). */
#define LAB5_2_PID_MIN_PCT           2.0f

// ============================================================================
// Task periods (ms)
// ============================================================================

#define LAB5_2_ACQ_TASK_MS           1000u
#define LAB5_2_CTRL_TASK_MS          250u
#define LAB5_2_ACT_TASK_MS           50u
#define LAB5_2_DISP_TASK_MS          500u
#define LAB5_2_CMD_TASK_MS           80u

/** DHT11/DHT22 need ≥ 2 s between full reads (Adafruit lib MIN_INTERVAL = 2000 ms).
 *  We set 2200 ms so clock drift never makes the call land just under 2000 ms,
 *  in which case the library would silently return the previous (cached) value. */
#define LAB5_2_DHT_MIN_REFRESH_MS    2200u

/** Set to 1 to emit Arduino Serial Plotter lines (SetPoint, Value, Output). */
#define LAB5_2_SERIAL_PLOTTER        0

// ============================================================================
// Types
// ============================================================================

typedef enum
{
    LAB5_2_MODE_ONOFF = 0,
    LAB5_2_MODE_PID = 1
} Lab52Mode;

/** Manual override for hardware bring-up: force the relay regardless of the controller. */
typedef enum
{
    LAB5_2_FORCE_AUTO = 0,    /* normal: controller drives the relay */
    LAB5_2_FORCE_ON   = 1,    /* relay forced closed (heater always on) */
    LAB5_2_FORCE_OFF  = 2     /* relay forced open  (heater always off) */
} Lab52ForceMode;

typedef struct
{
    float tempC;
    float humidityPct;
    bool valid;
    unsigned long timestampMs;
} Lab52Measurement;

/** Snapshot of one control-task iteration, posted to the actuator queue. */
typedef struct
{
    float heatPct;          /* 0..100 demand passed to the actuator             */
    float pidOutput;        /* raw, unclamped PID output (for diagnostics)      */
    float errorC;           /* SP - PV; positive ⇒ colder than setpoint         */
    Lab52Mode mode;
    bool valid;             /* false ⇒ sensor failed; actuator forces relay off */
    unsigned long timestampMs;
} Lab52ControlOutput;

typedef struct
{
    float setpointC;
    float hysteresisC;
    float kp;
    float ki;
    float kd;
    float outputLimit;
    float heatDutyPct;          /* PID heater cap (0..100)                  */
    uint16_t relayWindowMs;     /* time-proportional period for the relay   */

    uint16_t acqTaskMs;
    uint16_t ctrlTaskMs;
    uint16_t actTaskMs;
    uint16_t dispTaskMs;
    uint16_t cmdTaskMs;
} Lab52Config;

typedef struct
{
    /* Process value & sensor */
    float tempC;
    float humidityPct;
    bool sensorValid;

    /* Set-point and tuning snapshot for the display task */
    float setpointC;
    float hysteresisC;

    /* Controller outputs */
    float errorC;
    float pidOutput;
    float heatPct;              /* 0..100 demand the actuator is currently using */
    Lab52Mode mode;

    /* Actuator state */
    bool relayOn;
    bool onoffHeating;          /* latched ON-OFF state (between hysteresis bands) */
    Lab52ForceMode forceMode;   /* manual override for hardware bring-up testing */

    /* Telemetry */
    unsigned long lastSampleMs;
    unsigned long controlCycles;
    unsigned long commandCounter;

    /* Temporary LCD banner, used by `status` and `help` commands */
    char lcdBannerL1[17];
    char lcdBannerL2[17];
    unsigned long lcdBannerUntilMs;
} Lab52RuntimeState;

typedef struct
{
    Lab52Config config;
    Lab52RuntimeState state;

    QueueHandle_t qControl;
    SemaphoreHandle_t stateMutex;
    SemaphoreHandle_t ioMutex;
} Lab52Shared;

extern Lab52Shared g_lab52;

#endif
