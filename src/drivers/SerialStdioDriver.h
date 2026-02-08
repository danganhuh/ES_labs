/**
 * @file SerialStdioDriver.h
 * @brief ECAL Layer - Serial Communication & STDIO Abstraction
 * 
 * Provides a high-level interface for serial communication over UART,
 * including standard I/O redirection (printf/scanf to serial).
 * 
 * Features:
 * - Initialize serial communication at specified baud rate
 * - Redirect printf/FILE output to serial port
 * - Read complete lines from serial input
 * 
 * Usage:
 *   SerialStdioInit(9600);              // Initialize at 9600 baud
 *   printf("Hello World\r\n");          // Output via serial
 *   SerialReadLine(buffer, bufSize);    // Read line from serial
 */

#ifndef SerialStdioDriver_H
#define SerialStdioDriver_H

#include <Arduino.h>

/**
 * @brief Initialize serial communication and redirect STDIO
 * 
 * Sets up UART at specified baud rate and redirects stdout to serial port.
 * This allows printf() to output to the serial monitor.
 * 
 * @param baudRate Baud rate (typically 9600)
 */
void SerialStdioInit(unsigned long baudRate);

/**
 * @brief Read a complete line from serial input
 * 
 * Blocks until a complete line is received (terminated by \r or \n).
 * Stores the line (without the terminator) in the provided buffer.
 * 
 * @param buffer Pointer to character buffer to store the line
 * @param bufferSize Maximum bytes to read (including null terminator)
 */
void SerialReadLine(char *buffer, uint8_t bufferSize);

#endif
