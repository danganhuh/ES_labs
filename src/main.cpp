/**
 * @file main.cpp
 * @brief APP Layer - Laboratory Experiment Dispatcher
 * 
 * This is the entry point for the ES_Labs project. It acts as a dispatcher
 * that selects which laboratory exercise (Lab1, Lab1_2, Lab2, etc.) to execute.
 * 
 * To switch between labs, simply change the SELECTED_LAB macro value:
 * - 1    : Lab 1.1 (Serial LED Control)
 * - 12   : Lab 1.2 (Lock FSM with Keypad/LCD)
 * - 2, 3, etc. : Additional labs when available
 * 
 * Architecture: APP -> SRV(Lab) -> ECAL(Drivers) -> MCAL(Arduino) -> HW
 */

#include <Arduino.h>
#include <avr/wdt.h>

// ============================================================================
// CONFIGURATION: Change this to switch between labs
// ============================================================================
// Simply change the value below to select which lab to run:
//   1  = Lab 1.1 (Serial LED Control with printf/scanf)
//   12 = Lab 1.2 (Smart Lock FSM with Keypad/LCD + serial debug)
//   2  = Lab 2   (Button Duration Monitor – sequential bare-metal scheduler)
#define SELECTED_LAB 2          // Options: 1 (Lab 1.1), 12 (Lab 1.2), 2 (Lab 2)
// ============================================================================

#if SELECTED_LAB == 1
    #include "Lab1/Lab1.h"
#endif

#if SELECTED_LAB == 12
    // For Lab 1.2, we'll declare the functions that are in Lab1_2_main.cpp
    void lab1_2_setup();
    void lab1_2_loop();
#endif

#if SELECTED_LAB == 2
    // For Lab 2, declare the functions that are in Lab2/Lab2_main.cpp
    void lab2_setup();
    void lab2_loop();
#endif

/**
 * @brief Arduino setup() - Called once at startup
 * 
 * Initializes the selected lab based on the SELECTED_LAB macro.
 */
void setup()
{
    wdt_disable();  // Prevent bootloader WDT reset loop flooding serial

#if SELECTED_LAB == 1
    Lab1_Setup();
#endif

#if SELECTED_LAB == 12
    lab1_2_setup();
#endif

#if SELECTED_LAB == 2
    lab2_setup();
#endif
}

/**
 * @brief Arduino loop() - Called repeatedly after setup()
 * 
 * Runs the main loop for the selected lab.
 */
void loop()
{
#if SELECTED_LAB == 1
    Lab1_Loop();
#endif

#if SELECTED_LAB == 12
    lab1_2_loop();
#endif

#if SELECTED_LAB == 2
    lab2_loop();
#endif
}
