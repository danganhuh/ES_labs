#include "UltrasonicDriver.h"

UltrasonicDriver::UltrasonicDriver(uint8_t trig, uint8_t echo, unsigned long timeoutUs)
    : trigPin(trig),
      echoPin(echo),
      echoTimeoutUs(timeoutUs)
{
}

void UltrasonicDriver::Init()
{
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    digitalWrite(trigPin, LOW);
}

bool UltrasonicDriver::ReadDistanceCm(float* outDistanceCm)
{
    if (outDistanceCm == NULL)
    {
        return false;
    }

    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    const unsigned long durationUs = pulseIn(echoPin, HIGH, echoTimeoutUs);
    if (durationUs == 0UL)
    {
        return false;
    }

    const float distanceCm = (static_cast<float>(durationUs) * 0.0343f) * 0.5f;
    if (distanceCm < 2.0f || distanceCm > 400.0f)
    {
        return false;
    }

    *outDistanceCm = distanceCm;
    return true;
}
