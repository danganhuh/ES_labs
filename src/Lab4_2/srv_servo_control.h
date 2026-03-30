#ifndef LAB4_2_SRV_SERVO_CONTROL_H
#define LAB4_2_SRV_SERVO_CONTROL_H

#include <Arduino.h>
#include <Servo.h>

typedef struct
{
    Servo servo;
    uint8_t pin;
    uint16_t minDeg;
    uint16_t maxDeg;
    float appliedPct;
    uint16_t appliedDeg;
    bool initialized;
} ServoControl42;

void servo42_init(ServoControl42* ctrl, uint8_t pin, uint16_t minDeg, uint16_t maxDeg);
void servo42_apply_pct(ServoControl42* ctrl, float pct);
float servo42_get_pct(const ServoControl42* ctrl);
uint16_t servo42_get_deg(const ServoControl42* ctrl);

#endif
