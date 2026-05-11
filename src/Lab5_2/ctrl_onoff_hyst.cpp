#include "ctrl_onoff_hyst.h"

void OnOffHysteresisController52::Init(bool initialFanOn)
{
    fanOn = initialFanOn;
}

bool OnOffHysteresisController52::Step(float measuredC, float setpointC, float hysteresisC)
{
    /* Normalise sign — a negative hysteresis would flip the dead-band and
     * essentially break the controller, so we treat |H| as the half-band. */
    if (hysteresisC < 0.0f)
    {
        hysteresisC = -hysteresisC;
    }

    const float upperThreshold = setpointC + hysteresisC;
    const float lowerThreshold = setpointC - hysteresisC;

    /* Cooling logic: too hot ⇒ start cooling, cool enough ⇒ stop. */
    if (measuredC >= upperThreshold)
    {
        fanOn = true;
    }
    else if (measuredC <= lowerThreshold)
    {
        fanOn = false;
    }
    /* else: inside the dead-band — latch the previous decision. */

    return fanOn;
}

bool OnOffHysteresisController52::IsFanOn() const
{
    return fanOn;
}
