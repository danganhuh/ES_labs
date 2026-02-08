/**
 * @file LedDriver.h
 * @brief ECAL Layer - LED Hardware Abstraction Driver
 * 
 * Provides a high-level interface for controlling an LED connected to
 * a GPIO pin on the Arduino microcontroller.
 * 
 * This driver abstracts away direct Arduino digitalWrite/pinMode calls,
 * allowing labs to control LEDs without knowing pin details.
 * 
 * Usage:
 *   LedDriver led(13);  // LED on pin 13
 *   led.Init();         // Configure as output
 *   led.On();           // Turn on
 *   led.Off();          // Turn off
 *   led.Toggle();       // Toggle state
 */

#ifndef LedDriver_H
#define LedDriver_H

#include <Arduino.h>

/**
 * @class LedDriver
 * @brief Digital output driver for controlling an LED
 * 
 * Wraps Arduino GPIO functions (digitalWrite, pinMode, digitalRead)
 * to provide a clean interface for LED control.
 */
class LedDriver
{
private:
    uint8_t ledPin;  // GPIO pin number for this LED

public:
    /**
     * @brief Constructor
     * @param pin GPIO pin number where LED is connected
     */
    LedDriver(uint8_t pin);
    
    /**
     * @brief Initialize the LED (configure pin as output, set to OFF)
     */
    void Init();
    
    /**
     * @brief Turn LED ON
     */
    void On();
    
    /**
     * @brief Turn LED OFF
     */
    void Off();
    
    /**
     * @brief Toggle LED state (ON->OFF or OFF->ON)
     */
    void Toggle();
};

#endif
