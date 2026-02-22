/**
 * @file Lab2_main.cpp
 * @brief SRV Layer - Lab 2: Button Duration Monitor with Sequential Bare-Metal Scheduler
 *
 * =============================================================================
 * TASK DESCRIPTION
 * =============================================================================
 *
 * Task 1 – Button Detection & Duration Measurement (every 20 ms, offset 0 ms)
 *   • Calls ButtonDriver::Update() for non-blocking debounce.
 *   • On button release: measures press duration, classifies short/long.
 *   • Publishes press event and duration to shared variables (provider role).
 *   • Notifies via printf: "[T1] Press: <N> ms (SHORT|LONG)".
 *
 * Task 2 – Statistics & LED Signalling (every 50 ms, offset 10 ms)
 *   • Consumer of press events produced by Task 1.
 *   • Short press: GREEN LED on, RED LED off, yellow blinks  5 times.
 *   • Long  press: RED   LED on, GREEN LED off, yellow blinks 10 times.
 *   • Yellow LED is ON while button is held (no blinks pending).
 *   • After release: yellow blinks 5 (short) or 10 (long) times.
 *
 * Task 3 – Periodic Report via printf (every 10 000 ms, offset 100 ms)
 *   • Prints: total presses, short, long, average duration.
 *   • Resets all statistics counters after printing.
 *
 * =============================================================================
 * SCHEDULER – Non-Preemptive Bare-Metal Sequential OS
 * =============================================================================
 * • Timer 1 (ATmega328P) generates a 1 ms CTC interrupt (ISR sets g_sysTick).
 * • Main loop detects the tick flag and calls os_seq_scheduler_loop().
 * • Each task is described by a Task_t {func, rec, offset, cnt} structure.
 * • Priority order (checked first = highest): Task1 > Task2 > Task3.
 *   This matches the provider→consumer ordering (button data must be ready
 *   before stats task consumes it on any given 50 ms aligned tick).
 * • Tick execution is non-blocking: tasks must NEVER busy-wait.
 *
 * =============================================================================
 * TIMING PLAN
 * =============================================================================
 *  ID | Task               | Period  | Offset | Rationale
 * ----|--------------------|---------|--------|----------------------------------
 *   1 | Button Monitor     |  20 ms  |   0 ms | 5 samples × 20 ms = 100 ms debounce
 *   2 | Stats & Blink      |  50 ms  |  10 ms | interleaved; avoids same-tick clash
 *   3 | Periodic Report    | 10 000 ms| 100 ms | 10 s interval; initial noise margin
 *
 *  CPU load estimate per tick:
 *   Task1 exec ≈ 0.05 ms → load per tick = 0.05/20  ≈ 0.25 %
 *   Task2 exec ≈ 0.05 ms → load per tick = 0.05/50  ≈ 0.10 %
 *   Task3 exec ≈ 2 ms    → load per tick = 2/10000  ≈ 0.02 %
 *   Total ≈ < 1 % → well below the 70-80 % safe threshold.
 *
 * =============================================================================
 * HARDWARE – Pin Assignment (Lab 2)
 * =============================================================================
 *  Component      | Arduino Pin | Notes
 * ----------------|-------------|-------------------------------------------
 *  Push-button    |  3          | INPUT_PULLUP, active-low (button to GND)
 *  Green LED      |  10         | Short-press indicator
 *  Red   LED      |  11         | Long-press  indicator
 *  Yellow LED     |  12         | Press-active (steady) + blink count signal
 *  Serial STDIO   |  TX/RX (0,1)| 9600 baud – printf via SerialStdioInit
 *
 * Architecture: APP (main.cpp) → SRV (Lab2) → ECAL (Drivers) → MCAL (Arduino)
 */

#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>

#include "led/LedDriver.h"
#include "button/ButtonDriver.h"
#include "drivers/SerialStdioDriver.h"

// ============================================================================
// Pin Configuration
// ============================================================================

