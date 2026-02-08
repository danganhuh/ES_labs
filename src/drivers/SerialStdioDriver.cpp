#include "SerialStdioDriver.h"
#include <stdio.h>

static int SerialPutChar(char c, FILE *stream)
{
    Serial.write(c);
    return 0;
}

static FILE SerialStdOut;

void SerialStdioInit(unsigned long baudRate)
{
    Serial.begin(baudRate);
    while (!Serial) {}

    fdev_setup_stream(&SerialStdOut, SerialPutChar, NULL, _FDEV_SETUP_WRITE);
    stdout = &SerialStdOut;
}

void SerialReadLine(char *buffer, uint8_t bufferSize)
{
    uint8_t index = 0;

    while (1)
    {
        if (Serial.available())
        {
            char receivedChar = Serial.read();

            if (receivedChar == '\r' || receivedChar == '\n')
            {
                buffer[index] = '\0';
                return;
            }

            if (index < bufferSize - 1)
            {
                buffer[index++] = receivedChar;
            }
        }
    }
}
