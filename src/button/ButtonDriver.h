/**
 * @file ButtonDriver.h
 * @brief ECAL Layer - Push-Button Hardware Abstraction Driver
 *
 * Provides a non-blocking, debounced interface for a push-button.
 * Supports both active-low(INPUT_PULLUP) and active-high(INPUT) wiring.
 * The driver is designed to be called periodically (every ~20 ms)
 * via a bare-metal task scheduler; it never busy-waits.
 *
 * Debounce algorithm:
 *   The raw pin reading must remain stable for DEBOUNCE_SAMPLES
 *   consecutive Update() calls before the state is accepted.
 *   At 20 ms period this gives 5 × 20 ms = 100 ms debounce window,
 *   which is well within the ~200 ms minimum button press interval.
 *
 * Pin convention:
 *   Configurable at construction time:
 *   - active-low  + INPUT_PULLUP (button to GND)
 *   - active-high + INPUT        (button to +5V with external pull-down)
 *
 * Usage example (bare-metal task):
 *   ButtonDriver btn(A4);
 *   btn.Init();
 *   // --- called every 20 ms ---
 *   btn.Update();
 *   if (btn.WasJustReleased()) {
 *       uint32_t dur = btn.GetLastPressDuration(); // ms
 *   }
 *
 * Architecture: Lab2 -> ButtonDriver -> GPIO MCAL -> Arduino GPIO Hardware
 */

#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include <Arduino.h>

class ButtonDriver
{
public:

    explicit ButtonDriver(uint8_t pin,
                     bool activeLow = true,
                     bool useInternalPullup = true);

    void Init();

    void Update();

    bool IsPressed() const;

    bool WasJustReleased();

    uint32_t GetLastPressDuration() const;

private:
    uint8_t  _pin;                    ///< GPIO pin index
    bool     _activeLow;              ///< true: pressed=LOW, false: pressed=HIGH
    bool     _useInternalPullup;      ///< true: pinMode INPUT_PULLUP, false: INPUT
    bool     _debouncedState;         ///< Accepted (debounced) button state
    uint8_t  _sampleCount;            ///< Consecutive samples counter
    bool     _releaseFlag;            ///< Set on falling edge, cleared after read
    uint32_t _pressStartMs;           ///< millis() timestamp of last press start
    uint32_t _lastDurationMs;         ///< Duration of the last completed press

    /** Number of consecutive matching samples required to accept a state change.
     *  With WDT-based FreeRTOS tick (~16 ms), 3 samples ≈ 48 ms debounce window.
     */
    static const uint8_t DEBOUNCE_SAMPLES = 3;
};

#endif // BUTTON_DRIVER_H
