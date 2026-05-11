#ifndef LAB5_2_CTRL_ONOFF_HYST_H
#define LAB5_2_CTRL_ONOFF_HYST_H

/**
 * Heater-only ON/OFF controller with symmetric hysteresis around the set-point.
 *
 * State diagram:
 *
 *      PV <= SP - H              SP - H < PV < SP + H              PV >= SP + H
 *   ┌────────────────┐         ┌────────────────────┐         ┌─────────────────┐
 *   │  HEATER  ON    │ ──────► │   HOLD  PREVIOUS   │ ──────► │  HEATER  OFF    │
 *   └────────────────┘         └────────────────────┘         └─────────────────┘
 *           ▲                                                          │
 *           └──────────────────────────────────────────────────────────┘
 *                                  PV drops below SP - H
 *
 *  - Above the upper threshold (SP + H): heater forced OFF.
 *  - Below the lower threshold (SP - H): heater forced ON.
 *  - Inside the dead-band: state is latched (no chattering).
 */
class OnOffHysteresisController52
{
public:
    void Init(bool initialHeaterOn);

    /** Returns the new heater state. `hysteresis` is the half-band, |C|. */
    bool Step(float measuredValue, float setpoint, float hysteresis);

    bool IsHeaterOn() const;

private:
    bool heaterOn;
};

#endif
