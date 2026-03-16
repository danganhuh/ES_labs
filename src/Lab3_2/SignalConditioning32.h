#ifndef LAB3_2_SIGNAL_CONDITIONING32_H
#define LAB3_2_SIGNAL_CONDITIONING32_H

#include <Arduino.h>

struct Lab32ConditioningConfig
{
    float alpha;
    float minValue;
    float maxValue;
};

struct Lab32ConditioningResult
{
    float clampedValue;
    float medianValue;
    float filteredValue;
};

class Lab32SignalConditioner
{
private:
    Lab32ConditioningConfig cfg;
    bool filterInitialized;
    float filtered;
    float medianWindow[3];
    uint8_t medianCount;
    uint8_t medianIndex;

public:
    Lab32SignalConditioner();
    void Configure(const Lab32ConditioningConfig& config);
    Lab32ConditioningResult Process(float rawValue);
};

#endif
