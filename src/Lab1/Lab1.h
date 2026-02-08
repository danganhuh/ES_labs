/**
 * @file Lab1.h
 * @brief SRV Layer - Lab 1: Serial STDIO LED Control
 * 
 * Interface for Lab 1 experiment: Serial-based LED control
 * 
 * The user can send commands via serial terminal to control an LED:
 * - "led on"  : Turn LED ON
 * - "led off" : Turn LED OFF
 * 
 * This lab demonstrates:
 * - Serial communication (UART/STDIO)
 * - Digital output control (LED on pin 13)
 * - Simple command parsing
 */

#ifndef LAB1_H
#define LAB1_H

/**
 * @brief Initialize Lab 1
 * 
 * Sets up serial communication at 9600 baud and initializes the LED driver.
 * Displays welcome message and available commands.
 */
void Lab1_Setup();

/**
 * @brief Main loop for Lab 1
 * 
 * Continuously listens for serial commands and processes them.
 * Blocks waiting for user input (serial read is blocking).
 */
void Lab1_Loop();

#endif
