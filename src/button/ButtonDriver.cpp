/**
 * @file ButtonDriver.cpp
 * @brief ECAL Layer - Push-Button Driver Implementation
 *
 * Implements the non-blocking debounce state machine for a push-button.
 *
 * Debounce logic summary:
 *   • Raw pin read interpreted by configured polarity:
 *       - active-low  => LOW = pressed
 *       - active-high => HIGH = pressed
 *   • While the raw reading matches the accepted state → reset sample counter.
 *   • While differing → increment counter.
 *   • Once counter reaches DEBOUNCE_SAMPLES the new state is accepted:
 *       - Press  edge: record start time via millis().
 *       - Release edge: compute duration, set release flag.
 *
 * Architecture: Lab2 -> ButtonDriver -> GPIO MCAL -> Arduino GPIO Hardware
 */

#include "ButtonDriver.h"

ButtonDriver::ButtonDriver(uint8_t pin,
                                                     bool activeLow,
                                                     bool useInternalPullup)
    : _pin(pin),
            _activeLow(activeLow),
            _useInternalPullup(useInternalPullup), 
      _debouncedState(false),
      _sampleCount(0),
      _releaseFlag(false),
      _pressStartMs(0),
      _lastDurationMs(0)
{
}

void ButtonDriver::Init()
{
    pinMode(_pin, _useInternalPullup ? INPUT_PULLUP : INPUT);

    _debouncedState = false;
    _sampleCount    = 0;
    _releaseFlag    = false;
    _pressStartMs   = 0;
    _lastDurationMs = 0;
}


void ButtonDriver::Update()
{
    int rawValue = digitalRead(_pin);               // MCAL: digitalRead
    bool rawPressed = _activeLow ? (rawValue == LOW) : (rawValue == HIGH);

    if (rawPressed == _debouncedState)
    {
        // Stable – reset the transient counter
        _sampleCount = 0;
        return;
    }

    // State differs from accepted value – accumulate samples
    _sampleCount++;

    if (_sampleCount >= DEBOUNCE_SAMPLES)
    {
        _sampleCount = 0;

        bool prevState   = _debouncedState;
        _debouncedState  = rawPressed;

        if (!prevState && _debouncedState)
        {
            // Rising edge: button just pressed
            _pressStartMs = millis();              // MCAL: millis()
        }
        else if (prevState && !_debouncedState)
        {
            // Falling edge: button just released
            _lastDurationMs = millis() - _pressStartMs;  // MCAL: millis()
            _releaseFlag    = true;
        }
    }
}

bool ButtonDriver::IsPressed() const
{
    return _debouncedState;
}


bool ButtonDriver::WasJustReleased()
{
    if (_releaseFlag)
    {
        _releaseFlag = false;
        return true;
    }
    return false;
}

uint32_t ButtonDriver::GetLastPressDuration() const
{
    return _lastDurationMs;
}
