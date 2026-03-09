#ifndef NTC_ADC_DRIVER_H
#define NTC_ADC_DRIVER_H

#include <Arduino.h>

class NtcAdcDriver
{
private:
    uint8_t analogPin;
    float   vRef;
    float   beta;
    float   rNominal;
    float   tNominalK;
    float   seriesResistor;

public:
    explicit NtcAdcDriver(uint8_t pin,
                          float referenceVoltage = 5.0f,
                          float betaValue = 3950.0f,
                          float nominalRes = 10000.0f,
                          float nominalTempC = 25.0f,
                          float seriesRes = 10000.0f);

    void Init();
    uint16_t ReadRaw() const;
    float RawToVoltage(uint16_t raw) const;
    float RawToTemperatureC(uint16_t raw) const;
};

#endif