static const uint8_t BTN_PIN        = 3;   ///< Push-button (INPUT_PULLUP, active low)
static const uint8_t LED_GREEN_PIN  = 10;  ///< Green LED  – short press indicator
static const uint8_t LED_RED_PIN    = 11;  ///< Red   LED  – long  press indicator
static const uint8_t LED_YELLOW_PIN = 12;  ///< Yellow LED – press-active + blink

// ============================================================================
// Scheduler Configuration
// ============================================================================

#define REC_BUTTON  20       ///< Task 1: 20 ms
#define REC_STATS   50       ///< Task 2: 50 ms – one blink-toggle per call
#define REC_REPORT  10000    ///< Task 3: 10 s

#define OFFS_BUTTON 0        ///< Task 1: no offset
#define OFFS_STATS  10       ///< Task 2: 10 ms – interleaved with Task 1
#define OFFS_REPORT 100      ///< Task 3: 100 ms – startup noise margin

#define SHORT_PRESS_THRESHOLD_MS 500UL

// ============================================================================
// Task Context Structure  (Section 3.1 – optimised variant)
// ============================================================================

/**
 * @brief Descriptor for a single sequential task entry.
 */
typedef struct
{
    void (*task_func)(void); ///< Pointer to the task's main function
    int   rec;               ///< Period (ms) between consecutive activations
    int   offset;            ///< Initial delay (ms) before first activation
    int   rec_cnt;           ///< Down-counter; task fires when this reaches 0
} Task_t;

/** Enumerate task table indices */
enum TaskId
{
    TASK_BUTTON_ID = 0,
    TASK_STATS_ID,
    TASK_REPORT_ID,
    MAX_OF_TASKS
};

static void Task1_ButtonMonitor(void);
static void Task2_StatsAndLeds(void);
static void Task3_PeriodicReport(void);

/** Global task table (initialised in os_seq_scheduler_setup) */
static Task_t tasks[MAX_OF_TASKS];

// ============================================================================
// Hardware Driver Instances  (ECAL layer – reused from project drivers)
// ============================================================================

static LedDriver    ledGreen (LED_GREEN_PIN);
static LedDriver    ledRed   (LED_RED_PIN);
static LedDriver    ledYellow(LED_YELLOW_PIN);
static ButtonDriver button   (BTN_PIN, true, true); ///< active-low, INPUT_PULLUP

// ============================================================================
// Shared Variables  (inter-task communication via global setter/getter pattern)
// ============================================================================

/** Written by Task1 (provider), read & cleared by Task2 (consumer) */
static volatile bool     g_pressEvent    = false; ///< New press available flag
static volatile uint32_t g_pressDuration = 0;     ///< Duration of last press (ms)
static volatile bool     g_pressIsShort  = false; ///< true=short (<500ms), false=long

/** Written by Task2, read & reset by Task3 */
static volatile uint16_t g_totalPresses  = 0;  ///< Total button presses
static volatile uint16_t g_shortPresses  = 0;  ///< Presses shorter than 500 ms
static volatile uint16_t g_longPresses   = 0;  ///< Presses 500 ms or longer
static volatile uint32_t g_totalDuration = 0;  ///< Accumulated press duration (ms)

// ============================================================================
// Task 2 private state
// ============================================================================

/** Remaining yellow LED toggle steps (2 toggles = 1 blink) */
static uint8_t s_yellowTogglesLeft = 0;
static bool    s_prevPressed       = false;
static uint32_t s_pressStartMs     = 0;

// ============================================================================
// Sys-Tick flag  (set by Timer1 ISR, consumed by main loop)
// ============================================================================

static volatile bool g_sysTick = false; ///< Set every 1 ms by Timer1 ISR

// ============================================================================
// Timer 1 ISR  – 1 ms CTC interrupt
// ============================================================================

/**
 * @brief Timer 1 compare-A interrupt service routine.
 *        Fires every 1 ms. Sets g_sysTick so that the scheduler runs in
 *        the main-thread context (safe for printf calls in tasks).
 *
 * Timer 1 CTC configuration (set up in Timer1_Init()):
 *   Clock = 16 MHz, prescaler = 64 → tick period = 4 µs
 *   OCR1A = 249  →  ISR period = 250 × 4 µs = 1 ms
 */
ISR(TIMER1_COMPA_vect)
{
    g_sysTick = true;
}

