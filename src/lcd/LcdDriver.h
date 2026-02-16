/**
 * @file LcdDriver.h
 * @brief ECAL Layer - LCD Hardware Abstraction Driver with printf support
 *
 * Provides a high-level interface for controlling a 16x2 LCD.
 * Also supports printf redirection to LCD display.
 *
 * Usage:
 *   LcdDriver lcd(rs, en, d4, d5, d6, d7);
 *   lcd.Init();
 *   lcd.Print("Hello");
 *   printf("Output to LCD");  // When stdout is redirected
 */

#ifndef LCDDRIVER_H
#define LCDDRIVER_H

#include <Arduino.h>
#include <LiquidCrystal.h>

class LcdDriver {
private:
    LiquidCrystal* lcd;
    uint8_t rs, en, d4, d5, d6, d7;
    char printBuffer[33];  // Buffer for printf output (32 chars + null terminator)
    uint8_t bufferIndex;
    unsigned long lastUpdateTime;
    static const unsigned long UPDATE_INTERVAL = 500;  // Update display every 500ms during printing
public:
    LcdDriver(uint8_t rs, uint8_t en, uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);
    void Init();
    void Print(const char* l1, const char* l2 = "");
    void Clear();
    void SetCursor(uint8_t col, uint8_t row);
    int PutChar(char c);  // For printf redirection
    void FlushPrintBuffer();  // Display accumulated printf output
};

// Global LCD driver instance for printf callback
extern LcdDriver* g_lcdDriver;

#endif // LCDDRIVER_H
