#include "lib_sig_cond.h"

int sig_cond_saturate_int(int value, int minValue, int maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

float sig_cond_saturate_float(float value, float minValue, float maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

int sig_cond_time_abc_bin(int desiredState,
                          int currentState,
                          uint16_t minOnMs,
                          uint16_t minOffMs,
                          unsigned long nowMs,
                          unsigned long* lastChangeMs,
                          bool* transitionBlocked)
{
    if (transitionBlocked != NULL)
    {
        *transitionBlocked = false;
    }

    if (desiredState == currentState)
    {
        return currentState;
    }

    const uint16_t requiredMs = (desiredState == HIGH) ? minOnMs : minOffMs;
    const unsigned long elapsedMs = nowMs - *lastChangeMs;

    if (elapsedMs >= requiredMs)
    {
        *lastChangeMs = nowMs;
        return desiredState;
    }

    if (transitionBlocked != NULL)
    {
        *transitionBlocked = true;
    }

    return currentState;
}

float sig_cond_ema(float input, float previous, float alpha)
{
    float clampedAlpha = alpha;
    if (clampedAlpha < 0.01f) clampedAlpha = 0.01f;
    if (clampedAlpha > 1.0f) clampedAlpha = 1.0f;
    return (clampedAlpha * input) + ((1.0f - clampedAlpha) * previous);
}

float sig_cond_ramp(float desiredValue, float currentValue, float accelPerSec, float dtSec)
{
    if (dtSec <= 0.0f)
    {
        return currentValue;
    }

    const float deltaMax = accelPerSec * dtSec;
    if (desiredValue > (currentValue + deltaMax))
    {
        return currentValue + deltaMax;
    }
    if (desiredValue < (currentValue - deltaMax))
    {
        return currentValue - deltaMax;
    }

    return desiredValue;
}
