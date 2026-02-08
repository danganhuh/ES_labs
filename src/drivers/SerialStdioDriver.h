#ifndef SerialStdioDriver_H
#define SerialStdioDriver_H

#include <Arduino.h>

void SerialStdioInit(unsigned long baudRate);
void SerialReadLine(char *buffer, uint8_t bufferSize);

#endif
