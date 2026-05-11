#include "ctrl_onoff_hyst.h"

void OnOffHysteresisController52::Init(bool initialHeaterOn)
{
    heaterOn = initialHeaterOn;
}

bool OnOffHysteresisController52::Step(float measuredValue, float setpoint, float hysteresis)
{
    if (hysteresis < 0.0f)
    {
        hysteresis = -hysteresis;
    }

    const float lowerThreshold = setpoint - hysteresis;
    const float upperThreshold = setpoint + hysteresis;

    if (measuredValue >= upperThreshold)
    {
        heaterOn = false;
    }
    else if (measuredValue <= lowerThreshold)
    {
        heaterOn = true;
    }
    /* else: inside the dead-band, hold previous state. */

    return heaterOn;
}

bool OnOffHysteresisController52::IsHeaterOn() const
{
    return heaterOn;
}
