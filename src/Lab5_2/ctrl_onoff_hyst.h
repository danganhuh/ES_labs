#ifndef LAB5_2_CTRL_ONOFF_HYST_H
#define LAB5_2_CTRL_ONOFF_HYST_H

/**
 * Cooling-fan ON/OFF controller with symmetric hysteresis.
 *
 * Polarity is FLIPPED relative to a heater: here the actuator brings the PV
 * DOWN, so we turn the fan ON when the room is too hot and OFF when it has
 * cooled past the lower threshold. The dead-band in the middle is the
 * hysteresis zone — the previous decision is latched there to prevent
 * relay/MOSFET chatter when the temperature wobbles around the set-point.
 *
 *  PV >= SP + H            SP - H < PV < SP + H               PV <= SP - H
 *   ┌──────────┐          ┌───────────────────┐            ┌──────────┐
 *   │  FAN ON  │ ───────▶ │   HOLD PREVIOUS   │ ─────────▶ │  FAN OFF │
 *   └──────────┘          └───────────────────┘            └──────────┘
 *        ▲                                                       │
 *        └──── PV climbs back above SP + H ───────────────────────┘
 *
 * This mirrors the algorithm in the reference Lab 6.1
 * (`app_lab_6_1_i_ctrl_fan_task`) but is encapsulated in a class so the
 * latched state survives between control-task iterations without leaking
 * static globals into the task file.
 */
class OnOffHysteresisController52
{
public:
    void Init(bool initialFanOn);

    /**
     * Step the controller one tick.
     * @param measuredC   filtered process value (°C)
     * @param setpointC   target temperature (°C)
     * @param hysteresisC half-band (must be ≥ 0; sign is normalised)
     * @return            new fan state (true = ON / cooling, false = OFF)
     */
    bool Step(float measuredC, float setpointC, float hysteresisC);

    bool IsFanOn() const;

private:
    bool fanOn;
};

#endif
