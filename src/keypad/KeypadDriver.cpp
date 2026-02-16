/**
 * @file KeypadDriver.cpp
 * @brief ECAL Layer - Keypad Driver Implementation
 */

#include "KeypadDriver.h"

// Global keypad driver instance for scanf callback
KeypadDriver* g_keypadDriver = nullptr;

KeypadDriver::KeypadDriver(char (*userKeymap)[4], const byte* rows, const byte* cols, byte nRows, byte nCols)
    : rowPins(rows), colPins(cols), numRows(nRows), numCols(nCols), keymap(userKeymap), lastKey(0), lastKeyTime(0)
{
    keypad = nullptr;
}

void KeypadDriver::Init() {
    keypad = new Keypad(makeKeymap(keymap), rowPins, colPins, numRows, numCols);
    g_keypadDriver = this;  // Store global reference for scanf callback
}

char KeypadDriver::GetKey() {
    if (!keypad) return 0;
    
    char key = keypad->getKey();
    
    if (key) {
        unsigned long now = millis();
        if (key != lastKey || (now - lastKeyTime >= DEBOUNCE_DELAY)) {
            lastKey = key;
            lastKeyTime = now;
            return key;
        }
    } else {
        lastKey = 0;
    }
    
    return 0;
}

char KeypadDriver::GetChar() {
    // Blocking read - waits until a key is pressed
    char key = 0;
    while (key == 0) {
        key = GetKey();
        delay(10);  // Small delay to reduce CPU usage while waiting
    }
    return key;
}
