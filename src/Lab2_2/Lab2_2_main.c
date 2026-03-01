/**
 * @file Lab2_2_main.c
 * @brief Pure-C Lab 2.2 entry – creates FreeRTOS tasks, inits hardware
 */

#include "Lab2_2_main.h"
#include "Lab2_2_Shared.h"
#include "avr_helpers.h"
#include "MeasurementTask.h"
#include "StatisticsTask.h"
#include "ReportingTask.h"

#include <stdio.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

/* ── Global shared state (declared extern in Lab2_2_Shared.h) ──── */
SharedState g_shared;

void lab2_2_setup(void)
{
    /* --- Hardware pin init via C wrappers --- */
    hw_pin_mode(BTN_PIN, PIN_MODE_INPUT_PULLUP);
    hw_pin_mode(LED_GREEN_PIN, PIN_MODE_OUTPUT);
    hw_pin_mode(LED_RED_PIN,   PIN_MODE_OUTPUT);
    hw_pin_mode(LED_YELLOW_PIN, PIN_MODE_OUTPUT);

    hw_digital_write(LED_GREEN_PIN,  PIN_LOW);
    hw_digital_write(LED_RED_PIN,    PIN_LOW);
    hw_digital_write(LED_YELLOW_PIN, PIN_LOW);

    printf("[Lab2_2] FreeRTOS monitor starting\n");

    /* --- LED self-test (proves wiring works) --- */
    hw_digital_write(LED_GREEN_PIN, PIN_HIGH);
    hw_delay_ms(300);
    hw_digital_write(LED_GREEN_PIN, PIN_LOW);

    hw_digital_write(LED_RED_PIN, PIN_HIGH);
    hw_delay_ms(300);
    hw_digital_write(LED_RED_PIN, PIN_LOW);

    hw_digital_write(LED_YELLOW_PIN, PIN_HIGH);
    hw_delay_ms(300);
    hw_digital_write(LED_YELLOW_PIN, PIN_LOW);

    /* --- Zero shared state --- */
    g_shared.stats.totalPresses   = 0;
    g_shared.stats.shortPresses   = 0;
    g_shared.stats.longPresses    = 0;
    g_shared.stats.totalDurationMs = 0;
    g_shared.stats.shortDurationMs = 0;
    g_shared.stats.longDurationMs  = 0;

    /* --- Create synchronisation primitives --- */
    g_shared.pressEventQueue = xQueueCreate(PRESS_EVENT_QUEUE_LEN,
                                            sizeof(PressEventData));
    g_shared.statsMutex     = xSemaphoreCreateMutex();

    if (!g_shared.pressEventQueue || !g_shared.statsMutex)
    {
        printf("[Lab2_2] FATAL: RTOS primitive alloc failed!\n");
        for (;;) { /* halt */ }
    }
    printf("[Lab2_2] Queue + mutex OK\n");

    /* --- Create FreeRTOS tasks --- */
    BaseType_t ok;

    ok = xTaskCreate(TaskMeasurement, "BtnMeas", 512, (void *)0, 3, (TaskHandle_t *)0);
    if (ok != pdPASS) { printf("FAIL: BtnMeas\n"); for(;;); }
    printf("[Lab2_2] BtnMeas task created (pri 3, 512w)\n");

    ok = xTaskCreate(TaskStatistics,  "Stats",   512, (void *)0, 2, (TaskHandle_t *)0);
    if (ok != pdPASS) { printf("FAIL: Stats\n");   for(;;); }
    printf("[Lab2_2] Stats task created (pri 2, 512w)\n");

    ok = xTaskCreate(TaskReporting,   "Report",  512, (void *)0, 1, (TaskHandle_t *)0);
    if (ok != pdPASS) { printf("FAIL: Report\n");  for(;;); }
    printf("[Lab2_2] Report task created (pri 1, 512w)\n");

    printf("[Lab2_2] All tasks created - scheduler starts after setup()\n");
}

void lab2_2_loop(void)
{
    /* Called from idle-task hook – must NOT block */
}
