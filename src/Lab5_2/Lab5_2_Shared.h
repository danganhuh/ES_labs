#ifndef LAB5_2_SHARED_H
#define LAB5_2_SHARED_H

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <queue.h>
#include <semphr.h>

/*
 * Lab 5.2 — Closed-loop air-temperature control with a relay-driven motor/fan.
 *
 * Combined application (one folder, one binary) that fuses the two reference
 * labs from the methodological guide:
 *
 *   Lab 5.1 / 6.1 — ON-OFF control with hysteresis (binary actuation).
 *   Lab 5.2 / 6.2 — PID control via TIME-PROPORTIONAL relay switching.
 *
 * The control mode is switchable at runtime over the Serial command line, so
 * the same firmware demonstrates both control philosophies on the same plant
 * without re-flashing.
 *
 * --- Plant ----------------------------------------------------------------
 *   - DS18B20 1-Wire digital thermometer on LAB5_2_TEMP_PIN (4.7 kΩ pull-up
 *     to 5 V is mandatory — OneWire spec). 12-bit native resolution, sampled
 *     at 1000 ms in the acquisition task.
 *   - 5 V relay module on LAB5_2_RELAY_PIN driving a DC cooling fan/motor
 *     from the COM/NO contacts. The relay is BINARY (mechanical), so a PWM
 *     waveform would just buzz the coil — instead the PID task asks for a
 *     0..100% duty and the actuator implements TIME-PROPORTIONAL control:
 *     the duty is converted into ON / OFF slices across a window of
 *     LAB5_2_RELAY_WINDOW_MS (default 2000 ms).
 *   - I²C 16x2 LCD on the standard Mega lines (SDA = D20, SCL = D21).
 *
 * --- Why the relay (and not a PWM fan driver) ----------------------------
 *   The earlier draft used an L9110H PWM driver. That works in simulation
 *   but breaks on the physical bench, because the user's actuator is a
 *   DC motor connected through a relay — and the relay can't follow PWM.
 *   With a relay you need TPC: low duty ⇒ short ON-pulse + long OFF-pulse,
 *   high duty ⇒ long ON + short OFF. The mechanical inertia of the fan
 *   does the low-pass filtering for free.
 *
 * --- Control polarity (COOLING) -------------------------------------------
 *   PV > SP  ⇒ too hot ⇒ relay closed ⇒ motor running ⇒ fan blowing
 *   PV < SP  ⇒ already cold enough ⇒ relay open ⇒ motor off
 *   Symmetric: bringing temp below the set-point makes the controller's
 *   demand drop to 0%, and the relay opens.
 *
 * --- Layered architecture (matches the Indrumar) --------------------------
 *   APP   src/Lab5_2/Lab5_2_main.cpp            (this lab)
 *   APP   src/Lab5_2/ctrl_onoff_hyst.{h,cpp}    (control: ON-OFF + hyst)
 *   APP   src/Lab5_2/ctrl_pid.{h,cpp}           (control: discrete PID)
 *   SRV   src/Lab5_2/srv_temp_sensor.{h,cpp}    (DS18B20 wrapper + cond.)
 *   SRV   src/Lab5_2/srv_fan.{h,cpp}            (relay + TPC actuator)
 *   ECAL  Arduino DallasTemperature/OneWire libs
 *   MCAL  Arduino core (digitalWrite, Wire, ...)
 */

// ============================================================================
// Pin Configuration (Arduino Mega 2560)
// ============================================================================

/** DS18B20 1-Wire data line. Add a 4.7 kΩ pull-up between this pin and 5 V. */
#define LAB5_2_TEMP_PIN              5

/** Relay control input (IN pin on a typical 5 V relay module).
 *  Kept on D8 to match the existing physical wiring from the previous build. */
#define LAB5_2_RELAY_PIN             8

/** 1 ⇒ relay module is active-LOW (typical optocoupled JQC-3FF / Songle and
 *  every Wokwi `wokwi-relay-module` part: IN=LOW closes COM ↔ NO).
 *  0 ⇒ relay module is active-HIGH (raw transistor-driven boards).
 *
 *  This is overridable at runtime via the `polarity low|high` command —
 *  the constant only sets the BOOT value. */
#define LAB5_2_RELAY_ACTIVE_LOW      1

