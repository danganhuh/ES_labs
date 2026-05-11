/**
 * @file Lab7_main.h
 * @brief Lab7 Part 1 - Button-LED FSM Application Interface
 *
 * Two-state finite state machine application.
 * Implements Moore automaton for LED control based on button input.
 * Includes software debounce and real-time serial output.
 */

#ifndef LAB7_MAIN_H
#define LAB7_MAIN_H

/**
 * @brief Setup function for Lab7 Part 1
 *
 * Initializes:
 * - Serial communication (115200 baud)
 * - Button pin as input with pull-up
 * - LED pin as output
 * - FSM to initial state (LED_OFF)
 * - LCD output (16x2)
 *
 * Called once during system initialization.
 */
void lab7_setup(void);

/**
 * @brief Main loop function for Lab7 Part 1
 *
 * Implements 4-step Moore FSM cycle:
 * 1. Apply output based on current state (write LED)
 * 2. Wait for state evaluation period
 * 3. Read debounced button press event
 * 4. Evaluate state transition and update FSM
 *
 * Also handles periodic serial status display.
 *
 * Called repeatedly from Arduino `loop()` via `main.cpp`.
 */
void lab7_loop(void);

#endif
