#ifndef LAB4_SRV_MOTOR_CONTROL_H
#define LAB4_SRV_MOTOR_CONTROL_H

#include <Arduino.h>

void srv_motor_control_init(uint8_t enaPin, uint8_t in1Pin, uint8_t in2Pin, float alpha, float rampPctPerSec);
void srv_motor_control_set_speed(float speedPct);
void srv_motor_control_condition(float dtSec);
void srv_motor_control_apply();

float srv_motor_control_get_state();
float srv_motor_control_get_desired();
bool srv_motor_control_get_saturation_alert();

#endif