/** I²C LCD module address (most PCF8574 backpacks ship as 0x27, some 0x3F). */
#define LAB5_2_LCD_I2C_ADDR          0x27

// ============================================================================
// Control defaults
// ============================================================================

/** Default cooling set-point. The PID/ON-OFF tries to hold PV ≤ this value. */
#define LAB5_2_DEFAULT_SETPOINT_C    25.0f

/** Saturation band for the set-point (Indrumar prescribes 15..35 °C). */
#define LAB5_2_SETPOINT_MIN_C        15.0f
#define LAB5_2_SETPOINT_MAX_C        35.0f

/** Half-band hysteresis for ON-OFF mode (band = ±H around set-point). */
#define LAB5_2_DEFAULT_HYST_C        1.0f

/*
 * Default PID gains tuned for a small relay-switched DC fan in still air
 * (Ts = 100 ms control loop, 2000 ms TPC window). Profile: BALANCED.
 * Tuning advice (manual ramp from 0):
 *   Soft / quiet            : Kp = 25, Ki = 0.20, Kd = 0
 *   Balanced (default)      : Kp = 50, Ki = 0.50, Kd = 0
 *   Aggressive (overshoot)  : Kp = 80, Ki = 1.50, Kd = 5
 *   Pure P (demo SS error)  : Kp = 50, Ki = 0,    Kd = 0
 */
#define LAB5_2_DEFAULT_KP            50.0f
#define LAB5_2_DEFAULT_KI            0.50f
#define LAB5_2_DEFAULT_KD            0.0f

/** PID output saturation, in % duty (0..100). One direction only — the fan
 *  can only cool, so negative integral or P contribution is discarded. */
#define LAB5_2_DEFAULT_OUTPUT_LIMIT  100.0f

/** Anti-windup ceiling on the integral accumulator. Matches the reference. */
#define LAB5_2_PID_INTEGRAL_LIMIT    100.0f

// ============================================================================
// Relay actuator (time-proportional cycling)
// ============================================================================

/** Length of one TPC window in milliseconds.
 *
 *  Inside each window the actuator keeps the relay closed for
 *  (demand / 100) × window ms, then opens it for the remainder. Shorter
 *  windows give smoother control but wear the relay; longer windows are
 *  gentler on the contacts. 2000 ms is a balanced default — same value as
 *  the reference Indrumar Lab 5.2 / 6.2. Runtime-overridable with the
 *  `window <ms>` command (clamped to 500..10000 ms). */
#define LAB5_2_DEFAULT_RELAY_WINDOW_MS  2000u

/** Hard floor / ceiling for the `window` command. */
#define LAB5_2_RELAY_WINDOW_MIN_MS      500u
#define LAB5_2_RELAY_WINDOW_MAX_MS      10000u

// ============================================================================
// Task periods (ms) — matches Indrumar §2.4 timings
// ============================================================================

/** Sensor read recurrence. DS18B20 12-bit conversion costs ~750 ms, so
 *  anything faster than 1000 ms is wasted on the bus. */
#define LAB5_2_ACQ_TASK_MS           1000u

/** PID loop recurrence. 100 ms is the value used in the reference Lab 6.2. */
#define LAB5_2_CTRL_TASK_MS          100u

/** Actuator recurrence. 100 ms is fine-grained enough to resolve the
 *  TPC window with ~20 slices at the default 2000 ms window. */
#define LAB5_2_FAN_TASK_MS           100u

/** LCD refresh period (slow; LCD is the bottleneck). */
#define LAB5_2_DISP_TASK_MS          500u

/** Serial command poll period. Short = snappy, but we still yield the CPU. */
#define LAB5_2_CMD_TASK_MS           80u

/** Reporting / Plotter recurrence. */
#define LAB5_2_REPORT_TASK_MS        1000u

// ============================================================================
// Conditioning pipeline parameters (signal conditioning, mirrors lib_cond)
// ============================================================================

/** Length of the median filter window. Odd > 3, larger = stronger rejection
 *  of single-bit decode glitches at the cost of latency. 5 is a good default. */
#define LAB5_2_MEDIAN_WINDOW         5

/** Length of the weighted-average smoothing window applied AFTER the median.
 *  Newer samples weighted higher. */
#define LAB5_2_WAVG_WINDOW           5

/** Hard saturation band on incoming sensor samples (anything outside is
 *  treated as a decode error and clamped). */
