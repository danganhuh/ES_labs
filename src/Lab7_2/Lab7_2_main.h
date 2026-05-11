/**
 * @file Lab7_2_main.h
 * @brief Lab7 Part 2 - Intelligent Traffic Light FSM Application Interface
 * 
 * Multi-direction traffic light control system with dynamic priority switching.
 * Implements Moore FSMs with:
 * - Independent state machines for East-West and North-South
 * - Priority switching based on North request button
 * - FreeRTOS task synchronization
 * - Real-time serial display
 */

#ifndef LAB7_2_MAIN_H
#define LAB7_2_MAIN_H

/**
 * @brief Setup function for Lab7 Part 2
 * 
 * Initializes:
 * - Serial communication (115200 baud)
 * - Traffic light LED pins (6 LEDs for two directions)
 * - Button pin for North request
 * - FSMs to initial states
 * - FreeRTOS (if enabled) or bare-metal scheduling
 * 
 * Called once during system initialization.
 */
void lab7_2_setup(void);

/**
 * @brief Main loop function for Lab7 Part 2
 * 
 * If using FreeRTOS:
 * - Starts the scheduler (one-way call, never returns)
 * 
 * If using bare-metal:
 * - Would implement scheduling loop
 * 
 * Called once after setup(), transitions to FreeRTOS kernel.
 */
void lab7_2_loop(void);

#endif
