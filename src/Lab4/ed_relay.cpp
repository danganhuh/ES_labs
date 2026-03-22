#include "ed_relay.h"
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
    if (s_state == HIGH)
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
