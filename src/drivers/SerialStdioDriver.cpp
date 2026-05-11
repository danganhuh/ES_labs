/**
 * @file SerialStdioDriver.cpp
 * @brief ECAL Layer - Serial/LCD/Keypad Driver Implementation
 *
 * Provides simple Serial I/O wrappers and LCD/Keypad I/O redirection.
 * Uses Arduino MCAL hardware drivers through LcdDriver and KeypadDriver.
 *
 * Features:
 * - SerialStdioInit(): Initialises UART via Serial.begin()
 * - SerialReadLine(): Reads a newline-terminated line from Serial
 * - StdioInit(): Redirects printf() to LCD, scanf() from Keypad
 *
 * FreeRTOS: When the scheduler is running, each byte written through printf
 * is protected by a mutex so multiple tasks cannot interleave on
 * Serial.write() (which produces garbled monitors like "ain[main[ma...").
 *
 * Architecture: Lab -> StdioDriver -> UART or LCD/Keypad -> MCAL -> Hardware
 */

#include "SerialStdioDriver.h"
#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#include <stdio.h>
#include <task.h>

// ============================================================================
// Static FILE streams for UART stdio redirection
// ============================================================================

static FILE UartStdOut;

/** TX mutex: one complete stream of bytes per "line" of printf at a time. */
static SemaphoreHandle_t s_uartTxMutex = NULL;

/**
 * @brief Callback: write one character to the UART (used by printf)
 */
static int UartPutChar(char c, FILE *stream)
{
    (void)stream;

    if (s_uartTxMutex != NULL &&
        xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        if (xSemaphoreTake(s_uartTxMutex, portMAX_DELAY) == pdTRUE)
        {
            Serial.write((uint8_t)c);
            xSemaphoreGive(s_uartTxMutex);
        }
    }
    else
    {
        Serial.write((uint8_t)c);
    }
    return 0;
}

/**
 * @brief Callback: read one character from the UART (used by scanf)
 *        Blocks until a byte is available.
 */
static int UartGetChar(FILE *stream)
{
    (void)stream;

    for (;;)
    {
        if (Serial.available() > 0)
        {
            break;
        }
        /* Let TX and other tasks run while waiting (avoids starving printf). */
        if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        {
            taskYIELD();
        }
    }

    int value = -1;
    if (s_uartTxMutex != NULL &&
        xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        if (xSemaphoreTake(s_uartTxMutex, portMAX_DELAY) == pdTRUE)
        {
            value = Serial.read();
            xSemaphoreGive(s_uartTxMutex);
        }
    }
    else
    {
        value = Serial.read();
    }
    return value;
}

/**
 * @brief Initialize the Serial port and redirect printf/scanf to UART
 *
 * Opens the UART at the requested baud rate, waits until the
 * host-side serial monitor has connected, then hooks stdout/stdin
 * to the UART so that printf() and scanf() work over serial.
 *
 * @param baudRate Baud rate (e.g., 9600)
 */
void SerialStdioInit(unsigned long baudRate)
{
    Serial.begin(baudRate);

    unsigned long startMs = millis();
    while (!Serial && (millis() - startMs < 1500UL)) {}

    if (s_uartTxMutex == NULL)
    {
        s_uartTxMutex = xSemaphoreCreateMutex();
    }

    fdev_setup_stream(&UartStdOut, UartPutChar, UartGetChar, _FDEV_SETUP_RW);
    stdout = &UartStdOut;
    stdin  = &UartStdOut;
}

/**
 * @brief Read one line from Serial into a caller-supplied buffer
 *
 * Blocks until a newline (\\n) or carriage-return (\\r) is received,
 * or until maxLen-1 characters have been stored.  The terminating
 * newline/carriage-return is NOT stored.  The buffer is always
 * null-terminated.
 *
 * @param buf     Destination buffer
 * @param maxLen  Size of buf (including space for the null terminator)
 */
void SerialReadLine(char* buf, int maxLen)
{
    int index = 0;
    while (index < maxLen - 1)
    {
        while (!Serial.available()) {}
        char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r')
        {
            break;
        }
        buf[index++] = c;
    }
    buf[index] = '\0';
}

/**
 * @brief Initialize STDIO redirection to LCD and Keypad
 *
 * In this build profile, serial stdio is used; LCD/Keypad redirection
 * is intentionally not enabled to avoid optional hardware dependencies.
 */
void StdioInit()
{
    /* No-op for Lab3 serial-stdio mode. */
}
