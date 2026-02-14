/**
 * @file LcdDriver.h
 * @brief ECAL Layer - LCD Hardware Abstraction Driver
 *
 * Provides a high-level interface for controlling a 16x2 LCD.
 *
 * Usage:
 *   LcdDriver lcd(rs, en, d4, d5, d6, d7);
 *   lcd.Init();
 *   lcd.Print("Hello");
 */

#ifndef LCDDRIVER_H
#define LCDDRIVER_H

#include <Arduino.h>
#include <LiquidCrystal.h>

class LcdDriver {
private:
    LiquidCrystal* lcd;
    uint8_t rs, en, d4, d5, d6, d7;
public:
    LcdDriver(uint8_t rs, uint8_t en, uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);
    void Init();
    void Print(const char* l1, const char* l2 = "");
    void Clear();
    void SetCursor(uint8_t col, uint8_t row);
};

#endif // LCDDRIVER_H
