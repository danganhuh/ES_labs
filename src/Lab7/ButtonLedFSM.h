/**
 * @file ButtonLedFSM.h
 * @brief Lab7 Part 1 - Button-LED FSM Control Interface
 * 
 * Two-state Moore FSM for controlling LED based on button presses.
 * States: LED_OFF (0) and LED_ON (1)
 * Transitions: Triggered by button presses with software debounce
 * Output: LED state (0=OFF, 1=ON) dependent only on current state
 * 
 * Architecture: SRV Layer
 *   Application calls FSM_Init() at startup
 *   Application calls FSM_Update() periodically (~100ms)
 *   FSM manages state transitions and debounce logic internally
 */

#ifndef BUTTONLED_FSM_H
#define BUTTONLED_FSM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum LEDState
 * @brief Two-state FSM states for Button-LED control
 */
typedef enum {
    LED_OFF_STATE = 0,    // LED is off (output = 0)
    LED_ON_STATE = 1      // LED is on (output = 1)
} LEDState;

/**
 * @brief Initialize FSM to LED_OFF state
 * 
 * Must be called once during system setup.
 * Sets initial state to LED_OFF.
 */
void ButtonLedFSM_Init(void);

/**
 * @brief Get current LED state
 * 
 * @return Current state (LED_OFF_STATE or LED_ON_STATE)
 */
LEDState ButtonLedFSM_GetState(void);

/**
 * @brief Get output value for current state (Moore automaton)
 * 
 * Output depends only on current state:
 * - LED_OFF_STATE returns 0
 * - LED_ON_STATE returns 1
 * 
 * @return Output value (0 or 1)
 */
uint8_t ButtonLedFSM_GetOutput(void);

/**
 * @brief Process button press event and update FSM state
 * 
 * Called periodically with a debounced button press event.
 * Implements state transition logic:
 * - LED_OFF + button_pressed → LED_ON
 * - LED_ON + button_pressed → LED_OFF
 * - Any state + no button → stay in state
 * 
 * @param buttonInput Debounced button press event (0=no press, 1=press)
 */
void ButtonLedFSM_ProcessInput(uint8_t buttonInput);

/**
 * @brief Force FSM into a given state (e.g. from a serial command).
 *
 * Updates the Moore output and state-entry time. Button presses continue to
 * toggle from this state on subsequent ProcessInput calls.
 *
 * @param state LED_OFF_STATE or LED_ON_STATE
 */
void ButtonLedFSM_SetState(LEDState state);

/**
 * @brief Get state name for display purposes
 * 
 * @return State name as string ("LED_OFF" or "LED_ON")
 */
const char* ButtonLedFSM_GetStateName(void);

/**
 * @brief Get time in current state (for debugging)
 * 
 * @return Time in current state (milliseconds)
 */
uint32_t ButtonLedFSM_GetStateTime(void);

#ifdef __cplusplus
}
#endif

#endif // BUTTONLED_FSM_H
