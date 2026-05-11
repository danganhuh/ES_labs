/**
 * @file Lab7_main.cpp
 * @brief Lab7 Part 1 - Button-LED FSM Application Implementation
 *
 * Implements Moore FSM for LED control:
 * - Input: Button (with software debounce)
 * - Output: LED state (0=OFF, 1=ON)
 * - States: 2 (LED_OFF, LED_ON)
 * - Transitions: Triggered by button press
 *
 * 4-Step Moore FSM Cycle:
 * 1. Output based on current state
 * 2. Wait for state evaluation period
 * 3. Read debounced input
 * 4. Update state based on input
 *
 * Debounce: Software-based, 50ms window
 * Display: Serial output with state and timestamp
 * Latency: < 100ms guaranteed by design
 */

#include "Lab7_main.h"
#include "ButtonLedFSM.h"
#include "Lab7_Shared.h"
#include "drivers/SerialStdioDriver.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ============================================================================
// Button Debounce State Machine
// ============================================================================

typedef struct {
    uint8_t last_reading;
    uint32_t last_change_time_ms;
    uint8_t stable_reading;
} ButtonDebounceState;

static ButtonDebounceState g_button_state = {
    .last_reading = 1,
    .last_change_time_ms = 0,
    .stable_reading = 1
};

static uint8_t g_last_stable_reading = 1;

static uint8_t read_debounced_button(void)
{
    uint8_t current = digitalRead(LAB7_BUTTON_PIN);
    uint32_t now_ms = millis();

    if (current != g_button_state.last_reading)
    {
        g_button_state.last_reading = current;
        g_button_state.last_change_time_ms = now_ms;
    }

    if ((now_ms - g_button_state.last_change_time_ms) >= LAB7_DEBOUNCE_MS)
    {
        g_button_state.stable_reading = current;
    }

    return g_button_state.stable_reading;
}

static uint8_t read_button_press_event(void)
{
    uint8_t stable = read_debounced_button();
    uint8_t pressed_event = (g_last_stable_reading == 1u && stable == 0u) ? 1u : 0u;
    g_last_stable_reading = stable;
    return pressed_event;
}

// ============================================================================
// Serial commands (LED on / off, sync with FSM)
// ============================================================================

static char     s_cmdBuf[40];
static uint8_t  s_cmdIdx = 0;

static void trim_inplace(char* s)
{
    if (s == NULL || *s == '\0') {
        return;
    }
    char* end;
    while (*s == ' ' || *s == '\t') {
        memmove(s, s + 1, strlen(s));
    }
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t')) {
        *--end = '\0';
    }
}

static bool streq_ci(const char* a, const char* b)
{
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        ++a;
        ++b;
    }
    return (*a == '\0') && (*b == '\0');
}

static void print_help_serial(void)
{
    printf("Commands: led on | led off | on | off | help\r\n");
}

static void process_serial_line(char* line)
{
    trim_inplace(line);
    if (*line == '\0') {
        return;
    }

    if (streq_ci(line, "help") || streq_ci(line, "?")) {
        print_help_serial();
        return;
    }

    if (streq_ci(line, "led on") || streq_ci(line, "on")) {
        ButtonLedFSM_SetState(LED_ON_STATE);
        printf("OK: LED ON (FSM = LED_ON)\r\n");
        return;
    }

    if (streq_ci(line, "led off") || streq_ci(line, "off")) {
        ButtonLedFSM_SetState(LED_OFF_STATE);
        printf("OK: LED OFF (FSM = LED_OFF)\r\n");
        return;
    }

    printf("Unknown: '%s' — type help\r\n", line);
}

