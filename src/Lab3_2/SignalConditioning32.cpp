#include "SignalConditioning32.h"

Lab32SignalConditioner::Lab32SignalConditioner()
    : filterInitialized(false),
      filtered(0.0f),
      medianWindow{0.0f, 0.0f, 0.0f},
      medianCount(0u),
      medianIndex(0u)
{
    cfg.alpha = 0.30f;
    cfg.minValue = -40.0f;
    cfg.maxValue = 125.0f;
}

void Lab32SignalConditioner::Configure(const Lab32ConditioningConfig& config)
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
    if (cfg.maxValue < cfg.minValue)
    {
        const float tmp = cfg.maxValue;
        cfg.maxValue = cfg.minValue;
        cfg.minValue = tmp;
    }

    filterInitialized = false;
    filtered = 0.0f;
    medianWindow[0] = 0.0f;
    medianWindow[1] = 0.0f;
    medianWindow[2] = 0.0f;
    medianCount = 0u;
    medianIndex = 0u;
}

Lab32ConditioningResult Lab32SignalConditioner::Process(float rawValue)
{
    Lab32ConditioningResult result{};

    if (rawValue < cfg.minValue)
    {
        result.clampedValue = cfg.minValue;
    }
    else if (rawValue > cfg.maxValue)
    {
        result.clampedValue = cfg.maxValue;
    }
    else
    {
        result.clampedValue = rawValue;
    }

    medianWindow[medianIndex] = result.clampedValue;
    medianIndex = static_cast<uint8_t>((medianIndex + 1u) % 3u);
    if (medianCount < 3u)
    {
        medianCount++;
    }

    if (medianCount == 1u)
    {
        result.medianValue = medianWindow[0];
    }
    else if (medianCount == 2u)
    {
        result.medianValue = (medianWindow[0] + medianWindow[1]) * 0.5f;
    }
    else
    {
        float a = medianWindow[0];
        float b = medianWindow[1];
        float c = medianWindow[2];

        if (a > b)
        {
            const float t = a;
            a = b;
            b = t;
        }
        if (b > c)
        {
            const float t = b;
            b = c;
            c = t;
        }
        if (a > b)
        {
            const float t = a;
            a = b;
            b = t;
        }

        result.medianValue = b;
    }

    if (!filterInitialized)
    {
        filtered = result.medianValue;
        filterInitialized = true;
    }
    else
    {
        filtered = (cfg.alpha * result.medianValue) + ((1.0f - cfg.alpha) * filtered);
    }
    result.filteredValue = filtered;

    return result;
}
