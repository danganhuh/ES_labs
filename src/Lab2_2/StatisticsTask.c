/**
 * @file StatisticsTask.c
 * @brief Pure-C FreeRTOS task – updates stats, drives indicator LEDs
 */

#include "StatisticsTask.h"
#include "Lab2_2_Shared.h"
#include "avr_helpers.h"

#include <stdio.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

void TaskStatistics(void *pv)
{
    (void)pv;
    if (xSemaphoreTake(g_shared.ioMutex, portMAX_DELAY) == pdTRUE)
    {
        printf("[Stats] === Task started ===\n");
        xSemaphoreGive(g_shared.ioMutex);
    }

    for (;;)
    {
        PressEventData eventData;

        /* Block until a full press event is available */
        if (xQueueReceive(g_shared.pressEventQueue,
                          &eventData,
                          portMAX_DELAY) == pdTRUE)
        {
            uint8_t  isS = eventData.isShort;

            uint8_t blinks = 0;

            if (isS)
                blinks = 5;
            else
                blinks = 10;

            /* LED feedback: green = short, red = long */
            if (isS)
            {
                hw_digital_write(LED_GREEN_PIN, PIN_HIGH);
                hw_digital_write(LED_RED_PIN,   PIN_LOW);
            }
            else
            {
                hw_digital_write(LED_RED_PIN,   PIN_HIGH);
                hw_digital_write(LED_GREEN_PIN, PIN_LOW);
            }

            /* Yellow blink feedback: each blink = 250 ms on + 250 ms off */
            {
                uint8_t i;
                for (i = 0; i < blinks; i++)
                {
                    hw_digital_write(LED_YELLOW_PIN, PIN_HIGH);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    hw_digital_write(LED_YELLOW_PIN, PIN_LOW);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
            }

            /* All LEDs off after feedback sequence */
            hw_digital_write(LED_GREEN_PIN,  PIN_LOW);
            hw_digital_write(LED_RED_PIN,    PIN_LOW);
            hw_digital_write(LED_YELLOW_PIN, PIN_LOW);

            if (xSemaphoreTake(g_shared.ioMutex, portMAX_DELAY) == pdTRUE)
            {
                printf("[Stats] Event processed: %s, queue pending=%u\r\n",
                       isS ? "SHORT" : "LONG",
                      (unsigned)uxQueueMessagesWaiting(g_shared.pressEventQueue));
                xSemaphoreGive(g_shared.ioMutex);
            }
        }
    }
}
