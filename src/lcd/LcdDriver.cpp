/**
 * @file LcdDriver.cpp
 * @brief ECAL Layer - LCD Driver Implementation
 */

#include "LcdDriver.h"

LcdDriver::LcdDriver(uint8_t rs, uint8_t en, uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
    : rs(rs), en(en), d4(d4), d5(d5), d6(d6), d7(d7)
{
    lcd = nullptr;
}

void LcdDriver::Init() {
    lcd = new LiquidCrystal(rs, en, d4, d5, d6, d7);
    lcd->begin(16, 2);
}

void LcdDriver::Print(const char* l1, const char* l2) {
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print(l1);
    if (l2 && l2[0] != '\0') {
        lcd->setCursor(0, 1);
        lcd->print(l2);
    }
}

void LcdDriver::Clear() {
    lcd->clear();
}

void LcdDriver::SetCursor(uint8_t col, uint8_t row) {
    lcd->setCursor(col, row);
}
