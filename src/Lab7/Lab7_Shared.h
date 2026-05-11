#ifndef LAB7_SHARED_H
#define LAB7_SHARED_H

#include <Arduino.h>

/**
 * @file Lab7_Shared.h
 * @brief Lab7 Part 1 - Button-LED FSM Control
 *
 * Hardware Configuration for Button-LED FSM implementation
 * States: LED_OFF (0), LED_ON (1)
 * Input source: Button with debounce
 * Output display: Serial (printf) and I2C LCD
 */

// Pin Configuration
#define LAB7_BUTTON_PIN             2      // Digital input for push button
#define LAB7_LED_PIN                13     // Digital output for LED

// LCD Configuration (I2C 16x2)
#define LAB7_LCD_I2C_ADDR           0x27

// FSM Configuration
#define LAB7_FSM_STATE_DELAY_MS     100    // State machine evaluation period
#define LAB7_DEBOUNCE_MS            50     // Button debounce window

// Serial Configuration
#define LAB7_SERIAL_BAUD            115200 // Serial communication speed
#define LAB7_DISPLAY_UPDATE_MS      250    // Status display refresh rate

// LCD Configuration
#define LAB7_LCD_UPDATE_MS          500    // LCD refresh rate

#endif
