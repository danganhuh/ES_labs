/**
 * @file ReportingTask.c
 * @brief Pure-C FreeRTOS task – periodic serial statistics report
 */

#include "ReportingTask.h"
#include "Lab2_2_Shared.h"
#include "avr_helpers.h"

#include <stdio.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <semphr.h>

void TaskReporting(void *pv)
{
    (void)pv;
    if (xSemaphoreTake(g_shared.ioMutex, portMAX_DELAY) == pdTRUE)
    {
        printf("[Report] === Task started ===\r\n");
        xSemaphoreGive(g_shared.ioMutex);
    }

    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        uint16_t tp = 0, sp = 0, lp = 0;
        uint32_t td = 0;

        if (xSemaphoreTake(g_shared.statsMutex, portMAX_DELAY) == pdTRUE)
        {
            tp = g_shared.stats.totalPresses;
            sp = g_shared.stats.shortPresses;
            lp = g_shared.stats.longPresses;
            td = g_shared.stats.totalDurationMs;

            /* Reset for next reporting window */
            g_shared.stats.totalPresses    = 0;
            g_shared.stats.shortPresses    = 0;
            g_shared.stats.longPresses     = 0;
            g_shared.stats.totalDurationMs = 0;
            g_shared.stats.shortDurationMs = 0;
            g_shared.stats.longDurationMs  = 0;
            xSemaphoreGive(g_shared.statsMutex);
        }

        {
            uint32_t avg = (tp > 0) ? (td / tp) : 0;

            if (xSemaphoreTake(g_shared.ioMutex, portMAX_DELAY) == pdTRUE)
            {
                printf(
                    "+------------------------------------------+\r\n"
                    "|            Button Statistics             |\r\n"
                    "+------------------------------------------+\r\n"
                    "| Total presses         : %-12u |\r\n"
                    "| Short presses         : %-12u |\r\n"
                    "| Long presses          : %-12u |\r\n"
                    "| Average duration (ms) : %-12lu |\r\n"
                    "+------------------------------------------+\r\n\r\n",
                    tp,
                    sp,
                    lp,
                    (unsigned long)avg
                );
                xSemaphoreGive(g_shared.ioMutex);
            }
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(10000));
    }
}
