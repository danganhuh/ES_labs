/**
 * @file KeypadDriver.cpp
 * @brief ECAL Layer - Keypad Driver Implementation
 */

#include "KeypadDriver.h"

KeypadDriver::KeypadDriver(char (*userKeymap)[4], const byte* rows, const byte* cols, byte nRows, byte nCols)
    : rowPins(rows), colPins(cols), numRows(nRows), numCols(nCols), keymap(userKeymap), lastKey(0), lastKeyTime(0)
{
    keypad = nullptr;
}

void KeypadDriver::Init() {
    keypad = new Keypad(makeKeymap(keymap), rowPins, colPins, numRows, numCols);
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
