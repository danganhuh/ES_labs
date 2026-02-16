/**
 * @file KeypadDriver.h
 * @brief ECAL Layer - Keypad Hardware Abstraction Driver with scanf support
 *
 * Provides a high-level interface for reading a 4x4 matrix keypad.
 * Also supports scanf redirection to keypad input.
 *
 * Usage:
 *   KeypadDriver keypad(rowPins, colPins);
 *   keypad.Init();
 *   char key = keypad.GetKey();
 *   scanf("%s", buffer);  // When stdin is redirected
 */

#ifndef KEYPADDRIVER_H
#define KEYPADDRIVER_H

#include <Arduino.h>
#include <Keypad.h>

class KeypadDriver {
private:
    Keypad* keypad;
    const byte* rowPins;
    const byte* colPins;
    byte numRows;
    byte numCols;
    char (*keymap)[4];
    char lastKey;
    unsigned long lastKeyTime;
    static const unsigned long DEBOUNCE_DELAY = 20;
public:
    KeypadDriver(char (*userKeymap)[4], const byte* rows, const byte* cols, byte nRows, byte nCols);
    void Init();
    char GetKey();
    char GetChar();  // For scanf redirection - blocks until key is available
};

// Global keypad driver instance for scanf callback
extern KeypadDriver* g_keypadDriver;

#endif // KEYPADDRIVER_H
