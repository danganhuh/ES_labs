#ifndef LedDriver_H
#define LedDriver_H

#include <Arduino.h>

class LedDriver
{
private:
    uint8_t ledPin;

public:
    LedDriver(uint8_t pin);
    void Init();
    void On();
    void Off();
    void Toggle();
};

#endif
