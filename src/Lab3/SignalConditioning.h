#ifndef LAB3_SIGNAL_CONDITIONING_H
#define LAB3_SIGNAL_CONDITIONING_H

#include <Arduino.h>

struct ConditioningConfig
{
    float thresholdC;
    float hysteresisC;
    float alpha;
    float minTempC;
    float maxTempC;
    uint16_t persistenceSamples;
};

struct ConditioningResult
{
    float clampedTempC;
    float filteredTempC;
    bool hysteresisCandidate;
    bool debouncedAlert;
    uint16_t pendingCount;
    bool stateChanged;
};

class SignalConditioner
{
private:
    ConditioningConfig cfg;
    bool filterInitialized;
    float filtered;
    bool hysteresisState;
    bool debouncedState;
    uint16_t pendingTransitions;

public:
    SignalConditioner();
    void Configure(const ConditioningConfig& config);
    ConditioningResult Process(float measuredTempC);
};

#endif
