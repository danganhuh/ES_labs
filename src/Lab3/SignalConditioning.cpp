#include "SignalConditioning.h"

SignalConditioner::SignalConditioner()
    : filterInitialized(false),
      filtered(0.0f),
      hysteresisState(false),
      debouncedState(false),
      pendingTransitions(0u)
{
    cfg.thresholdC = 25.0f;
    cfg.hysteresisC = 1.0f;
    cfg.alpha = 0.25f;
    cfg.minTempC = -40.0f;
    cfg.maxTempC = 125.0f;
    cfg.persistenceSamples = 2u;
}

void SignalConditioner::Configure(const ConditioningConfig& config)
{
    cfg = config;
    if (cfg.alpha < 0.01f)
    {
        cfg.alpha = 0.01f;
    }
    if (cfg.alpha > 1.0f)
    {
        cfg.alpha = 1.0f;
    }
    if (cfg.persistenceSamples == 0u)
    {
        cfg.persistenceSamples = 1u;
    }

    filterInitialized = false;
    filtered = 0.0f;
    hysteresisState = false;
    debouncedState = false;
    pendingTransitions = 0u;
}

ConditioningResult SignalConditioner::Process(float measuredTempC)
{
    ConditioningResult result{};

    if (measuredTempC < cfg.minTempC)
    {
        result.clampedTempC = cfg.minTempC;
    }
    else if (measuredTempC > cfg.maxTempC)
    {
        result.clampedTempC = cfg.maxTempC;
    }
    else
    {
        result.clampedTempC = measuredTempC;
    }

    if (!filterInitialized)
    {
        filtered = result.clampedTempC;
        filterInitialized = true;
    }
    else
    {
        filtered = (cfg.alpha * result.clampedTempC) + ((1.0f - cfg.alpha) * filtered);
    }
    result.filteredTempC = filtered;

    const float high = cfg.thresholdC + cfg.hysteresisC;
    const float low  = cfg.thresholdC - cfg.hysteresisC;

    if (hysteresisState)
    {
        if (filtered <= low)
        {
            hysteresisState = false;
        }
    }
    else
    {
        if (filtered >= high)
        {
            hysteresisState = true;
        }
    }

    result.hysteresisCandidate = hysteresisState;

    if (result.hysteresisCandidate != debouncedState)
    {
        pendingTransitions++;
        if (pendingTransitions >= cfg.persistenceSamples)
        {
            debouncedState = result.hysteresisCandidate;
            result.stateChanged = true;
            pendingTransitions = 0u;
        }
    }
    else
    {
        pendingTransitions = 0u;
        result.stateChanged = false;
    }

    result.debouncedAlert = debouncedState;
    result.pendingCount = pendingTransitions;
    return result;
}