// ============================================================================
// Scheduler – sequential bare-metal OS implementation
// ============================================================================

/**
 * @brief Initialise one task descriptor and load its counter with the offset.
 */
static void os_seq_scheduler_task_init(Task_t* task,
                                       void (*task_func)(void),
                                       int rec,
                                       int offset)
{
    task->task_func = task_func;
    task->rec       = rec;
    task->offset    = offset;
    task->rec_cnt   = offset; // first activation delayed by offset ms
}

/**
 * @brief Set up the task table with recurence, offset and function pointers.
 *        Task order defines priority: Task1 (button) > Task2 (stats) > Task3 (report).
 *        Provider (Task1) is listed before its consumer (Task2).
 */
static void os_seq_scheduler_setup(void)
{
    os_seq_scheduler_task_init(&tasks[TASK_BUTTON_ID], Task1_ButtonMonitor,
                               REC_BUTTON, OFFS_BUTTON);

    os_seq_scheduler_task_init(&tasks[TASK_STATS_ID],  Task2_StatsAndLeds,
                               REC_STATS,  OFFS_STATS);

    os_seq_scheduler_task_init(&tasks[TASK_REPORT_ID], Task3_PeriodicReport,
                               REC_REPORT, OFFS_REPORT);
}

/**
 * @brief Main scheduler body – called once per 1 ms sys-tick from main loop.
 *
 *        Each task's down-counter is decremented; when it reaches zero the
 *        task function is executed and the counter is reloaded with the
 *        configured period (recurence).  Offset is applied only on first
 *        activation via os_seq_scheduler_task_init().
 */
static void os_seq_scheduler_loop(void)
{
    for (int i = 0; i < MAX_OF_TASKS; i++)
    {
        if (--tasks[i].rec_cnt <= 0)
        {
            tasks[i].rec_cnt = tasks[i].rec; // reload period
            tasks[i].task_func();            // execute task (non-preemptive)
        }
    }
}

// ============================================================================
// Timer 1 hardware initialisation
// ============================================================================

/**
 * @brief Configure Timer 1 for CTC mode with 1 ms period at 16 MHz.
 *
 *   Prescaler = 64  →  timer tick = 4 µs
 *   OCR1A     = 249 →  ISR fires every 250 ticks × 4 µs = 1 ms
 */
static void Timer1_Init(void)
{
    cli();                                              // disable global interrupts
    TCCR1A = 0;                                         // normal port operation
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); // CTC, prescaler 64
    OCR1A  = 249;                                       // compare value for 1 ms
    TIMSK1 = (1 << OCIE1A);                             // enable compare-A interrupt
    sei();                                              // re-enable global interrupts
}

// ============================================================================
// Task Implementations
// ============================================================================

/**
 * @brief Task 1 – Button Detection & Duration Measurement
 *        Period: 20 ms  |  Offset: 0 ms  |  Priority: HIGHEST (provider)
 *
 *        Calls ButtonDriver::Update() for one debounce sample per activation.
 *        On a confirmed release event:
 *          • Reads press duration from the driver.
 *          • Classifies as short (< 500 ms) or long (≥ 500 ms).
 *          • Publishes press data to shared variables for Task2 to consume.
 *          • Prints a one-line press notification via printf.
 */
static void Task1_ButtonMonitor(void)
{
    button.Update();

    if (button.WasJustReleased())
    {
        uint32_t dur     = button.GetLastPressDuration();
        bool     isShort = (dur < SHORT_PRESS_THRESHOLD_MS);

        g_pressDuration = dur;
        g_pressIsShort  = isShort;
        g_pressEvent    = true;

        printf("[T1] Press: %lu ms (%s)\n", dur, isShort ? "SHORT" : "LONG");
    }
}

/**
 * @brief Task 2 – Statistics & LED Signalling
 *        Period: 50 ms | Offset: 10 ms | Priority: MEDIUM
 *
 *        On short press: GREEN on, RED off, yellow blinks 5 times.
 *        On long  press: RED   on, GREEN off, yellow blinks 10 times.
 *        Yellow is ON while button is held (steady), blinks after release.
 */
