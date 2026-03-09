#include "AlertManager.h"

AlertManager::AlertManager(uint8_t ledPin)
    : led(ledPin),
      active(false)
{
}

void AlertManager::Init()
{
    led.Init();
    led.Off();
    active = false;
}

bool AlertManager::ApplyState(bool nextState)
{
    const bool changed = (nextState != active);
    active = nextState;

    if (active)
    {
        led.On();
    }
    else
    {
        led.Off();
    }

    return changed;
}

bool AlertManager::IsActive() const
{
    return active;
}
