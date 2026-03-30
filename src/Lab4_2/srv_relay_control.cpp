#include "srv_relay_control.h"

static void write_relay(uint8_t pin, bool activeLow, bool logicalOn)
{
    const uint8_t out = activeLow ? (logicalOn ? LOW : HIGH) : (logicalOn ? HIGH : LOW);
    digitalWrite(pin, out);
}

void relay42_init(RelayControl42* ctrl, uint8_t relayPin, bool activeLow, uint16_t debounceMs, bool initialState)
{
    ctrl->relayPin = relayPin;
    ctrl->activeLow = activeLow;
    ctrl->appliedState = initialState;
    ctrl->candidateState = initialState;
    ctrl->candidateSinceMs = millis();
    ctrl->debounceMs = debounceMs;
    ctrl->debounceAlert = false;
    ctrl->initialized = true;
    ctrl->transitions = 0u;

    pinMode(ctrl->relayPin, OUTPUT);
    write_relay(ctrl->relayPin, ctrl->activeLow, ctrl->appliedState);
}

void relay42_set_command(RelayControl42* ctrl, bool requestedState, unsigned long nowMs)
{
    if (!ctrl->initialized) { return; }

    if (requestedState != ctrl->candidateState)
    {
        if ((ctrl->candidateState != ctrl->appliedState) && ((nowMs - ctrl->candidateSinceMs) < ctrl->debounceMs))
        {
            ctrl->debounceAlert = true;
        }

        ctrl->candidateState = requestedState;
        ctrl->candidateSinceMs = nowMs;
    }
}

void relay42_update(RelayControl42* ctrl, unsigned long nowMs)
{
    if (!ctrl->initialized) { return; }

    if ((ctrl->candidateState != ctrl->appliedState) && ((nowMs - ctrl->candidateSinceMs) >= ctrl->debounceMs))
    {
        ctrl->appliedState = ctrl->candidateState;
        ctrl->transitions++;
        write_relay(ctrl->relayPin, ctrl->activeLow, ctrl->appliedState);
    }
}

bool relay42_get_state(const RelayControl42* ctrl)
{
    return ctrl->appliedState;
}

bool relay42_consume_debounce_alert(RelayControl42* ctrl)
{
    const bool flagged = ctrl->debounceAlert;
    ctrl->debounceAlert = false;
    return flagged;
}

uint32_t relay42_get_transitions(const RelayControl42* ctrl)
{
    return ctrl->transitions;
}
