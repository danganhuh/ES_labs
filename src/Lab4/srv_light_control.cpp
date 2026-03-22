#include "srv_light_control.h"
#include "ed_relay.h"
#include "lib_sig_cond.h"

static int s_desiredState = LOW;
static int s_currentState = LOW;
static unsigned long s_lastChangeMs = 0UL;
static uint16_t s_debounceOnMs = 80u;
static uint16_t s_debounceOffMs = 80u;
static bool s_blockedTransition = false;
static uint32_t s_stateTransitions = 0UL;
static bool s_stateChanged = false;

void srv_light_control_init(uint8_t relayPin, uint16_t debounceOnMs, uint16_t debounceOffMs)
{
    ed_relay_init(relayPin);

    s_desiredState = LOW;
    s_currentState = LOW;
    s_lastChangeMs = millis();
    s_debounceOnMs = debounceOnMs;
    s_debounceOffMs = debounceOffMs;
    s_blockedTransition = false;
    s_stateTransitions = 0UL;
    s_stateChanged = false;

    ed_relay_set_state(LOW);
}

void srv_light_control_set_state(int commandState)
{
    s_desiredState = (commandState == HIGH) ? HIGH : LOW;
}

void srv_light_control_condition(unsigned long nowMs)
{
    s_stateChanged = false;
    bool blocked = false;

    const int prev = s_currentState;
    s_currentState = sig_cond_time_abc_bin(s_desiredState,
                                           s_currentState,
                                           s_debounceOnMs,
                                           s_debounceOffMs,
                                           nowMs,
                                           &s_lastChangeMs,
                                           &blocked);

    s_blockedTransition = blocked;
    if (s_currentState != prev)
    {
        s_stateTransitions++;
        s_stateChanged = true;
    }
}

void srv_light_control_apply()
{
    ed_relay_set_state(s_currentState);
}

int srv_light_control_get_state()
{
    return s_currentState;
}

bool srv_light_control_get_alert()
{
    return s_blockedTransition;
}

uint32_t srv_light_control_get_transitions()
{
    return s_stateTransitions;
}

bool srv_light_control_get_state_changed()
{
    return s_stateChanged;
}

int actuator_get_state()
{
    return srv_light_control_get_state();
}