static void Task2_StatsAndLeds(void)
{
    bool pressedNow = button.IsPressed();

    // ---- Red/Green LEDs: active only during an ongoing press ----
    if (pressedNow && !s_prevPressed)
    {
        s_pressStartMs = millis();
    }

    if (pressedNow)
    {
        uint32_t heldMs = millis() - s_pressStartMs;
        if (heldMs < SHORT_PRESS_THRESHOLD_MS)
        {
            ledGreen.On();
            ledRed.Off();
        }
        else
        {
            ledRed.On();
            ledGreen.Off();
        }
    }
    else
    {
        ledGreen.Off();
        ledRed.Off();
    }

    // ---- Consume press event (read-then-clear) ----
    if (g_pressEvent)
    {
        g_pressEvent = false;

        g_totalPresses++;
        g_totalDuration += g_pressDuration;

        if (g_pressIsShort)
        {
            g_shortPresses++;
            s_yellowTogglesLeft = 10; // 5 blinks x 2 toggles
        }
        else
        {
            g_longPresses++;
            s_yellowTogglesLeft = 20; // 10 blinks x 2 toggles
        }

        // Start blink sequence from a known state for consistent pulse count.
        ledYellow.Off();
    }

    // ---- Yellow LED: blink after release, or steady ON while held ----
    if (s_yellowTogglesLeft > 0)
    {
        ledYellow.Toggle();
        s_yellowTogglesLeft--;
    }
    else if (pressedNow)
    {
        ledYellow.On();
    }
    else
    {
        ledYellow.Off();
    }

    s_prevPressed = pressedNow;
}

/**
 * @brief Task 3 – Periodic Statistics Report via Serial
 *        Period: 10 000 ms  |  Offset: 100 ms  |  Priority: LOWEST (reporter)
 *
 *        Reads accumulated statistics written by Task 2, prints a formatted
 *        report via printf, then resets all counters.
 *        This implements the 10-second periodic reporting requirement.
 */
static void Task3_PeriodicReport(void)
{
    // Snapshot statistics
    uint16_t total  = g_totalPresses;
    uint16_t shorts = g_shortPresses;
    uint16_t longs  = g_longPresses;
    uint32_t durSum = g_totalDuration;

    uint32_t avgDur = (total > 0u) ? (durSum / total) : 0u;

    // --- Print report via printf ---
    printf("--- Button Statistics ---\n");
    printf("Total presses   : %u\n", total);
    printf("Short (<500ms)  : %u\n", shorts);
    printf("Long  (>=500ms) : %u\n", longs);
    printf("Avg duration    : %lu ms\n", avgDur);
    printf("-------------------------\n");

    // --- Reset statistics (Task 3 acts as consumer of Task 2 counters) ---
    g_totalPresses  = 0;
    g_shortPresses  = 0;
    g_longPresses   = 0;
    g_totalDuration = 0;
}

// ============================================================================
// Public entry points (called from main.cpp)
// ============================================================================

/**
 * @brief One-time hardware and scheduler initialisation for Lab 2.
 *        Called from Arduino setup().
 */
void lab2_setup(void)
{
    ledGreen.Init();
    ledRed.Init();
    ledYellow.Init();
    button.Init();

    SerialStdioInit(9600);
    printf("[Lab2] Scheduler starting. BTN=D3 GREEN=D10 RED=D11 YELLOW=D12\n");
    printf("[Lab2] Short(<500ms)->GREEN+5blinks | Long(>=500ms)->RED+10blinks\n");

    os_seq_scheduler_setup();
    Timer1_Init();
}

/**
 * @brief Main application loop for Lab 2.
 *        Called from Arduino loop().
 *
 *        Checks for the 1 ms sys-tick flag set by Timer1 ISR and delegates
 *        execution to the sequential scheduler.  Tasks therefore run in
 *        main-thread context — safe for Serial I/O.
 */
void lab2_loop(void)
{
    if (g_sysTick)
    {
        g_sysTick = false;           // consume tick
        os_seq_scheduler_loop();     // run scheduler (may invoke 0–3 tasks)
    }
}
