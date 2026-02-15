/**
 * @file Lab1_2_main.cpp
 * @brief Main application for advanced menu lock system (Lab 1.2)
 *
 * Integrates FSM, keypad, LCD, and LED drivers for interactive menu-driven lock.
 * Includes serial communication for debugging and optional serial input.
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
unsigned long feedbackTime = 0;
bool showingFeedback = false;
bool feedbackWasShowing = false;

void lab1_2_setup() {
    // Initialize serial communication at 9600 baud for printf/scanf support
    // This allows debugging messages and optional serial input
    SerialStdioInit(9600);
    
    // Display startup message via serial
    printf("=== Lab 1.2 - Smart Lock FSM ===\r\n");
    printf("Initializing hardware...\r\n");
    
    // Initialize all hardware drivers
    ledGreen.Init();
    ledRed.Init();
    lcd.Init();
    keypad.Init();
    LockFSM_Init();             // Initialize the lock's finite state machine
    
    // Display initial LCD message
    lcd.Print("Smart Lock", "Press * for menu");
    
    // Ensure LEDs start in off state
    ledGreen.Off();
    ledRed.Off();
    
    // Confirm initialization via serial
    printf("Lab 1.2 initialized successfully\r\n");
}

void triggerFeedback() {
    // Set flags to show visual feedback (via LEDs) for 1 second
    showingFeedback = true;
    feedbackWasShowing = true;
    feedbackTime = millis();
}

void updateLEDs() {
    // Check if feedback display period is still active (1 second timeout)
    bool feedbackActive = showingFeedback && (millis() - feedbackTime < 1000);
    
    if (feedbackActive) {
        // Display status via LED colors during feedback period
        if (LockFSM_WasLastOpError()) {
            // Red LED: operation error
            ledGreen.Off();
            ledRed.On();
        } else if (LockFSM_GetState() == STATE_UNLOCKED) {
            // Green LED: lock successfully unlocked
            ledGreen.On();
            ledRed.Off();
        } else {
            // Red LED: locked or other state
            ledGreen.Off();
            ledRed.On();
        }
    } else {
        // Turn off LEDs when not in feedback period
        ledGreen.Off();
        ledRed.Off();
        // Restore default LCD message after feedback timeout
        if (feedbackWasShowing && !feedbackActive) {
            lcd.Print("Press * for menu", "");
            feedbackWasShowing = false;
        }
        showingFeedback = false;
    }
}

void lab1_2_loop() {
    // Read a single key press from the keypad (non-blocking, returns 0 if no key)
    char key = keypad.GetKey();
    
    if (key) {
        // '*' enters menu mode
        if (key == '*') {
            inMenu = true;
            inputIndex = 0;
            memset(inputBuffer, 0, sizeof(inputBuffer));
            // Display menu options on LCD
            lcd.Print("*0:Lock *1:Unlock", "*2:ChgPass *3:Stat");
            printf("Menu activated\r\n");
        } 
        // '#' confirms selection in menu or input mode
        else if (key == '#' && inMenu && inputIndex > 0) {
            inputBuffer[inputIndex] = '\0';
            char op = inputBuffer[0];
            printf("Operation selected: %c\r\n", op);
            // Send operation to FSM state machine
            LockFSM_SelectOperation(op);
            lcd.Print(LockFSM_GetMessage(), "");
            triggerFeedback();              // Show LED feedback for 1 second
            inputIndex = 0;
            memset(inputBuffer, 0, sizeof(inputBuffer));
            inMenu = false;
        } 
        // '#' confirms password/PIN input
        else if (key == '#' && !inMenu && LockFSM_GetCurrentState() >= STATE_INPUT_UNLOCK) {
            inputBuffer[inputIndex] = '\0';
            printf("Input processed: %s\r\n", inputBuffer);
            // Send password attempt to FSM
            LockFSM_ProcessInput(inputBuffer);
            lcd.Print(LockFSM_GetMessage(), "");
            triggerFeedback();              // Show LED feedback for 1 second
            inputIndex = 0;
            memset(inputBuffer, 0, sizeof(inputBuffer));
        } 
        // Store key in input buffer for menu or password entry
        else if (inMenu || (LockFSM_GetCurrentState() >= STATE_INPUT_UNLOCK && LockFSM_GetCurrentState() <= STATE_INPUT_CHANGE_NEW)) {
            if (inputIndex < (int)sizeof(inputBuffer)-1) {
                // Add character to buffer and display on LCD
                inputBuffer[inputIndex++] = key;
                lcd.SetCursor(0, 1);
                lcd.Print("Cmd:", inputBuffer);
            }
        }
    }
    // Update LED status based on FSM state and feedback timer
    updateLEDs();
}
