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
 * - Redirects scanf() to receive input from serial
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
 * Non-blocking - characters are buffered by Serial class.
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

/**
 * @brief Callback function for scanf redirection
 * 
 * This function is called by scanf() to read each character.
 * It uses the Arduino MCAL Serial.read() to receive from UART.
 * Blocks until data is available on the serial port.
 * 
 * @param stream FILE stream (unused)
 * @return Character read from serial
 */
static int SerialGetChar(FILE *stream)
{
    // Block until data available on serial port
    while (!Serial.available()) {}
    // Read and return one byte from UART
    return Serial.read();  // MCAL: Read from Arduino serial
}

// Static FILE structures for redirecting stdout and stdin
static FILE SerialStdOut;
static FILE SerialStdIn;

/**
 * @brief Initialize serial communication at specified baud rate
 * 
 * Sets up:
 * - Arduino UART communication
 * - Waits for serial connection (important for USB boards)
 * - Redirects stdout to serial port for printf() support
 * - Redirects stdin to serial port for scanf() support
 * 
 * @param baudRate Baud rate (e.g., 9600)
 */
void SerialStdioInit(unsigned long baudRate)
{
    // Initialize UART at specified baud rate
    Serial.begin(baudRate);         // MCAL: Initialize serial
    while (!Serial) {}              // Wait for serial connection (important for USB)

    // Redirect stdout (printf) to serial port
    fdev_setup_stream(&SerialStdOut, SerialPutChar, NULL, _FDEV_SETUP_WRITE);
    stdout = &SerialStdOut;         // All printf() calls now go to serial

    // Redirect stdin (scanf) to serial port  
    fdev_setup_stream(&SerialStdIn, NULL, SerialGetChar, _FDEV_SETUP_READ);
    stdin = &SerialStdIn;           // All scanf() calls now read from serial
}

