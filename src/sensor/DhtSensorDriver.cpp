#include "DhtSensorDriver.h"

#include <math.h>

DhtSensorDriver::DhtSensorDriver(uint8_t dataPin, uint8_t sensorType)
        : pin(dataPin),
            type(sensorType),
            dht(dataPin, sensorType)
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

    // Trigger ONE fresh bus transaction (force=true bypasses the library's
    // MIN_INTERVAL=2000ms guard, which otherwise quietly returns cached data
    // if our 2 s schedule undershoots by even 1 ms due to clock drift).
    // The subsequent readHumidity()/readTemperature() calls then decode from
    // the same in-memory buffer (they are cache hits, NOT extra bus reads).
    if (!dht.read(true))
    {
        return false;
    }

    const float h = dht.readHumidity();
    const float t = dht.readTemperature();

    if (!isnan(t) && !isnan(h)
        && t >= -40.0f && t <= 85.0f
        && h >= 0.0f && h <= 100.0f)
    {
        *outTempC = t;
        *outHumidityPct = h;
        return true;
    }

    return false;
}
