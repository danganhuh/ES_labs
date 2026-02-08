/**
 * @file LedDriver.cpp
 * @brief ECAL Layer - LED Driver Implementation
 * 
 * Implements the LED hardware abstraction driver.
 * Uses Arduino MCAL (digitalWrite, pinMode, digitalRead) to control GPIO.
 * 
 * Architecture: Lab -> LedDriver -> GPIO MCAL -> Arduino GPIO Hardware
 */

#include "LedDriver.h"

/**
 * @brief Constructor - Store the GPIO pin number
 */
LedDriver::LedDriver(uint8_t pin)
{
    ledPin = pin;
}

/**
 * @brief Initialize LED pin as output and set to OFF
 * 
 * Uses MCAL: pinMode() and digitalWrite() from Arduino core
 */
void LedDriver::Init()
{
    pinMode(ledPin, OUTPUT);      // MCAL: Configure as output
    digitalWrite(ledPin, LOW);     // MCAL: Set low (LED off)
}

/**
 * @brief Turn LED ON (set pin HIGH)
 * 
 * Uses MCAL: digitalWrite()
 */
void LedDriver::On()
{
    digitalWrite(ledPin, HIGH);    // MCAL: Set high (LED on)
}

/**
 * @brief Turn LED OFF (set pin LOW)
 * 
 * Uses MCAL: digitalWrite()
 */
void LedDriver::Off()
{
    digitalWrite(ledPin, LOW);     // MCAL: Set low (LED off)
}

/**
 * @brief Toggle LED state
 * 
 * Reads current pin state and inverts it.
 * Uses MCAL: digitalWrite() and digitalRead()
 */
void LedDriver::Toggle()
{
    digitalWrite(ledPin, !digitalRead(ledPin));  // MCAL: Toggle pin
}
