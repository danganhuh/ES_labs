/**
 * @file LcdDriver.cpp
 * @brief ECAL Layer - LCD Driver Implementation
 */

#include "LcdDriver.h"
#include <string.h>

// Global LCD driver instance for printf callback
LcdDriver* g_lcdDriver = nullptr;

LcdDriver::LcdDriver(uint8_t rs, uint8_t en, uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
    : rs(rs), en(en), d4(d4), d5(d5), d6(d6), d7(d7), bufferIndex(0), lastUpdateTime(0)
{
    lcd = nullptr;
    memset(printBuffer, 0, sizeof(printBuffer));
}

void LcdDriver::Init() {
    lcd = new LiquidCrystal(rs, en, d4, d5, d6, d7);
    lcd->begin(16, 2);
    g_lcdDriver = this;  // Store global reference for printf callback
}

void LcdDriver::Print(const char* l1, const char* l2) {
    lcd->clear();
    delay(2);  // HD44780 clear takes ~1.52ms; wait before writing or first chars are lost
    lcd->setCursor(0, 0);
    lcd->print(l1);
    if (l2 && l2[0] != '\0') {
        lcd->setCursor(0, 1);
        lcd->print(l2);
    }
    bufferIndex = 0;
    memset(printBuffer, 0, sizeof(printBuffer));
}

void LcdDriver::Clear() {
    lcd->clear();
    bufferIndex = 0;
    memset(printBuffer, 0, sizeof(printBuffer));
}

void LcdDriver::SetCursor(uint8_t col, uint8_t row) {
    lcd->setCursor(col, row);
}

int LcdDriver::PutChar(char c) {
    // Handle newline: add to buffer then flush so we can split on \n for correct line breaks
    if (c == '\r' || c == '\n') {
        if (bufferIndex < sizeof(printBuffer) - 1) {
            printBuffer[bufferIndex++] = '\n';
            printBuffer[bufferIndex] = '\0';
        }
        FlushPrintBuffer();
        return 0;
    }
    
    // Add character to buffer
    if (bufferIndex < sizeof(printBuffer) - 1) {
        printBuffer[bufferIndex++] = c;
        printBuffer[bufferIndex] = '\0';
        
        // Flush when buffer fills (would truncate otherwise)
        if (bufferIndex >= sizeof(printBuffer) - 1) {
            FlushPrintBuffer();
        }
    }
    
    return 0;
}

void LcdDriver::FlushPrintBuffer() {
    if (bufferIndex == 0) return;
    
    char line1[17] = {0};
    char line2[17] = {0};
    
    // Try to split on newline first (so "line1\nline2" displays correctly)
    char* nl = (char*)memchr(printBuffer, '\n', bufferIndex);
    if (nl) {
        size_t len1 = (size_t)(nl - printBuffer);
        if (len1 > 16) len1 = 16;
        strncpy(line1, printBuffer, len1);
        line1[len1] = '\0';
        size_t off2 = len1 + 1;  // skip the \n
        if (off2 < (size_t)bufferIndex) {
            size_t len2 = (size_t)bufferIndex - off2;
            if (len2 > 16) len2 = 16;
            strncpy(line2, printBuffer + off2, len2);
            line2[len2] = '\0';
        }
    } else {
        // No newline: split at 16 chars
        if (bufferIndex <= 16) {
            strncpy(line1, printBuffer, 16);
        } else {
            strncpy(line1, printBuffer, 16);
            strncpy(line2, printBuffer + 16, 16);
        }
    }
    
    Print(line1, line2);
    lastUpdateTime = millis();
    bufferIndex = 0;
    memset(printBuffer, 0, sizeof(printBuffer));
}
