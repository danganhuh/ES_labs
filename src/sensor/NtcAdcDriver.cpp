#include "NtcAdcDriver.h"
#include <math.h>

NtcAdcDriver::NtcAdcDriver(uint8_t pin,
                           float referenceVoltage,
                           float betaValue,
                           float nominalRes,
                           float nominalTempC,
                           float seriesRes)
    : analogPin(pin),
      vRef(referenceVoltage),
      beta(betaValue),
      rNominal(nominalRes),
      tNominalK(nominalTempC + 273.15f),
      seriesResistor(seriesRes)
{
}

void NtcAdcDriver::Init()
{
    pinMode(analogPin, INPUT);
}

uint16_t NtcAdcDriver::ReadRaw() const
{
    return static_cast<uint16_t>(analogRead(analogPin));
}

float NtcAdcDriver::RawToVoltage(uint16_t raw) const
{
    return (static_cast<float>(raw) * vRef) / 1023.0f;
}

float NtcAdcDriver::RawToTemperatureC(uint16_t raw) const
{
    if (raw == 0u)
    {
        raw = 1u;
    }
    if (raw >= 1023u)
    {
        raw = 1022u;
    }

    const float ratio = (1023.0f / static_cast<float>(raw)) - 1.0f;
    const float rNtc  = seriesResistor / ratio;

    const float steinhart = (1.0f / tNominalK) + (log(rNtc / rNominal) / beta);
    const float tempK = 1.0f / steinhart;
    return tempK - 273.15f;
}
