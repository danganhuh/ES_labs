/**
 * @file Lab7_2_main.cpp
 * @brief Lab7 Part 2 - Intelligent Traffic Light FSM Application
 *
 * Implements a multi-direction traffic light system with:
 * - Two independent FSMs (EW and NS directions)
 * - FreeRTOS task management
 * - Priority-based traffic control
 * - Button-driven North request
 *
 * Task Architecture:
 * - EW_FSM_Task / NS_FSM_Task: Update each Moore FSM
 * - Button_Task:               Debounce the NS request button
 * - Output_Task:               Drive LEDs from FSM outputs
 * - Display_Task:              Serial + LCD status reporting
 * - Priority_Task:             Apply NS request to FSM
 *
 * IMPORTANT: For the feilipu Arduino_FreeRTOS AVR port, the stack depth in
 * xTaskCreate is in BYTES (StackType_t = uint8_t). vprintf alone uses
 * ~150-200 bytes, so any task that prints needs >= 384 bytes of stack.
 */

#include "Lab7_2_main.h"
#include "TrafficLightFSM.h"
#include "Lab7_2_Shared.h"
#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <util/delay.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ============================================================================
// Global State and Semaphores
// ============================================================================

static SemaphoreHandle_t g_button_semaphore = NULL;
static SemaphoreHandle_t g_io_mutex = NULL;
static volatile bool g_ns_request_active = false;
static volatile uint32_t g_last_button_press_ms = 0;
static LiquidCrystal_I2C g_lcd(LAB7_2_LCD_I2C_ADDR, 16, 2);
static uint32_t g_last_lcd_update_ms = 0;
static bool g_lcd_ready = false;

typedef struct {
    uint8_t last_reading;
    uint32_t last_change_time_ms;
    uint8_t stable_reading;
} ButtonDebounceState;

static ButtonDebounceState g_button_state;

// ----------------------------------------------------------------------------
// Locked serial helpers
// ----------------------------------------------------------------------------
//
// We deliberately avoid printf / vprintf in tasks. On the feilipu AVR port a
// single vprintf call needs ~150-200 bytes of stack, which is too much for the
// small task stacks we can afford on the ATmega2560. Instead we use a tiny set
// of Serial.print helpers that take the I/O mutex once they are safe to use.

static bool io_lock(TickType_t wait_ticks)
{
    if (g_io_mutex == NULL) {
        return true;
    }
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return true;
    }
    return (xSemaphoreTake(g_io_mutex, wait_ticks) == pdTRUE);
}

static void io_unlock(void)
{
    if (g_io_mutex == NULL) {
        return;
    }
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return;
    }
    xSemaphoreGive(g_io_mutex);
}

static void log_status(uint32_t now_ms,
                       const char* sys,
                       char ew_state,
                       uint32_t ew_time_ms,
                       char ns_state,
                       uint32_t ns_time_ms,
                       bool ns_req)
{
    if (!io_lock(pdMS_TO_TICKS(20))) {
        return;
    }
    Serial.print('[');
    if (now_ms < 10000UL) { Serial.print('0'); }
    if (now_ms < 1000UL)  { Serial.print('0'); }
    if (now_ms < 100UL)   { Serial.print('0'); }
    if (now_ms < 10UL)    { Serial.print('0'); }
    Serial.print(now_ms);
    Serial.print(F("] SYS:"));
    Serial.print(sys);
    Serial.print(F(" EW:"));
    Serial.print(ew_state);
    Serial.print('(');
    Serial.print(ew_time_ms);
    Serial.print(F("ms) NS:"));
    Serial.print(ns_state);
    Serial.print('(');
    Serial.print(ns_time_ms);
    Serial.print(F("ms) REQ:"));
    Serial.println(ns_req ? F("Y") : F("N"));
    io_unlock();
}

// ============================================================================
// FreeRTOS Task: Button Input Handler
// ============================================================================

