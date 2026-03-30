#ifndef LAB4_2_LIB_SIG_COND_H
#define LAB4_2_LIB_SIG_COND_H

#include <Arduino.h>

typedef struct
{
    float samples[3];
    uint8_t sampleIndex;
    uint8_t sampleCount;
    float weighted;
    float ramped;
    unsigned long lastMs;
    bool initialized;
} SigCond42State;

void sig42_init(SigCond42State* state, float initialPct, unsigned long nowMs);

void sig42_step(
    SigCond42State* state,
    float rawPct,
    unsigned long nowMs,
    float alpha,
    float rampPctPerSec,
    float* outClampedPct,
    float* outMedianPct,
    float* outWeightedPct,
    float* outRampedPct,
    bool* outClampAlert,
    bool* outLimitAlert);

#endif
