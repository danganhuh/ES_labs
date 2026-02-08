#include <Arduino.h>

#define SELECTED_LAB 1

#if SELECTED_LAB == 1
#include "Lab1/Lab1.h"
#endif

void setup()
{
#if SELECTED_LAB == 1
    Lab1_Setup();
#endif
}

void loop()
{
#if SELECTED_LAB == 1
    Lab1_Loop();
#endif
}
