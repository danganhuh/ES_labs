/**
 * @file SerialStdioDriver.h
 * @brief ECAL Layer - STDIO Abstraction (Serial/LCD/Keypad)
 * 
 * Provides interfaces for standard I/O redirection.
 * Supports both serial-based and hardware-based (LCD/Keypad) I/O.
 * 
 * Features:
 * - SerialStdioInit(): Redirect printf/scanf to serial port
 * - StdioInit(): Redirect printf to LCD, scanf to Keypad
 * 
 * Usage:
 *   SerialStdioInit(9600);  // For serial-based labs (printf/scanf to UART)
 *   StdioInit();            // For LCD/Keypad labs (printf to LCD, scanf to Keypad)
 */

#ifndef SerialStdioDriver_H
#define SerialStdioDriver_H

#include <Arduino.h>

/**
 * @brief Initialize STDIO redirection to Serial port (UART)
 * 
 * Sets up UART at specified baud rate and redirects stdout/stdin to serial port.
 * This allows printf() to output to the serial monitor and scanf() to read from it.
 * 
 * @param baudRate Baud rate (typically 9600)
 */
void SerialStdioInit(unsigned long baudRate);

/**
 * @brief Initialize STDIO redirection to LCD and Keypad
 * 
 * Redirects stdout (printf) to LCD display and stdin (scanf) to Keypad input.
 * This allows printf() to output to the LCD and scanf() to read from the keypad.
 * 
 * Note: Must call lcd.Init() and keypad.Init() before calling this function.
 */
void StdioInit();

#endif
