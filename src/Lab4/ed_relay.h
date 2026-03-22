#ifndef LAB4_ED_RELAY_H
#define LAB4_ED_RELAY_H

#include <Arduino.h>

void ed_relay_init(uint8_t pin);
void ed_relay_set_state(int state);
int ed_relay_get_state();

#endif
