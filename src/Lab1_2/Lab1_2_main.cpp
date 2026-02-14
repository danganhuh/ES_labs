/**
 * @file Lab1_2_main.cpp
 * @brief Main application for advanced menu lock system (Lab 1.2)
 *
 * Integrates FSM, keypad, LCD, and LED drivers for interactive menu-driven lock.
 */

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "led/LedDriver.h"
#include "keypad/KeypadDriver.h"
#include "lcd/LcdDriver.h"
#include "fsm/LockFSM.h"

static const uint8_t LCD_RS = 12, LCD_EN = 11, LCD_D4 = 5, LCD_D5 = 4, LCD_D6 = 3, LCD_D7 = 2;
static const uint8_t LED_GREEN_PIN = 10, LED_RED_PIN = 13;
static const byte ROWS = 4, COLS = 4;
static const byte rowPins[ROWS] = {A0, A1, A2, A3};
static const byte colPins[COLS] = {6, 7, 8, 9};
static char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

LedDriver ledGreen(LED_GREEN_PIN);
LedDriver ledRed(LED_RED_PIN);
KeypadDriver keypad(keys, rowPins, colPins, ROWS, COLS);
LcdDriver lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

char inputBuffer[32];
int inputIndex = 0;
bool inMenu = false;
unsigned long feedbackTime = 0;
bool showingFeedback = false;
bool feedbackWasShowing = false;

void setup() {
    ledGreen.Init();
    ledRed.Init();
    lcd.Init();
    keypad.Init();
    LockFSM_Init();
    lcd.Print("Smart Lock", "Press * for menu");
    ledGreen.Off();
    ledRed.Off();
}

void triggerFeedback() {
    showingFeedback = true;
    feedbackWasShowing = true;
    feedbackTime = millis();
}

void updateLEDs() {
    bool feedbackActive = showingFeedback && (millis() - feedbackTime < 1000);
    
    if (feedbackActive) {
        if (LockFSM_WasLastOpError()) {
            ledGreen.Off();
            ledRed.On();
        } else if (LockFSM_GetState() == STATE_UNLOCKED) {
            ledGreen.On();
            ledRed.Off();
        } else {
            ledGreen.Off();
            ledRed.On();
        }
    } else {
        ledGreen.Off();
        ledRed.Off();
        if (feedbackWasShowing && !feedbackActive) {
            lcd.Print("Press * for menu", "");
            feedbackWasShowing = false;
        }
        showingFeedback = false;
    }
}

void loop() {
    char key = keypad.GetKey();
    
    if (key) {
        if (key == '*') {
            inMenu = true;
            inputIndex = 0;
            memset(inputBuffer, 0, sizeof(inputBuffer));
            lcd.Print("*0:Lock *1:Unlock", "*2:ChgPass *3:Stat");
        } else if (key == '#' && inMenu && inputIndex > 0) {
            inputBuffer[inputIndex] = '\0';
            char op = inputBuffer[0];
            LockFSM_SelectOperation(op);
            lcd.Print(LockFSM_GetMessage(), "");
            triggerFeedback();
            inputIndex = 0;
            memset(inputBuffer, 0, sizeof(inputBuffer));
            inMenu = false;
        } else if (key == '#' && !inMenu && LockFSM_GetCurrentState() >= STATE_INPUT_UNLOCK) {
            inputBuffer[inputIndex] = '\0';
            LockFSM_ProcessInput(inputBuffer);
            lcd.Print(LockFSM_GetMessage(), "");
            triggerFeedback();
            inputIndex = 0;
            memset(inputBuffer, 0, sizeof(inputBuffer));
        } else if (inMenu || (LockFSM_GetCurrentState() >= STATE_INPUT_UNLOCK && LockFSM_GetCurrentState() <= STATE_INPUT_CHANGE_NEW)) {
            if (inputIndex < (int)sizeof(inputBuffer)-1) {
                inputBuffer[inputIndex++] = key;
                lcd.SetCursor(0, 1);
                lcd.Print("Cmd:", inputBuffer);
            }
        }
    }
    updateLEDs();
}
