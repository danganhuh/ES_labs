#include "srv_servo_control.h"

static float clamp_pct(float value)
{
    if (value < 0.0f) { return 0.0f; }
    if (value > 100.0f) { return 100.0f; }
    return value;
}

void servo42_init(ServoControl42* ctrl, uint8_t pin, uint16_t minDeg, uint16_t maxDeg)
{
    ctrl->pin = pin;
    ctrl->minDeg = minDeg;
    ctrl->maxDeg = maxDeg;
    ctrl->appliedPct = 0.0f;
    ctrl->appliedDeg = minDeg;
    ctrl->initialized = true;

    ctrl->servo.attach(ctrl->pin);
    ctrl->servo.write((int)ctrl->appliedDeg);
}

void servo42_apply_pct(ServoControl42* ctrl, float pct)
{
    if (!ctrl->initialized) { return; }

    ctrl->appliedPct = clamp_pct(pct);
    const float span = (float)(ctrl->maxDeg - ctrl->minDeg);
    const float deg = ((ctrl->appliedPct / 100.0f) * span) + (float)ctrl->minDeg;

    ctrl->appliedDeg = (uint16_t)(deg + 0.5f);
    ctrl->servo.write((int)ctrl->appliedDeg);
}

float servo42_get_pct(const ServoControl42* ctrl)
{
    return ctrl->appliedPct;
}

uint16_t servo42_get_deg(const ServoControl42* ctrl)
{
    return ctrl->appliedDeg;
}