static void button_task(void* pvParameters) {
    (void)pvParameters;

    if (io_lock(pdMS_TO_TICKS(50))) {
        Serial.println(F("[TASK] Button"));
        io_unlock();
    }

    while (1) {
        const uint8_t current = digitalRead(LAB7_2_BUTTON_NS_REQUEST);
        const uint32_t now_ms = millis();

        if (current != g_button_state.last_reading) {
            g_button_state.last_reading = current;
            g_button_state.last_change_time_ms = now_ms;
        }

        if ((now_ms - g_button_state.last_change_time_ms) >= LAB7_2_DEBOUNCE_MS) {
            if (current != g_button_state.stable_reading) {
                g_button_state.stable_reading = current;
                g_last_button_press_ms = now_ms;

                if (current == 0) {
                    g_ns_request_active = true;
                } else {
                    g_ns_request_active = false;
                }
                xSemaphoreGive(g_button_semaphore);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(LAB7_2_DEBOUNCE_MS));
    }
}

// ============================================================================
// FreeRTOS Task: EW / NS FSM Update
// ============================================================================

static void ew_fsm_task(void* pvParameters) {
    (void)pvParameters;

    if (io_lock(pdMS_TO_TICKS(50))) {
        Serial.println(F("[TASK] EW_FSM"));
        io_unlock();
    }

    while (1) {
        TrafficLightFSM_Update(DIRECTION_EW);
        vTaskDelay(pdMS_TO_TICKS(LAB7_2_FSM_UPDATE_MS));
    }
}

static void ns_fsm_task(void* pvParameters) {
    (void)pvParameters;

    if (io_lock(pdMS_TO_TICKS(50))) {
        Serial.println(F("[TASK] NS_FSM"));
        io_unlock();
    }

    while (1) {
        TrafficLightFSM_Update(DIRECTION_NS);
        vTaskDelay(pdMS_TO_TICKS(LAB7_2_FSM_UPDATE_MS));
    }
}

// ============================================================================
// FreeRTOS Task: Output Control (LED Driver)
// ============================================================================

static void output_task(void* pvParameters) {
    (void)pvParameters;

    if (io_lock(pdMS_TO_TICKS(50))) {
        Serial.println(F("[TASK] Output"));
        io_unlock();
    }

    while (1) {
        const uint8_t ew_output = TrafficLightFSM_GetOutput(DIRECTION_EW);
        const uint8_t ns_output = TrafficLightFSM_GetOutput(DIRECTION_NS);

        digitalWrite(LAB7_2_LED_EW_RED,    (ew_output == 0) ? HIGH : LOW);
        digitalWrite(LAB7_2_LED_EW_GREEN,  (ew_output == 1) ? HIGH : LOW);
        digitalWrite(LAB7_2_LED_EW_YELLOW, (ew_output == 2) ? HIGH : LOW);

        digitalWrite(LAB7_2_LED_NS_RED,    (ns_output == 0) ? HIGH : LOW);
        digitalWrite(LAB7_2_LED_NS_GREEN,  (ns_output == 1) ? HIGH : LOW);
        digitalWrite(LAB7_2_LED_NS_YELLOW, (ns_output == 2) ? HIGH : LOW);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ============================================================================
// FreeRTOS Task: Display/Monitoring (Serial + LCD)
// ============================================================================

static void display_task(void* pvParameters) {
    (void)pvParameters;

    if (io_lock(pdMS_TO_TICKS(50))) {
        Serial.println(F("[TASK] Display"));
        io_unlock();
    }

    while (1) {
        const uint32_t now_ms = millis();
        const char ew = TrafficLightFSM_GetStateName(DIRECTION_EW)[0];
        const char ns = TrafficLightFSM_GetStateName(DIRECTION_NS)[0];
        const uint32_t ew_time_ms = TrafficLightFSM_GetStateTime(DIRECTION_EW);
        const uint32_t ns_time_ms = TrafficLightFSM_GetStateTime(DIRECTION_NS);
        const bool ns_req = g_ns_request_active;
        const char* sys = TrafficLightFSM_GetSystemState();

        log_status(now_ms, sys, ew, ew_time_ms, ns, ns_time_ms, ns_req);

        if (g_lcd_ready && (now_ms - g_last_lcd_update_ms) >= LAB7_2_LCD_UPDATE_MS) {
            g_last_lcd_update_ms = now_ms;

            char line1[17];
            char line2[17];
            const uint32_t req_age_s = ns_req ? ((now_ms - g_last_button_press_ms) / 1000UL) : 0UL;
            snprintf(line1, sizeof(line1), "EW:%c NS:%c", ew, ns);
            snprintf(line2, sizeof(line2), "REQ:%s %lus", ns_req ? "Y" : "N", req_age_s);

            if (io_lock(pdMS_TO_TICKS(50))) {
                g_lcd.clear();
                g_lcd.setCursor(0, 0);
                g_lcd.print(line1);
                g_lcd.setCursor(0, 1);
                g_lcd.print(line2);
                io_unlock();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(LAB7_2_DISPLAY_UPDATE_MS));
    }
}

// ============================================================================
// FreeRTOS Task: Priority Manager
// ============================================================================

static void priority_manager_task(void* pvParameters) {
    (void)pvParameters;

    if (io_lock(pdMS_TO_TICKS(50))) {
        Serial.println(F("[TASK] Priority"));
        io_unlock();
    }

    while (1) {
        if (xSemaphoreTake(g_button_semaphore, pdMS_TO_TICKS(200)) == pdTRUE) {
            const bool req = g_ns_request_active;
            TrafficLightFSM_SetNSRequest(req);

            if (io_lock(pdMS_TO_TICKS(20))) {
                Serial.print(F("[PRIORITY] NS_REQ "));
                Serial.println(req ? F("ACTIVATED") : F("RELEASED"));
                io_unlock();
            }
        }
    }
}

// ============================================================================
// Setup helpers
// ============================================================================

static void busy_delay_ms(uint16_t ms)
{
    while (ms > 0u) {
        _delay_ms(1);
        ms--;
    }
}

static bool create_task(TaskFunction_t fn,
                        const char* name,
                        uint16_t stack_bytes,
                        UBaseType_t priority)
{
    BaseType_t ok = xTaskCreate(fn, name, stack_bytes, NULL, priority, NULL);
    if (ok != pdPASS) {
        Serial.print(F("[FATAL] Task create failed: "));
        Serial.println(name);
        return false;
    }
    return true;
}

// ============================================================================
// Public Setup / Loop
// ============================================================================

void lab7_2_setup(void) {
    busy_delay_ms(50);

    Serial.println(F("[lab7_2] setup start"));

    // Create the I/O mutex BEFORE we touch the LCD or start any task. The
    // mutex is created here (not in the task) so that all code that uses
    // io_lock() / io_unlock() works consistently.
    g_io_mutex = xSemaphoreCreateMutex();
    g_button_semaphore = xSemaphoreCreateBinary();
    if (g_io_mutex == NULL || g_button_semaphore == NULL) {
        Serial.println(F("[FATAL] Could not create FreeRTOS objects"));
        for (;;) { busy_delay_ms(1000); }
    }

    Serial.println(F("========================================"));
    Serial.println(F("Lab7 Part 2: Intelligent Traffic Light"));
    Serial.println(F("========================================"));
    Serial.print(F("EW Lights G/Y/R: "));
    Serial.print(LAB7_2_LED_EW_GREEN);  Serial.print('/');
    Serial.print(LAB7_2_LED_EW_YELLOW); Serial.print('/');
    Serial.println(LAB7_2_LED_EW_RED);
    Serial.print(F("NS Lights G/Y/R: "));
    Serial.print(LAB7_2_LED_NS_GREEN);  Serial.print('/');
    Serial.print(LAB7_2_LED_NS_YELLOW); Serial.print('/');
    Serial.println(LAB7_2_LED_NS_RED);
    Serial.print(F("Button (NS Req): "));
    Serial.println(LAB7_2_BUTTON_NS_REQUEST);
    Serial.println(F("========================================"));

    pinMode(LAB7_2_BUTTON_NS_REQUEST, INPUT_PULLUP);

    pinMode(LAB7_2_LED_EW_GREEN,  OUTPUT);
    pinMode(LAB7_2_LED_EW_YELLOW, OUTPUT);
    pinMode(LAB7_2_LED_EW_RED,    OUTPUT);
    pinMode(LAB7_2_LED_NS_GREEN,  OUTPUT);
    pinMode(LAB7_2_LED_NS_YELLOW, OUTPUT);
    pinMode(LAB7_2_LED_NS_RED,    OUTPUT);

    digitalWrite(LAB7_2_LED_EW_GREEN,  LOW);
    digitalWrite(LAB7_2_LED_EW_YELLOW, LOW);
    digitalWrite(LAB7_2_LED_EW_RED,    LOW);
    digitalWrite(LAB7_2_LED_NS_GREEN,  LOW);
    digitalWrite(LAB7_2_LED_NS_YELLOW, LOW);
    digitalWrite(LAB7_2_LED_NS_RED,    LOW);

    // Seed the debounce state from the actual pin reading so we don't fire a
    // bogus "released" event a few ms after boot.
    const uint8_t initial = digitalRead(LAB7_2_BUTTON_NS_REQUEST);
    g_button_state.last_reading        = initial;
    g_button_state.stable_reading      = initial;
    g_button_state.last_change_time_ms = millis();
    g_ns_request_active = (initial == 0);

#if LAB7_2_ENABLE_LCD
    Serial.println(F("[lab7_2] lcd init"));
    Wire.begin();
    busy_delay_ms(20);
    g_lcd.init();
    g_lcd.backlight();
    g_lcd.clear();
    g_lcd.setCursor(0, 0);
    g_lcd.print(F("Lab7.2 Traffic"));
    g_lcd.setCursor(0, 1);
    g_lcd.print(F("Boot..."));
    g_lcd_ready = true;
    Serial.println(F("[lab7_2] lcd ok"));
#else
    g_lcd_ready = false;
    Serial.println(F("[lab7_2] lcd disabled"));
#endif

    TrafficLightFSM_Init();
    Serial.print(F("FSM init  EW="));
    Serial.print(TrafficLightFSM_GetStateName(DIRECTION_EW));
    Serial.print(F("  NS="));
    Serial.println(TrafficLightFSM_GetStateName(DIRECTION_NS));

    Serial.println(F("Starting tasks..."));

    if (!create_task(button_task,           "Button",   LAB7_2_BUTTON_STACK,   LAB7_2_BUTTON_PRIORITY))    { for(;;){ busy_delay_ms(1000);} }
    if (!create_task(ew_fsm_task,           "EW_FSM",   LAB7_2_EW_FSM_STACK,   LAB7_2_EW_FSM_PRIORITY))    { for(;;){ busy_delay_ms(1000);} }
    if (!create_task(ns_fsm_task,           "NS_FSM",   LAB7_2_NS_FSM_STACK,   LAB7_2_NS_FSM_PRIORITY))    { for(;;){ busy_delay_ms(1000);} }
    if (!create_task(output_task,           "Output",   LAB7_2_OUTPUT_STACK,   1))                          { for(;;){ busy_delay_ms(1000);} }
    if (!create_task(display_task,          "Display",  LAB7_2_DISPLAY_STACK,  LAB7_2_DISPLAY_PRIORITY))   { for(;;){ busy_delay_ms(1000);} }
    if (!create_task(priority_manager_task, "Priority", LAB7_2_PRIORITY_STACK, 2))                          { for(;;){ busy_delay_ms(1000);} }

    Serial.println(F("Tasks created. Starting scheduler..."));
    Serial.flush();

    vTaskStartScheduler();

    // Should never reach here.
    Serial.println(F("[FATAL] Scheduler returned"));
    for (;;) { busy_delay_ms(1000); }
}

void lab7_2_loop(void) {
    // Empty. Everything runs from FreeRTOS tasks.
}
