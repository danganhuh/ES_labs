/**
 * @file main.cpp
 * @brief APP Layer - Laboratory Experiment Dispatcher
 * 
 * This is the entry point for the ES_Labs project. It acts as a dispatcher
 * that selects which laboratory exercise (Lab1, Lab2, etc.) to execute.
 * 
 * To switch between labs, simply change the SELECTED_LAB macro value.
 * The preprocessor will then include and call the corresponding lab's
 * setup and loop functions.
 * 
 * Architecture: APP -> SRV(Lab) -> ECAL(Drivers) -> MCAL(Arduino) -> HW
 */

#include <Arduino.h>

// Select which lab to run (1, 2, etc.)
#define SELECTED_LAB 1

#if SELECTED_LAB == 1
#include "Lab1/Lab1.h"
#endif

void setup()
{
#if SELECTED_LAB == 1
    Lab1_Setup();  // Initialize Lab1 (serial, LED, etc.)
#endif
}

void loop()
{
#if SELECTED_LAB == 1
    Lab1_Loop();   // Run Lab1 main logic
#endif
}
