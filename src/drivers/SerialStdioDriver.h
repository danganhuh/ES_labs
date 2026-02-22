/**
 * @file SerialStdioDriver.h
 * @brief ECAL Layer - Serial/LCD/Keypad I/O Abstraction
 *
 * Provides interfaces for Serial I/O (using Arduino Serial.print / Serial.read)
 * and for hardware-based (LCD/Keypad) I/O.
 *
 * Features:
 * - SerialStdioInit(): Open UART via Serial.begin()
 * - SerialReadLine(): Read a newline-terminated line from the serial monitor
 * - StdioInit(): Redirect printf to LCD, scanf to Keypad
 *
 * Usage (serial labs):
 *   SerialStdioInit(9600);
 *   Serial.print("Hello");
 *   char buf[64];
 *   SerialReadLine(buf, sizeof(buf));
 *
 * Usage (LCD/Keypad lab):
 *   StdioInit();   // after lcd.Init() and keypad.Init()
 */

#ifndef SerialStdioDriver_H
#define SerialStdioDriver_H

#include <Arduino.h>

/**
 * @brief Initialise the Serial port
 *
 * Opens UART at the specified baud rate and waits for a host connection.
 *
 * @param baudRate Baud rate (typically 9600)
 */
void SerialStdioInit(unsigned long baudRate);

/**
 * @brief Read one newline-terminated line from the Serial monitor
 *
 * Blocks until '\\n' or '\\r' is received (or buf is full).
 * The line terminator is not stored.  The buffer is always null-terminated.
 *
 * @param buf     Destination buffer
 * @param maxLen  Size of buf (including null terminator)
 */
void SerialReadLine(char* buf, int maxLen);

/**
 * @brief Initialize STDIO redirection to LCD and Keypad
 *
 * Redirects stdout (printf) to LCD display and stdin (scanf) to Keypad input.
 *
 * Note: Must call lcd.Init() and keypad.Init() before calling this function.
 */
void StdioInit();

#endif
