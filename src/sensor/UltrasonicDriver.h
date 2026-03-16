#ifndef ULTRASONIC_DRIVER_H
#define ULTRASONIC_DRIVER_H

#include <Arduino.h>

class UltrasonicDriver
{
private:
    uint8_t trigPin;
    uint8_t echoPin;
    unsigned long echoTimeoutUs;

public:
    UltrasonicDriver(uint8_t trig, uint8_t echo, unsigned long timeoutUs = 30000UL);

    void Init();
    bool ReadDistanceCm(float* outDistanceCm);
};

#endif
