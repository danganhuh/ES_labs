#include "ctrl_pid.h"

void PidController52::Init(float kp, float ki, float kd, float limitAbs, float initialOutput)
{
    this->kp = kp;
    this->ki = ki;
    this->kd = kd;
    this->limitAbs = (limitAbs > 1.0f) ? limitAbs : 1.0f;

    Reset(initialOutput);
}

void PidController52::Reset(float initialOutput)
{
    integral = 0.0f;
    prevError = initialOutput;
    prevDerivative = 0.0f;
    initialized = false;
}

void PidController52::Configure(float kp, float ki, float kd, float limitAbs)
{
    this->kp = kp;
    this->ki = ki;
    this->kd = kd;
    this->limitAbs = (limitAbs > 1.0f) ? limitAbs : 1.0f;
}

float PidController52::clamp(float value, float limit) const
{
    if (value > limit) { return limit; }
    if (value < -limit) { return -limit; }
    return value;
}

float PidController52::Step(float error, float dtSeconds)
{
    if (dtSeconds < 0.001f)
    {
        dtSeconds = 0.001f;
    }

    if (!initialized)
    {
        prevError = error;
        prevDerivative = 0.0f;
        initialized = true;
    }

    const float rawDerivative = (error - prevError) / dtSeconds;
    const float derivFilterTauS = 0.12f;
    const float derivAlpha = dtSeconds / (derivFilterTauS + dtSeconds);
    const float derivative = prevDerivative + (derivAlpha * (rawDerivative - prevDerivative));

    const float pTerm = kp * error;
    const float dTerm = kd * derivative;

    const float tentativeIntegral = integral + (error * dtSeconds);

    const float rawWithoutITerm = pTerm + dTerm;
    const float tentativeRaw = rawWithoutITerm + (ki * tentativeIntegral);
    const bool saturatingHigh = (tentativeRaw > limitAbs);
    const bool saturatingLow = (tentativeRaw < -limitAbs);
    const bool pushingFurtherHigh = saturatingHigh && (error > 0.0f);
    const bool pushingFurtherLow = saturatingLow && (error < 0.0f);

    if (!(pushingFurtherHigh || pushingFurtherLow))
    {
        integral = tentativeIntegral;
    }

    float iTerm = ki * integral;

    float raw = pTerm + iTerm + dTerm;
    float saturated = clamp(raw, limitAbs);

    prevError = error;
    prevDerivative = derivative;
    return saturated;
}
