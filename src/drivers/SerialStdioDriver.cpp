/**
 * @file SerialStdioDriver.cpp
 * @brief ECAL Layer - Serial Driver Implementation
 * 
 * Implements serial communication with STDIO redirection.
 * Uses Arduino MCAL (Serial class) for UART communication.
 * 
 * Features:
 * - Configures Arduino serial port at specified baud rate
 * - Redirects printf() to send output over serial
 * - Provides blocking read for receiving complete lines
 * 
 * Architecture: Lab -> SerialStdioDriver -> UART MCAL -> Arduino Serial
 */

#include "SerialStdioDriver.h"
#include <stdio.h>

/**
 * @brief Callback function for printf redirection
 * 
 * This function is called by printf() to output each character.
 * It uses the Arduino MCAL Serial.write() to send to UART.
 * 
 * @param c Character to write
 * @param stream FILE stream (unused)
 * @return 0 on success
 */
static int SerialPutChar(char c, FILE *stream)
{
    Serial.write(c);  // MCAL: Write to Arduino serial
    return 0;
}

// Static FILE structure for redirecting stdout
static FILE SerialStdOut;

/**
 * @brief Initialize serial communication at specified baud rate
 * 
 * Sets up:
 * - Arduino UART communication
 * - Waits for serial connection (important for USB boards)
 * - Redirects stdout to serial port for printf() support
 * 
 * @param baudRate Baud rate (e.g., 9600)
 */
void SerialStdioInit(unsigned long baudRate)
{
    Serial.begin(baudRate);         // MCAL: Initialize serial
    while (!Serial) {}              // Wait for serial connection

    // Setup stdout redirection - printf outputs go to serial
    fdev_setup_stream(&SerialStdOut, SerialPutChar, NULL, _FDEV_SETUP_WRITE);
    stdout = &SerialStdOut;
}

/**
 * @brief Read a complete line from serial input (blocking)
 * 
 * Waits for data on serial port and reads characters until:
 * - Carriage return (\r), OR
 * - Newline (\n)
 * is received. The line is stored in the buffer with null terminator.
 * 
 * Note: This is a blocking function - it will wait indefinitely for input.
 * 
 * @param buffer Pointer to buffer to store the line
 * @param bufferSize Maximum number of characters (including null terminator)
 */
void SerialReadLine(char *buffer, uint8_t bufferSize)
{
    uint8_t index = 0;

    while (1)
    {
        // Check if data is available on serial port (MCAL)
        if (Serial.available())
        {
            char receivedChar = Serial.read();  // MCAL: Read one character

            // Check for line terminator
            if (receivedChar == '\r' || receivedChar == '\n')
            {
                buffer[index] = '\0';  // Null terminate the string
                return;                // Return complete line
            }

            // Store character if buffer has space
            if (index < bufferSize - 1)
            {
                buffer[index++] = receivedChar;
            }
        }
    }
}
