/**
 * @file SerialStdioDriver.h
 * @brief ECAL Layer - Serial Communication & STDIO Abstraction
 * 
 * Provides a simple interface for serial communication over UART,
 * using standard I/O redirection (printf/scanf to serial).
 * 
 * Features:
 * - Initialize serial communication at specified baud rate
 * - Redirect printf/scanf to serial port via stdio
 * 
 * Usage:
 *   SerialStdioInit(9600);              // Initialize at 9600 baud
 *   printf("Hello World\r\n");          // Output via serial
 *   scanf("%s", buffer);                // Read input via serial
 */

#ifndef SerialStdioDriver_H
#define SerialStdioDriver_H

#include <Arduino.h>

/**
 * @brief Initialize serial communication and redirect STDIO
 * 
 * Sets up UART at specified baud rate and redirects stdout and stdin to serial port.
 * This allows printf() to output to the serial monitor and scanf() to read from it.
 * 
 * @param baudRate Baud rate (typically 9600)
 */
void SerialStdioInit(unsigned long baudRate);

#endif
