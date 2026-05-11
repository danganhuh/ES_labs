/**
 * @file main.cpp
 * @brief APP Layer - Laboratory Experiment Dispatcher
 *
 * Selects the active lab via SELECTED_LAB macro.
 * Also provides extern "C" hardware wrappers so the Lab2_2 .c files
 * can call Arduino (C++) APIs like Serial, pinMode, etc.
 *
 * Architecture: APP -> SRV(Lab) -> ECAL(Drivers) -> MCAL(Arduino) -> HW
 */

#include <Arduino.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdio.h>
#include "drivers/SerialStdioDriver.h"

// ============================================================================
// CONFIGURATION – change this value to select a lab
// ============================================================================
//   1  = Lab 1.1   12 = Lab 1.2   2 = Lab 2   22 = Lab 2.2 (FreeRTOS, C)
//   3  = Lab 3     32 = Lab 3.2   41 = Lab 4.1   42 = Lab 4.2
//   52 = Lab 5.2 (DS18B20 + L9110H fan, ON-OFF + PID combined)
//   7  = Lab 7 (Button-LED FSM)   72 = Lab 7.2 (Traffic Light FSM)
//
// Note: standalone Lab 5 has been scrapped; src/Lab5/ now only holds a
// frozen snapshot of the old relay-heater build under
// `backup_lab5_2_relay_heater/` (not compiled).
#define SELECTED_LAB 7
// ============================================================================


// ============================================================================
// C-callable hardware wrappers (used by Lab2_2/*.c via avr_helpers.h)
// ============================================================================
extern "C" {

void hw_pin_mode(uint8_t pin, uint8_t mode)
{
    pinMode(pin, mode);
}

void hw_digital_write(uint8_t pin, uint8_t val)
{
    digitalWrite(pin, val);
}

uint8_t hw_digital_read(uint8_t pin)
{
    return (uint8_t)digitalRead(pin);
}

unsigned long hw_millis(void)
{
    return millis();
}

void hw_delay_ms(unsigned long ms)
{
    /* Busy-wait – safe before AND after scheduler starts.
       Uses avr-libc _delay_ms(1) which is a calibrated busy loop. */
    while (ms > 0) { _delay_ms(1); ms--; }
}

void dbg_print(const char *s)
{
    Serial.print(s);
}

void dbg_print_num(long n)
{
    Serial.print(n);
}

void dbg_println(const char *s)
{
    Serial.println(s);
}

void dbg_println_num(long n)
{
    Serial.println(n);
}

void dbg_println_empty(void)
{
    Serial.println();
}

void dbg_flush(void)
{
    Serial.flush();
}

} /* extern "C" */

// ============================================================================
// Lab includes
// ============================================================================

#if SELECTED_LAB == 1
    #include "Lab1/Lab1.h"
#endif

#if SELECTED_LAB == 12
    void lab1_2_setup();
    void lab1_2_loop();
#endif

#if SELECTED_LAB == 2
    void lab2_setup();
    void lab2_loop();
#endif

#if SELECTED_LAB == 22
    #include "Lab2_2/Lab2_2_main.h"
#endif

#if SELECTED_LAB == 3
    #include "Lab3/Lab3_main.h"
#endif

#if SELECTED_LAB == 32
    #include "Lab3_2/Lab3_2_main.h"
#endif

#if SELECTED_LAB == 41
    #include "Lab4/Lab4_main.h"
#endif

#if SELECTED_LAB == 42
    #include "Lab4_2/Lab4_2_main.h"
#endif

#if SELECTED_LAB == 52
    #include "Lab5_2/Lab5_2_main.h"
#endif

#if SELECTED_LAB == 7
    #include "Lab7/Lab7_main.h"
#endif

#if SELECTED_LAB == 72
    #include "Lab7_2/Lab7_2_main.h"
#endif

// ============================================================================
// Arduino entry points
// ============================================================================


void setup()
{
    wdt_disable();

#if SELECTED_LAB == 22
    SerialStdioInit(9600);

    printf("[main] === Lab 2.2 starting ===\n");
#endif

#if SELECTED_LAB == 3
    SerialStdioInit(9600);
    printf("[main] === Lab 3 starting ===\n");
#endif

#if SELECTED_LAB == 32
    SerialStdioInit(9600);
    printf("[main] === Lab 3.2 starting ===\n");
#endif

#if SELECTED_LAB == 41
    SerialStdioInit(9600);
    printf("[main] === Lab 4.1 starting ===\n");
#endif

#if SELECTED_LAB == 42
    SerialStdioInit(9600);
    printf("[main] === Lab 4.2 starting ===\n");
#endif

#if SELECTED_LAB == 52
    SerialStdioInit(115200);
    printf("[main] === Lab 5.2 starting ===\n");
#endif

#if SELECTED_LAB == 7
    SerialStdioInit(115200);
    printf("[main] === Lab 7 Part 1 starting ===\n");
#endif

#if SELECTED_LAB == 72
    SerialStdioInit(115200);
    printf("[main] === Lab 7 Part 2 starting ===\n");
#endif

#if SELECTED_LAB == 1

    Lab1_Setup();
#elif SELECTED_LAB == 12
    lab1_2_setup();
#elif SELECTED_LAB == 2
    lab2_setup();
#elif SELECTED_LAB == 22
    lab2_2_setup();
#elif SELECTED_LAB == 3
    lab3_setup();
#elif SELECTED_LAB == 32
    lab3_2_setup();
#elif SELECTED_LAB == 41
    lab4_setup();
#elif SELECTED_LAB == 42
    lab4_2_setup();
#elif SELECTED_LAB == 52
    lab5_2_setup();
#elif SELECTED_LAB == 7
    lab7_setup();
#elif SELECTED_LAB == 72
    lab7_2_setup();
#endif
}

void loop()
{
#if SELECTED_LAB == 1
    Lab1_Loop();
#elif SELECTED_LAB == 12
    lab1_2_loop();
#elif SELECTED_LAB == 2
    lab2_loop();
#elif SELECTED_LAB == 22
    lab2_2_loop();
#elif SELECTED_LAB == 3
    lab3_loop();
#elif SELECTED_LAB == 32
    lab3_2_loop();
#elif SELECTED_LAB == 41
    lab4_loop();
#elif SELECTED_LAB == 42
    lab4_2_loop();
#elif SELECTED_LAB == 52
    lab5_2_loop();
#elif SELECTED_LAB == 7
    lab7_loop();
#elif SELECTED_LAB == 72
    lab7_2_loop();
#endif
}
