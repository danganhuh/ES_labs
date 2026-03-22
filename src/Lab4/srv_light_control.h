#ifndef LAB4_SRV_LIGHT_CONTROL_H
#define LAB4_SRV_LIGHT_CONTROL_H

#include <Arduino.h>

void srv_light_control_init(uint8_t relayPin, uint16_t debounceOnMs, uint16_t debounceOffMs);
void srv_light_control_set_state(int commandState);
void srv_light_control_condition(unsigned long nowMs);
void srv_light_control_apply();

int srv_light_control_get_state();
bool srv_light_control_get_alert();
uint32_t srv_light_control_get_transitions();
bool srv_light_control_get_state_changed();

int actuator_get_state();

#endif