#define LAB5_2_TEMP_MIN_C            (-10)
#define LAB5_2_TEMP_MAX_C            (100)

// ============================================================================
// Reporting modes (selected at boot, may be changed at runtime via `plotter`)
// ============================================================================

#define LAB5_2_REPORT_MODE_SERIAL    0u   /* human-readable text         */
#define LAB5_2_REPORT_MODE_PLOTTER   1u   /* >Temp:..,SetPoint:..,...    */
#define LAB5_2_REPORT_MODE_LCD       2u   /* LCD only, quiet on Serial    */

// ============================================================================
// Types
// ============================================================================

typedef enum
{
    LAB5_2_MODE_ONOFF = 0,
    LAB5_2_MODE_PID   = 1
} Lab52Mode;

/** Manual override of the actuator, useful for hardware bring-up. */
typedef enum
{
    LAB5_2_FORCE_AUTO = 0,    /* normal: controller decides the duty     */
    LAB5_2_FORCE_ON   = 1,    /* relay forced closed (motor running)     */
    LAB5_2_FORCE_OFF  = 2     /* relay forced open   (motor stopped)     */
} Lab52ForceMode;

/** When `force on` is active, this is the duty handed to the actuator.
 *  For a relay it just means "closed continuously". */
#define LAB5_2_FORCE_ON_PCT          100

/** A single sample of the process value, posted by the acquisition task. */
typedef struct
{
    float tempC;            /* filtered air temperature, °C              */
    float rawTempC;         /* unfiltered sample (kept for plotter)       */
    bool  valid;            /* false ⇒ sensor failed; controller bails    */
    unsigned long timestampMs;
} Lab52Measurement;

/** One iteration of the controller, passed to the actuator task. */
typedef struct
{
    float fanPctTarget;     /* 0..100 demand from controller              */
    float pidOutput;        /* raw, unclamped PID — telemetry only        */
    float errorC;           /* PV - SP; positive ⇒ hotter than set-point  */
    Lab52Mode mode;
    bool  valid;            /* false ⇒ no sensor; force actuator off      */
    unsigned long timestampMs;
} Lab52ControlOutput;

/** Persistent (mutex-guarded) configuration. Modified by the command task. */
typedef struct
{
    float setpointC;
    float hysteresisC;
    float kp;
    float ki;
    float kd;
    float outputLimit;

    uint16_t acqTaskMs;
    uint16_t ctrlTaskMs;
    uint16_t fanTaskMs;
    uint16_t dispTaskMs;
    uint16_t cmdTaskMs;
    uint16_t reportTaskMs;

    /* Relay actuator */
    uint16_t relayWindowMs;     /* TPC window length, ms                 */
    bool     relayActiveLow;    /* runtime polarity (default from macro) */
} Lab52Config;

/** Live runtime state. Mutex-guarded; one snapshot read per task cycle. */
typedef struct
{
    /* Sensor pipeline */
    float tempC;            /* filtered                                   */
    float tempRawC;         /* last raw sample                            */
    bool  sensorValid;

    /* Controller snapshot */
    float setpointC;
    float hysteresisC;
    float errorC;
    float pidOutput;
    float fanPctDemand;     /* what the controller asked for (0..100)     */
    float fanPctActual;     /* what the actuator translated this cycle    */
    bool  relayOn;          /* instantaneous relay contact state          */
    Lab52Mode mode;

    /* Reporting */
    uint8_t reportMode;     /* LAB5_2_REPORT_MODE_*                       */
    Lab52ForceMode forceMode;

    /* Telemetry */
    unsigned long lastSampleMs;
    unsigned long controlCycles;
    unsigned long commandCounter;

    /* Temporary LCD banner (shown by `status` / `help`) */
    char lcdBannerL1[17];
    char lcdBannerL2[17];
    unsigned long lcdBannerUntilMs;
} Lab52RuntimeState;

/** Bag of FreeRTOS handles + state shared by every task. */
typedef struct
{
    Lab52Config       config;
    Lab52RuntimeState state;

    QueueHandle_t     qControl;     /* taskControl → taskFan              */
    SemaphoreHandle_t stateMutex;   /* protects `config` + `state`        */
    SemaphoreHandle_t ioMutex;      /* serialises printf / scanf streams  */
} Lab52Shared;

extern Lab52Shared g_lab52;

#endif
