#include "ed_l298.h"
#include "lib_sig_cond.h"

static uint8_t s_enaPin = 0u;
static uint8_t s_in1Pin = 0u;
static uint8_t s_in2Pin = 0u;
static float s_speedPct = 0.0f;

void ed_l298_init(uint8_t enaPin, uint8_t in1Pin, uint8_t in2Pin)
{
    s_enaPin = enaPin;
    s_in1Pin = in1Pin;
    s_in2Pin = in2Pin;

    pinMode(s_enaPin, OUTPUT);
    pinMode(s_in1Pin, OUTPUT);
    pinMode(s_in2Pin, OUTPUT);

    analogWrite(s_enaPin, 0);
    digitalWrite(s_in1Pin, LOW);
    digitalWrite(s_in2Pin, LOW);
    s_speedPct = 0.0f;
}

void ed_l298_set_speed_pct(float speedPct)
{
    s_speedPct = sig_cond_saturate_float(speedPct, -100.0f, 100.0f);

    const float absPct = (s_speedPct >= 0.0f) ? s_speedPct : -s_speedPct;
    const int pwm = static_cast<int>((absPct / 100.0f) * 255.0f);

    if (pwm <= 0)
    {
        analogWrite(s_enaPin, 0);
        digitalWrite(s_in1Pin, LOW);
        digitalWrite(s_in2Pin, LOW);
        return;
    }

    analogWrite(s_enaPin, pwm);
    if (s_speedPct >= 0.0f)
    {
        digitalWrite(s_in1Pin, HIGH);
        digitalWrite(s_in2Pin, LOW);
    }
    else
    {
        digitalWrite(s_in1Pin, LOW);
        digitalWrite(s_in2Pin, HIGH);
    }
}

float ed_l298_get_speed_pct()
{
    return s_speedPct;
}
