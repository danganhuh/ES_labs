/**
 * @file SerialStdioDriver.cpp
 * @brief ECAL Layer - Serial/LCD/Keypad Driver Implementation
 * 
 * Implements STDIO redirection for both serial and hardware (LCD/Keypad) I/O.
 * Uses Arduino MCAL hardware drivers through LcdDriver and KeypadDriver.
 * 
 * Features:
 * - SerialStdioInit(): Redirects printf() to serial, scanf() from serial
 * - StdioInit(): Redirects printf() to LCD, scanf() from Keypad
 * 
 * Architecture: Lab -> StdioDriver -> UART or LCD/Keypad -> MCAL -> Hardware
 */

#include "SerialStdioDriver.h"
#include "../lcd/LcdDriver.h"
#include "../keypad/KeypadDriver.h"
#include <stdio.h>

// Forward declarations of global driver instances
extern LcdDriver* g_lcdDriver;
extern KeypadDriver* g_keypadDriver;

// ============================================================================
// SERIAL-BASED STDIO FUNCTIONS (for serial communication)
// ============================================================================

/**
 * @brief Callback function for printf redirection to Serial
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

/**
 * @brief Callback function for scanf redirection from Serial
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

// Static FILE structures for serial redirection
static FILE SerialStdOut;
static FILE SerialStdIn;

/**
 * @brief Initialize STDIO redirection to Serial port
 * 
 * Sets up:
 * - Arduino UART communication at specified baud rate
 * - Redirects stdout to serial port for printf() support
 * - Redirects stdin to serial port for scanf() support
 * 
 * @param baudRate Baud rate (e.g., 9600)
 */
void SerialStdioInit(unsigned long baudRate)
{
    // Initialize UART at specified baud rate
    Serial.begin(baudRate);         // MCAL: Initialize serial
    while (!Serial) {}              // Wait for serial connection

    // Redirect stdout (printf) to serial port
    fdev_setup_stream(&SerialStdOut, SerialPutChar, NULL, _FDEV_SETUP_WRITE);
    stdout = &SerialStdOut;         // All printf() calls now go to serial

    // Redirect stdin (scanf) to serial port
    fdev_setup_stream(&SerialStdIn, NULL, SerialGetChar, _FDEV_SETUP_READ);
    stdin = &SerialStdIn;           // All scanf() calls now read from serial
}

// ============================================================================
// LCD/KEYPAD-BASED STDIO FUNCTIONS (for LCD/Keypad I/O)
// ============================================================================

/**
 * @brief Callback function for printf redirection to LCD
 * 
 * This function is called by printf() to output each character.
 * It uses the LcdDriver to display output on the LCD.
 * 
 * @param c Character to write
 * @param stream FILE stream (unused)
 * @return 0 on success
 */
static int LcdPutChar(char c, FILE *stream)
{
    if (g_lcdDriver) {
        g_lcdDriver->PutChar(c);
    }
    return 0;
}

/**
 * @brief Callback function for scanf redirection from Keypad
 * 
 * This function is called by scanf() to read each character.
 * It uses the KeypadDriver to get input from the keypad.
 * Blocks until a key is available on the keypad.
 * 
 * @param stream FILE stream (unused)
 * @return Character read from keypad
 */
static int KeypadGetChar(FILE *stream)
{
    // Block until key available on keypad
    if (g_keypadDriver) {
        int c = g_keypadDriver->GetChar();
        // Treat '#' as newline so scanf() sees end-of-input when user confirms
        if (c == '#') return '\n';
        return c;
    }
    return 0;
}

// Static FILE structures for LCD/Keypad redirection
static FILE LcdStdOut;
static FILE KeypadStdIn;

/**
 * @brief Initialize STDIO redirection to LCD and Keypad
 * 
 * Sets up:
 * - Redirects stdout to LCD display for printf() support
 * - Redirects stdin to Keypad for scanf() support
 * 
 * Prerequisites:
 * - g_lcdDriver must be initialized (lcd.Init() called)
 * - g_keypadDriver must be initialized (keypad.Init() called)
 */
void StdioInit()
{
    // Redirect stdout (printf) to LCD display
    fdev_setup_stream(&LcdStdOut, LcdPutChar, NULL, _FDEV_SETUP_WRITE);
    stdout = &LcdStdOut;         // All printf() calls now go to LCD

    // Redirect stdin (scanf) to Keypad input
    fdev_setup_stream(&KeypadStdIn, NULL, KeypadGetChar, _FDEV_SETUP_READ);
    stdin = &KeypadStdIn;        // All scanf() calls now read from keypad
}

