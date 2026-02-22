/**
 * @file Lab1.cpp
 * @brief SRV Layer - Lab 1 Implementation
 * 
 * Implements Lab 1: Serial STDIO LED Control experiment
 * 
 * Uses:
 * - LedDriver (ECAL): Abstracts LED control
 * - SerialStdioDriver (ECAL): Abstracts serial communication
 * 
 * The lab processes user commands from serial terminal to control an LED.
 */

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "../led/LedDriver.h"
#include "../drivers/SerialStdioDriver.h"
#include "Lab1.h"

// Hardware pin definitions
#define LedPin 13       // LED on digital pin 13
#define BufferSize 64   // Serial buffer size

// Module-level LED driver instance (static = file scope)
static LedDriver StatusLed(LedPin);

/**
 * @brief Process user command from serial input
 * 
 * Parses the command string and executes corresponding LED action.
 * Supports: "led on", "led off"
 * 
 * @param command String containing the user command
 */
static void ProcessCommand(char *command)
{
    if (strcmp(command, "led on") == 0)
    {
        StatusLed.On();  // Turn LED on via ECAL driver
        printf("LED turned ON\n");
    }
    else if (strcmp(command, "led off") == 0)
    {
        StatusLed.Off();  // Turn LED off via ECAL driver
        printf("LED turned OFF\n");
    }
    else
    {
        printf("Invalid command. Use: led on | led off\n");
    }
}

/**
 * @brief Initialize Lab 1 resources
 * 
 * Sets up:
 * - Serial communication (UART/STDIO) at 9600 baud
 * - LED driver and initial pin configuration
 * - Displays welcome message and command instructions
 */
void Lab1_Setup()
{
    SerialStdioInit(9600);  // Initialize serial at 9600 baud (ECAL), redirects printf/scanf
    StatusLed.Init();       // Initialize LED pin (ECAL)

    // Display welcome message
    printf("=== Lab 1.1 - Serial STDIO LED Control ===\n");
    printf("Available commands:\n");
    printf("led on\n");
    printf("led off\n");
    printf("\n");
}

/**
 * @brief Main loop for Lab 1
 * 
 * Continuously waits for serial input and processes commands.
 * Uses SerialReadLine to read a full command line from the serial monitor.
 */
void Lab1_Loop()
{
    char command[BufferSize];

    // Read a full line from the serial monitor (blocks until \n received)
    scanf(" %63[^\n]", command);

    // Parse and execute the complete command
    ProcessCommand(command);
}
