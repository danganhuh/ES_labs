#include "LedDriver.h"

LedDriver::LedDriver(uint8_t pin)
{
    ledPin = pin;
}

void LedDriver::Init()
{
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);
}

void LedDriver::On()
{
    digitalWrite(ledPin, HIGH);
}

void LedDriver::Off()
{
    digitalWrite(ledPin, LOW);
}

void LedDriver::Toggle()
{
    digitalWrite(ledPin, !digitalRead(ledPin));
}
