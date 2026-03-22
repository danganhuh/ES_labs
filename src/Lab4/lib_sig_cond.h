#ifndef LAB4_LIB_SIG_COND_H
#define LAB4_LIB_SIG_COND_H

#include <Arduino.h>

int sig_cond_saturate_int(int value, int minValue, int maxValue);
float sig_cond_saturate_float(float value, float minValue, float maxValue);

int sig_cond_time_abc_bin(int desiredState,
                          int currentState,
                          uint16_t minOnMs,
                          uint16_t minOffMs,
                          unsigned long nowMs,
                          unsigned long* lastChangeMs,
                          bool* transitionBlocked);

float sig_cond_ema(float input, float previous, float alpha);
float sig_cond_ramp(float desiredValue, float currentValue, float accelPerSec, float dtSec);

#endif
