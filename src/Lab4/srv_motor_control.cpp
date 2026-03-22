#include "srv_motor_control.h"
#include "ed_l298.h"
#include "lib_sig_cond.h"

static float s_desiredSpeedPct = 0.0f;
static float s_filteredCmdPct = 0.0f;
static float s_currentSpeedPct = 0.0f;
static float s_alpha = 0.30f;
static float s_rampPctPerSec = 250.0f;
static bool s_saturationAlert = false;

void srv_motor_control_init(uint8_t enaPin, uint8_t in1Pin, uint8_t in2Pin, float alpha, float rampPctPerSec)
{
    ed_l298_init(enaPin, in1Pin, in2Pin);

    s_desiredSpeedPct = 0.0f;
    s_filteredCmdPct = 0.0f;
    s_currentSpeedPct = 0.0f;
    s_alpha = alpha;
    s_rampPctPerSec = rampPctPerSec;
    s_saturationAlert = false;

    if (s_alpha < 0.01f) s_alpha = 0.01f;
    if (s_alpha > 1.0f) s_alpha = 1.0f;
    if (s_rampPctPerSec < 20.0f) s_rampPctPerSec = 20.0f;
}

void srv_motor_control_set_speed(float speedPct)
{
    s_desiredSpeedPct = speedPct;
}

void srv_motor_control_condition(float dtSec)
{
    const float clamped = sig_cond_saturate_float(s_desiredSpeedPct, -100.0f, 100.0f);
    s_saturationAlert = (clamped != s_desiredSpeedPct);

    s_filteredCmdPct = sig_cond_ema(clamped, s_filteredCmdPct, s_alpha);
    s_currentSpeedPct = sig_cond_ramp(s_filteredCmdPct, s_currentSpeedPct, s_rampPctPerSec, dtSec);
    s_currentSpeedPct = sig_cond_saturate_float(s_currentSpeedPct, -100.0f, 100.0f);
}

void srv_motor_control_apply()
{
    ed_l298_set_speed_pct(s_currentSpeedPct);
}

float srv_motor_control_get_state()
{
    return s_currentSpeedPct;
}

float srv_motor_control_get_desired()
{
    return s_desiredSpeedPct;
}

bool srv_motor_control_get_saturation_alert()
{
    return s_saturationAlert;
}
