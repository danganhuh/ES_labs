#include <Arduino.h>
#include <string.h>
#include <stdio.h>

#include "../led/LedDriver.h"
#include "../drivers/SerialStdioDriver.h"
#include "Lab1.h"

#define LedPin 13
#define ButtonPin 2
#define BufferSize 64

static LedDriver StatusLed(LedPin);

static void ProcessCommand(char *command)
{
    if (strcmp(command, "led on") == 0)
    {
        StatusLed.On();
        printf("LED turned ON\r\n");
    }
    else if (strcmp(command, "led off") == 0)
    {
        StatusLed.Off();
        printf("LED turned OFF\r\n");
    }
    else
    {
        printf("Invalid command. Use: led on | led off\r\n");
    }
}

void Lab1_Setup()
{
    SerialStdioInit(9600);

    StatusLed.Init();

    printf("=== Lab 1.1 - Serial STDIO LED Control ===\r\n");
    printf("Available commands:\r\n");
    printf("led on\r\n");
    printf("led off\r\n\r\n");
}

void Lab1_Loop()
{
    char commandBuffer[BufferSize];

    SerialReadLine(commandBuffer, BufferSize);
    ProcessCommand(commandBuffer);
}