static void poll_serial_commands(void)
{
    while (Serial.available() > 0) {
        const int c = Serial.read();
        if (c < 0) {
            break;
        }
        if (c == '\r' || c == '\n') {
            if (s_cmdIdx > 0) {
                s_cmdBuf[s_cmdIdx] = '\0';
                printf("> %s\r\n", s_cmdBuf);
                process_serial_line(s_cmdBuf);
                s_cmdIdx = 0;
            }
        } else if ((unsigned char)c < 0x20) {
            /* drop other control chars */
        } else if (s_cmdIdx < (sizeof(s_cmdBuf) - 1)) {
            s_cmdBuf[s_cmdIdx++] = (char)c;
        } else {
            s_cmdIdx = 0; /* line too long */
            printf("[lab7] line too long — dropped\r\n");
        }
    }
}

// ============================================================================
// Display and Timing State
// ============================================================================

static uint32_t g_last_display_time_ms = 0;
static uint32_t g_last_lcd_time_ms = 0;
static LiquidCrystal_I2C g_lcd(LAB7_LCD_I2C_ADDR, 16, 2);

static void update_display(void)
{
    uint32_t now_ms = millis();

    if ((now_ms - g_last_display_time_ms) >= LAB7_DISPLAY_UPDATE_MS)
    {
        g_last_display_time_ms = now_ms;

        printf("[%05lu] State: %s | Output: %u | Time in State: %lu ms\r\n",
               (unsigned long)now_ms,
               ButtonLedFSM_GetStateName(),
               (unsigned)ButtonLedFSM_GetOutput(),
               (unsigned long)ButtonLedFSM_GetStateTime());
    }
}

static void update_lcd_display(void)
{
    uint32_t now_ms = millis();
    if ((now_ms - g_last_lcd_time_ms) < LAB7_LCD_UPDATE_MS)
    {
        return;
    }

    g_last_lcd_time_ms = now_ms;

    char line1[17] = {0};
    char line2[17] = {0};
    const uint8_t output = ButtonLedFSM_GetOutput();
    const uint32_t state_time = ButtonLedFSM_GetStateTime();

    snprintf(line1, sizeof(line1), "LED:%s", output ? "ON" : "OFF");
    snprintf(line2, sizeof(line2), "T:%5lums", (unsigned long)state_time);
    g_lcd.clear();
    g_lcd.setCursor(0, 0);
    g_lcd.print(line1);
    g_lcd.setCursor(0, 1);
    g_lcd.print(line2);
}

// ============================================================================
// Lab7 Setup and Loop
// ============================================================================

void lab7_setup(void)
{
    SerialStdioInit(LAB7_SERIAL_BAUD);
    delay(100);

    printf("\r\n========================================\r\n");
    printf("Lab7 Part 1: Button-LED FSM Control\r\n");
    printf("========================================\r\n");
    printf("Button Pin: %d (INPUT_PULLUP)\r\n", LAB7_BUTTON_PIN);
    printf("LED Pin: %d (OUTPUT)\r\n", LAB7_LED_PIN);
    printf("Debounce Window: %d ms\r\n", LAB7_DEBOUNCE_MS);
    printf("FSM Evaluation: %d ms\r\n", LAB7_FSM_STATE_DELAY_MS);
    printf("Serial: led on | led off | on | off | help\r\n");
    printf("========================================\r\n\r\n");

    pinMode(LAB7_BUTTON_PIN, INPUT_PULLUP);
    g_button_state.last_reading = digitalRead(LAB7_BUTTON_PIN);
    g_button_state.stable_reading = g_button_state.last_reading;
    g_last_stable_reading = g_button_state.stable_reading;

    pinMode(LAB7_LED_PIN, OUTPUT);
    digitalWrite(LAB7_LED_PIN, LOW);

    Wire.begin();
    g_lcd.init();
    g_lcd.backlight();

    ButtonLedFSM_Init();

    printf("System initialized. Press button to toggle LED.\r\n\r\n");
}

void lab7_loop(void)
{
    poll_serial_commands();

    uint8_t led_output = ButtonLedFSM_GetOutput();
    digitalWrite(LAB7_LED_PIN, led_output ? HIGH : LOW);

    delay(LAB7_FSM_STATE_DELAY_MS);

    uint8_t button_input = read_button_press_event();

    ButtonLedFSM_ProcessInput(button_input);

    update_display();
    update_lcd_display();
}
