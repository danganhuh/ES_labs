#include "ed_relay.h"
#include "Lab4_Shared.h"
#include "../led/LedDriver.h"

static LedDriver* s_relay = NULL;
static int s_state = LOW;

void ed_relay_init(uint8_t pin)
{
    if (s_relay == NULL)
    {
        s_relay = new LedDriver(pin);
    }
    s_relay->Init();
    s_state = LOW;
}

void ed_relay_set_state(int state)
{
    if (s_relay == NULL)
    {
        return;
    }

    s_state = (state == HIGH) ? HIGH : LOW;
    bool shouldEnergize = (s_state == HIGH);
#if LAB4_RELAY_ACTIVE_LOW
    shouldEnergize = !shouldEnergize;
#endif

    if (shouldEnergize)
    {
        s_relay->On();
    }
    else
    {
        s_relay->Off();
    }
}

int ed_relay_get_state()
{
    return s_state;
}
