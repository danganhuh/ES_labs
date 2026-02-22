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
// SERIAL-BASED I/O FUNCTIONS (for serial communication)
// ============================================================================

// ============================================================================
// Static FILE streams for UART stdio redirection
// ============================================================================

static FILE UartStdOut;

/**
 * @brief Callback: write one character to the UART (used by printf)
 */
static int UartPutChar(char c, FILE *stream)
{
    Serial.write(c);
    return 0;
}

/**
 * @brief Callback: read one character from the UART (used by scanf)
 *        Blocks until a byte is available.
 */
static int UartGetChar(FILE *stream)
{
    while (!Serial.available()) {}
    return Serial.read();
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
    Serial.begin(baudRate);   // MCAL: Initialize serial
    while (!Serial) {}        // Wait for serial connection

    // Redirect printf (stdout) and scanf (stdin) to UART
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
        while (!Serial.available()) {}       // block until a byte arrives
        char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r')
        {
            break;                           // end of line
        }
        buf[index++] = c;
    }
    buf[index] = '\0';
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

