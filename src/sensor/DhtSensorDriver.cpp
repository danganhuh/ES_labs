#include "DhtSensorDriver.h"

#include <math.h>

#ifndef DHTTYPE
#define DHTTYPE DHT22
#endif

DhtSensorDriver::DhtSensorDriver(uint8_t dataPin)
    : pin(dataPin),
      dht(dataPin, DHTTYPE)
{
}

void DhtSensorDriver::Init()
{
    // DHT data line is idle-high; pull-up helps stable reads in simulation and hardware.
    pinMode(pin, INPUT_PULLUP);
    delay(20);
    dht.begin();
    delay(1000);
}

bool DhtSensorDriver::Read(float* outTempC, float* outHumidityPct)
{
    if (outTempC == NULL || outHumidityPct == NULL)
    {
        return false;
    }

    // Retry a few times because DHT timing may occasionally fail in RTOS context.
    for (uint8_t attempt = 0; attempt < 3u; ++attempt)
    {
        float h = dht.readHumidity(false);
        float t = dht.readTemperature(false, false);

        // If cached/non-forced read fails, trigger one fresh bus transaction.
        if (isnan(t) || isnan(h))
        {
            h = dht.readHumidity(true);
            t = dht.readTemperature(false, true);
        }

        if (!isnan(t) && !isnan(h)
            && t >= -40.0f && t <= 85.0f
            && h >= 0.0f && h <= 100.0f)
        {
            *outTempC = t;
            *outHumidityPct = h;
            return true;
        }

        delay(5);
    }

    return false;
}
