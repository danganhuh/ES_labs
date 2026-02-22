/**
 * @file Lab2_main.cpp
 * @brief SRV Layer - Lab 2: Button Duration Monitor with Sequential Bare-Metal Scheduler
 *
 * =============================================================================
 * TASK DESCRIPTION
 * =============================================================================
 * Implements a button press monitoring application structured as 3 sequential
 * bare-metal tasks managed by a timer-interrupt-driven scheduler.
 *
 * Task 1 – Button Detection & Duration Measurement (every 20 ms, offset 0 ms)
 *   • Calls ButtonDriver::Update() for non-blocking debounce.
 *   • On button release: measures press duration (ms), classifies short/long.
 *   • Publishes press event and duration to shared variables (provider role).
 *   • Notifies via printf: "[T1] Press detected: <N> ms (SHORT|LONG)".
 *
 * Task 2 – Statistics & LED Blink (every 50 ms, offset 10 ms)
 *   • Consumer of press events produced by Task 1.
 *   • On new press: increments counters, accumulates total duration.
 *   • Short press: schedules  5 LED blinks (10 toggles × 50 ms = 500 ms).
 *   • Long  press: schedules 10 LED blinks (20 toggles × 50 ms = 1 000 ms).
 *   • Each invocation advances the blink state machine by one toggle (non-blocking).
 *
 * Task 3 – Periodic Report via printf (every 10 000 ms, offset 100 ms)
 *   • Prints via printf: total presses, short, long, average duration.
 *   • Resets all statistics counters after printing.
 *
 * =============================================================================
 * SCHEDULER – Part 1: Non-Preemptive Bare-Metal Sequential OS
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
 *  Green LED      |  10         | 220 Ω series resistor → GND
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

static const uint8_t BTN_PIN  = 3;  ///< Push-button (INPUT_PULLUP, active low)
static const uint8_t LED_PIN  = 10; ///< Green LED – visual feedback (single LED)

// ============================================================================
// Scheduler Configuration
// ============================================================================

/** Task recurring period in ms */
#define REC_BUTTON   20      ///< Task 1: 20 ms – debounce needs 5 × 20 ms = 100 ms
#define REC_BLINK    50      ///< Task 2: 50 ms – one blink-toggle per call
#define REC_REPORT   10000   ///< Task 3: 10 s  – periodic statistics report
#define DEBUG_BTN_CHECK_TIMEOUT_MS 5000  ///< Setup debug button check window

/** Task initial offset in ms (delay before first execution) */
#define OFFS_BUTTON  0       ///< Task 1: no offset – runs at tick 0
#define OFFS_BLINK   10      ///< Task 2: 10 ms offset – interleaved with Task 1
#define OFFS_REPORT  100     ///< Task 3: 100 ms offset – startup noise margin

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

// Forward declarations of task functions
static void Task1_ButtonMonitor(void);
static void Task2_StatsAndBlink(void);
static void Task3_PeriodicReport(void);

/** Global task table (initialised in os_seq_scheduler_setup) */
static Task_t tasks[MAX_OF_TASKS];

// ============================================================================
// Hardware Driver Instances  (ECAL layer – reused from project drivers)
// ============================================================================

static LedDriver    led   (LED_PIN);  ///< Single green LED for all visual feedback
static ButtonDriver button(BTN_PIN, true, true);

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
// Task 2 private state  (blink state machine – must persist between calls)
// ============================================================================

/** Number of remaining LED toggle steps (2 toggles = 1 visible blink) */
static uint8_t s_blinkTogglesLeft = 0;

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

    os_seq_scheduler_task_init(&tasks[TASK_STATS_ID],  Task2_StatsAndBlink,
                               REC_BLINK,  OFFS_BLINK);

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
    // Advance debounce state machine (non-blocking – just one sample)
    button.Update();

    // Check for completed press/release cycle
    if (button.WasJustReleased())
    {
        uint32_t dur     = button.GetLastPressDuration();
        bool     isShort = (dur < 500UL);

        // --- Publish press data (provider writes shared variables) ---
        g_pressDuration = dur;
        g_pressIsShort  = isShort;
        g_pressEvent    = true;  // signal consumer task

        // --- Notify via printf ---
        printf("[T1] Press detected: %lu ms (%s)\n", dur, isShort ? "SHORT" : "LONG");
    }
}

/**
 * @brief Task 2 – Statistics Counter & LED Blink
 *        Period: 50 ms  |  Offset: 10 ms  |  Priority: MEDIUM (consumer)
 *
 *        Consumes press events published by Task 1:
 *          • Updates global counters and total duration.
 *          • Arms the blink state machine on the single LED:
 *              – Short press →  5 blinks (10 toggles × 50 ms = 500 ms)
 *              – Long  press → 10 blinks (20 toggles × 50 ms = 1 000 ms)
 *
 *        Each call advances the blink state machine by one toggle (non-blocking).
 *        LED is forced off when the blink sequence completes.
 */
static void Task2_StatsAndBlink(void)
{
    // ---- Consume press event (read-then-clear) ----
    if (g_pressEvent)
    {
        g_pressEvent = false; // consume the flag

        g_totalPresses++;
        g_totalDuration += g_pressDuration;

        if (g_pressIsShort)
        {
            g_shortPresses++;
            s_blinkTogglesLeft = 10; //  5 blinks × 2 toggles
        }
        else
        {
            g_longPresses++;
            s_blinkTogglesLeft = 20; // 10 blinks × 2 toggles
        }
    }

    // ---- Single LED blink state machine (non-blocking one step) ----
    if (s_blinkTogglesLeft > 0)
    {
        led.Toggle();            // one toggle step every 50 ms
        s_blinkTogglesLeft--;
    }
    else
    {
        led.Off();               // keep LED off when idle
    }
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
    // --- Initialise ECAL hardware drivers ---
    led.Init();
    button.Init();

    // --- Debug: show LED ON at startup ---
    led.On();
    delay(1000);

    // --- Initialise Serial and redirect printf/scanf ---
    SerialStdioInit(9600);
    printf("[Lab2] Sequential bare-metal scheduler starting...\n");
    printf("[Lab2] Press button to begin. Report every 10 s.\n");

    // --- Debug: startup button self-check ---
    printf("[DBG] Button self-check: press and hold D3 button now...\n");
    uint32_t checkStart = millis();
    bool buttonDetected = false;
    while ((millis() - checkStart) < DEBUG_BTN_CHECK_TIMEOUT_MS)
    {
        button.Update();
        if (button.IsPressed())
        {
            buttonDetected = true;
            break;
        }
        delay(20);
    }

    if (buttonDetected)
    {
        printf("[DBG] Button check PASS (D3 press detected).\n");
    }
    else
    {
        printf("[DBG] Button check FAIL (no press). Check D3 <-> GND wiring.\n");
    }

    // --- Set up sequential task scheduler ---
    os_seq_scheduler_setup();

    // --- Start Timer 1 for 1 ms sys-tick ---
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
