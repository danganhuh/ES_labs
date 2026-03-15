#ifndef DHT_SENSOR_DRIVER_H
#define DHT_SENSOR_DRIVER_H

#include <Arduino.h>
#include <DHT.h>

class DhtSensorDriver
{
private:
    uint8_t pin;
    DHT dht;

public:
    explicit DhtSensorDriver(uint8_t dataPin);

    void Init();
    bool Read(float* outTempC, float* outHumidityPct);
};

#endif
