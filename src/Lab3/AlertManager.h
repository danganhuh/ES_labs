#ifndef LAB3_ALERT_MANAGER_H
#define LAB3_ALERT_MANAGER_H

#include <Arduino.h>
#include "../led/LedDriver.h"

class AlertManager
{
private:
    LedDriver led;
    bool active;

public:
    explicit AlertManager(uint8_t ledPin);

    void Init();
    bool ApplyState(bool nextState);
    bool IsActive() const;
};

#endif
