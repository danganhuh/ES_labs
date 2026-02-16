/**
 * @file Lab1_2_main.cpp
 * @brief Main application for advanced menu lock system (Lab 1.2)
 *
 * Integrates FSM, keypad, LCD, and LED drivers for interactive menu-driven lock.
 * Uses printf for LCD output and scanf for keypad input.
 */

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "led/LedDriver.h"
#include "keypad/KeypadDriver.h"
#include "lcd/LcdDriver.h"
#include "fsm/LockFSM.h"
#include "drivers/SerialStdioDriver.h"

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
bool passwordPromptShown = false;
unsigned long feedbackTime = 0;
bool showingFeedback = false;
bool feedbackWasShowing = false;

void lab1_2_setup() {
    // Initialize all hardware drivers FIRST (before StdioInit)
    ledGreen.Init();
    ledRed.Init();
    lcd.Init();
    keypad.Init();
    LockFSM_Init();

    // Redirect printf to LCD, scanf from keypad ('#' = Enter)
    StdioInit();

    // Display startup via printf (output goes to LCD)
    printf("Smart Lock Init\nPress * for menu\n");

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
            printf("Press * for menu\n");
            feedbackWasShowing = false;
        }
        showingFeedback = false;
    }
}

void lab1_2_loop() {
    // Menu mode: show menu and read command via scanf (blocks until user presses #)
    if (inMenu) {
        // Single printf, no middle \n, so both lines display (16 chars each)
        printf("*0:Lock *1:Open*2:ChgPass *3:St\n");
        if (scanf("%31s", inputBuffer) >= 1 && inputBuffer[0] != '\0') {
            LockFSM_SelectOperation(inputBuffer[0]);
            printf("Cmd: %s\n%s\n", inputBuffer, LockFSM_GetMessage());
            if (inputBuffer[0] == '1' || inputBuffer[0] == '2') {
                ;  // FSM needs password; will scanf in next branch
            } else {
                triggerFeedback();
            }
        }
        inMenu = false;
    }
    // FSM needs password input: prompt, then echo each key as pressed until #
    else if (LockFSM_GetCurrentState() >= STATE_INPUT_UNLOCK &&
             LockFSM_GetCurrentState() <= STATE_INPUT_CHANGE_NEW) {
        if (!passwordPromptShown) {
            printf("%s\n", LockFSM_GetMessage());
            passwordPromptShown = true;
            inputIndex = 0;
            memset(inputBuffer, 0, sizeof(inputBuffer));
        }
        char key = keypad.GetKey();
        if (key == '#') {
            if (inputIndex > 0) {
                inputBuffer[inputIndex] = '\0';
                LockFSM_ProcessInput(inputBuffer);
                printf("Code: %s\n%s\n", inputBuffer, LockFSM_GetMessage());
                triggerFeedback();
            }
            passwordPromptShown = false;
        } else if (key && key >= '0' && key <= '9' && inputIndex < (int)sizeof(inputBuffer) - 1) {
            inputBuffer[inputIndex++] = key;
            inputBuffer[inputIndex] = '\0';
            printf("%s\n%s\n", LockFSM_GetMessage(), inputBuffer);
        }
    }
    // Idle: detect '*' to enter menu (non-blocking)
    else {
        char key = keypad.GetKey();
        if (key == '*') {
            inMenu = true;
        }
    }

    updateLEDs();
}
