#ifndef LAB7_2_SHARED_H
#define LAB7_2_SHARED_H

#include <Arduino.h>

/**
 * @file Lab7_2_Shared.h
 * @brief Lab7 Part 2 - Intelligent Traffic Light FSM Control
 * 
 * Hardware Configuration for multi-FSM traffic light system
 * Two independent traffic directions: East-West and North-South
 * Each direction: GREEN → YELLOW → RED cycle
 * Priority: Dynamic switching based on North request button
 */

// ============================================================================
// Pin Configuration
// ============================================================================

// Traffic Light LEDs (East-West direction)
#define LAB7_2_LED_EW_GREEN       11     // EW Green LED
#define LAB7_2_LED_EW_YELLOW      10     // EW Yellow LED
#define LAB7_2_LED_EW_RED         9      // EW Red LED

// Traffic Light LEDs (North-South direction)
#define LAB7_2_LED_NS_GREEN       8      // NS Green LED
#define LAB7_2_LED_NS_YELLOW      7      // NS Yellow LED
#define LAB7_2_LED_NS_RED         6      // NS Red LED

// Input Button (North request)
#define LAB7_2_BUTTON_NS_REQUEST  2      // Digital input for North request

// ============================================================================
// FSM Configuration
// ============================================================================

// State durations (in milliseconds)
#define LAB7_2_STATE_GREEN_MS     3000   // Green light duration
#define LAB7_2_STATE_YELLOW_MS    1000   // Yellow light duration
#define LAB7_2_STATE_RED_MS       4000   // Red light duration (includes wait)

// Debounce and timing
#define LAB7_2_DEBOUNCE_MS        50     // Button debounce window
#define LAB7_2_FSM_UPDATE_MS      100    // FSM evaluation period
#define LAB7_2_DISPLAY_UPDATE_MS  500    // Status display refresh rate

// ============================================================================
// Serial Configuration
// ============================================================================

#define LAB7_2_SERIAL_BAUD        115200 // Serial communication speed

// ============================================================================
// LCD Configuration (I2C 16x2)
// ============================================================================

#define LAB7_2_LCD_I2C_ADDR        0x27
#define LAB7_2_LCD_UPDATE_MS       500
#define LAB7_2_ENABLE_LCD           1

// ============================================================================
// FreeRTOS Task Configuration (if used)
// ============================================================================
// NOTE: For the feilipu Arduino_FreeRTOS AVR port, StackType_t is uint8_t,
// so xTaskCreate's stack depth is in BYTES (not words). vprintf alone needs
// ~150-200 bytes of stack, so any task that calls printf/vprintf needs a
// stack of at least ~384 bytes; 256 bytes will overflow and crash the scheduler.

#define LAB7_2_EW_FSM_STACK       192
#define LAB7_2_NS_FSM_STACK       192
#define LAB7_2_BUTTON_STACK       192
#define LAB7_2_OUTPUT_STACK       192
#define LAB7_2_PRIORITY_STACK     256
#define LAB7_2_DISPLAY_STACK      512

#define LAB7_2_EW_FSM_PRIORITY    2
#define LAB7_2_NS_FSM_PRIORITY    2
#define LAB7_2_BUTTON_PRIORITY    1
#define LAB7_2_DISPLAY_PRIORITY   1

#endif
