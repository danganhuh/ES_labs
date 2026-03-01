/**
 * @file MeasurementTask.c
 * @brief Pure-C FreeRTOS task – polls button, debounces, signals press events
 */

#include "MeasurementTask.h"
#include "Lab2_2_Shared.h"
#include "avr_helpers.h"

#include <stdio.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>

void TaskMeasurement(void *pv)
{
    (void)pv;

    uint8_t  stablePressed = 0;
    uint8_t  matchCount    = 0;
    unsigned long pressStartMs = 0;
    unsigned long eventIndex = 0;

    for (;;)
    {
        uint8_t rawVal     = hw_digital_read(BTN_PIN);
        uint8_t rawPressed = (rawVal == PIN_LOW) ? 1 : 0;  /* active-low */

        /* ── Debounce state machine ─────────────────────────────── */
        if (rawPressed != stablePressed)
        {
            matchCount++;
            if (matchCount >= DEBOUNCE_SAMPLES)
            {
                matchCount    = 0;
                stablePressed = rawPressed;

                if (stablePressed)
                {
                    /* Button just pressed */
                    pressStartMs = hw_millis();
                }
                else
                {
                    /* Button just released */
                    unsigned long dur = hw_millis() - pressStartMs;
                    uint8_t isS = (dur < SHORT_THRESHOLD_MS) ? 1 : 0;
                    eventIndex++;

                    /* Update statistics immediately when event is detected */
                    if (xSemaphoreTake(g_shared.statsMutex,
                                      portMAX_DELAY) == pdTRUE)
                    {
                        g_shared.stats.totalPresses++;
                        g_shared.stats.totalDurationMs += (uint32_t)dur;

                        if (isS)
                        {
                            g_shared.stats.shortPresses++;
                            g_shared.stats.shortDurationMs += (uint32_t)dur;
                        }
                        else
                        {
                            g_shared.stats.longPresses++;
                            g_shared.stats.longDurationMs += (uint32_t)dur;
                        }

                        xSemaphoreGive(g_shared.statsMutex);
                    }

                    /* Queue one full event so bursts are not lost */
                    {
                        PressEventData eventData;
                        eventData.durationMs = (uint32_t)dur;
                        eventData.isShort    = isS;

                        if (xQueueSend(g_shared.pressEventQueue, &eventData, 0) != pdPASS)
                        {
                            printf("[WARN] LED event queue full\r\n");
                        }
                    }

                    printf(
                        "+---------------- Button Event ----------------+\r\n"
                        "| Event ID : %-31lu |\r\n"
                        "| Type     : %-31s |\r\n"
                        "| Duration : %-27lu ms |\r\n"
                        "+----------------------------------------------+\r\n\r\n",
                        eventIndex,
                        isS ? "SHORT" : "LONG",
                        (unsigned long)dur
                    );
                }
            }
        }
        else
        {
            matchCount = 0;
        }

        vTaskDelay(1);  /* yield for 1 tick */
    }
}
