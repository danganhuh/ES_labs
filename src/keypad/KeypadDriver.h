/**
 * @file KeypadDriver.h
 * @brief ECAL Layer - Keypad Hardware Abstraction Driver
 *
 * Provides a high-level interface for reading a 4x4 matrix keypad.
 *
 * Usage:
 *   KeypadDriver keypad(rowPins, colPins);
 *   keypad.Init();
 *   char key = keypad.GetKey();
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
};

#endif // KEYPADDRIVER_H
