#ifndef LAB4_2_SRV_RELAY_CONTROL_H
#define LAB4_2_SRV_RELAY_CONTROL_H

#include <Arduino.h>

typedef struct
{
    uint8_t relayPin;
    bool activeLow;
    bool appliedState;
    bool candidateState;
    unsigned long candidateSinceMs;
    uint16_t debounceMs;
    bool debounceAlert;
    bool initialized;
    uint32_t transitions;
} RelayControl42;

void relay42_init(RelayControl42* ctrl, uint8_t relayPin, bool activeLow, uint16_t debounceMs, bool initialState);
void relay42_set_command(RelayControl42* ctrl, bool requestedState, unsigned long nowMs);
void relay42_update(RelayControl42* ctrl, unsigned long nowMs);
bool relay42_get_state(const RelayControl42* ctrl);
bool relay42_consume_debounce_alert(RelayControl42* ctrl);
uint32_t relay42_get_transitions(const RelayControl42* ctrl);

#endif
