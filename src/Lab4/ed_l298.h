#ifndef LAB4_ED_L298_H
#define LAB4_ED_L298_H

#include <Arduino.h>

void ed_l298_init(uint8_t enaPin, uint8_t in1Pin, uint8_t in2Pin);
void ed_l298_set_speed_pct(float speedPct);
float ed_l298_get_speed_pct();

#endif
