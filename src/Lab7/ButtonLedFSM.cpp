/**
 * @file ButtonLedFSM.cpp
 * @brief Lab7 Part 1 - Button-LED FSM Implementation
 * 
 * Two-state Moore FSM:
 *   States: LED_OFF (output=0), LED_ON (output=1)
 *   Transitions: Button press toggles state
 *   Output rule: Output = CurrentState (Moore model)
 * 
 * State Table:
 * ┌─────┬──────────┬────────┬────────────────────────────┐
 * │ Num │ Name     │ Output │ Next[input=0] Next[input=1]│
 * ├─────┼──────────┼────────┼────────────────────────────┤
 * │ 0   │ LED_OFF  │ 0      │ LED_OFF       │ LED_ON     │
 * │ 1   │ LED_ON   │ 1      │ LED_ON        │ LED_OFF    │
 * └─────┴──────────┴────────┴────────────────────────────┘
 */

#include "ButtonLedFSM.h"
#include <Arduino.h>
#include <stdio.h>

// ============================================================================
// FSM State Definition Structure
// ============================================================================
typedef struct {
    uint8_t output;              // Output value for this state (Moore)
    uint32_t time_entered_ms;    // Timestamp when state was entered
} FSMStateData;

// ============================================================================
// FSM State Table (Table-driven implementation)
// ============================================================================
typedef struct {
    uint8_t output;                      // Output in this state (Moore)
    LEDState next_state[2];              // Next states: [button=0], [button=1]
} ButtonLedFSMStateTransition;

static const ButtonLedFSMStateTransition FSM_TABLE[2] = {
    // State 0: LED_OFF
    {
        .output = 0,                     // Output: LED is OFF
        .next_state = {LED_OFF_STATE, LED_ON_STATE}  // Transitions on [no press, press]
    },
    // State 1: LED_ON
    {
        .output = 1,                     // Output: LED is ON
        .next_state = {LED_ON_STATE, LED_OFF_STATE}  // Transitions on [no press, press]
    }
};

// ============================================================================
// FSM Runtime State
// ============================================================================
static LEDState g_current_state = LED_OFF_STATE;
static FSMStateData g_state_data = {0, 0};

// ============================================================================
// FSM Interface Implementation
// ============================================================================

void ButtonLedFSM_Init(void) {
    g_current_state = LED_OFF_STATE;
    g_state_data.output = FSM_TABLE[LED_OFF_STATE].output;
    g_state_data.time_entered_ms = millis();
}

LEDState ButtonLedFSM_GetState(void) {
    return g_current_state;
}

uint8_t ButtonLedFSM_GetOutput(void) {
    return FSM_TABLE[g_current_state].output;
}

void ButtonLedFSM_ProcessInput(uint8_t buttonInput) {
    // Clamp input to 0 or 1
    buttonInput = (buttonInput != 0) ? 1 : 0;
    
    // State transition using table
    LEDState next_state = FSM_TABLE[g_current_state].next_state[buttonInput];
    
    // Only update state if it actually changes (for timestamp tracking)
    if (next_state != g_current_state) {
        g_current_state = next_state;
        g_state_data.output = FSM_TABLE[g_current_state].output;
        g_state_data.time_entered_ms = millis();
    }
}

void ButtonLedFSM_SetState(LEDState state) {
    if (state != LED_OFF_STATE && state != LED_ON_STATE) {
        return;
    }
    if (state != g_current_state) {
        g_current_state = state;
        g_state_data.output = FSM_TABLE[g_current_state].output;
        g_state_data.time_entered_ms = millis();
    }
}

const char* ButtonLedFSM_GetStateName(void) {
    return (g_current_state == LED_OFF_STATE) ? "LED_OFF" : "LED_ON";
}

uint32_t ButtonLedFSM_GetStateTime(void) {
    return (millis() - g_state_data.time_entered_ms);
}
