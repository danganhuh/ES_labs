#include "ctrl_pid.h"

#include "Lab5_2_Shared.h"

void PidController52::Init(float kpIn, float kiIn, float kdIn, float limitAbsIn, float initialOutput)
{
    this->kp = kpIn;
    this->ki = kiIn;
    this->kd = kdIn;
    this->limitAbs = (limitAbsIn > 1.0f) ? limitAbsIn : 1.0f;

    Reset(initialOutput);
}

void PidController52::Reset(float initialOutput)
{
    integral    = 0.0f;
    prevError   = initialOutput;
    initialized = false;
}

void PidController52::Configure(float kpIn, float kiIn, float kdIn, float limitAbsIn)
{
    this->kp       = kpIn;
    this->ki       = kiIn;
    this->kd       = kdIn;
    this->limitAbs = (limitAbsIn > 1.0f) ? limitAbsIn : 1.0f;
}

float PidController52::clamp(float value, float limit) const
{
    if (value >  limit) { return  limit; }
    if (value < -limit) { return -limit; }
    return value;
}

float PidController52::Step(float error, float dtSeconds)
{
    /* Guard against pathological dt (e.g. on the very first call from a
     * task that just woke up). 1 ms keeps the derivative term finite. */
    if (dtSeconds < 0.001f)
    {
        dtSeconds = 0.001f;
    }

    if (!initialized)
    {
        prevError   = error;
        initialized = true;
    }

    /* --- Proportional term --------------------------------------------- */
    const float pTerm = kp * error;

    /* --- Integral term with anti-windup -------------------------------- */
    /* Accumulate the running error*dt and hard-clamp the accumulator. The
     * reference solution clamps at +100 only (cooling-only plant); we keep
     * the same magnitude for behavioural parity but allow negative values
     * so the integrator can also DIS-charge symmetrically.               */
    integral += error * dtSeconds;
    if (integral >  LAB5_2_PID_INTEGRAL_LIMIT) { integral =  LAB5_2_PID_INTEGRAL_LIMIT; }
    if (integral < -LAB5_2_PID_INTEGRAL_LIMIT) { integral = -LAB5_2_PID_INTEGRAL_LIMIT; }
    const float iTerm = ki * integral;

    /* --- Derivative term (on error) ------------------------------------ */
    const float derivative = (error - prevError) / dtSeconds;
    const float dTerm = kd * derivative;

    /* --- Compose, saturate, store -------------------------------------- */
    const float raw       = pTerm + iTerm + dTerm;
    const float saturated = clamp(raw, limitAbs);

    prevError = error;
    return saturated;
}
