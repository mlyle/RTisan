#ifndef __RTISAN_LED_H
#define __RTISAN_LED_H

#include <rtisan_dio.h>

void RTLEDInit(int numLed, const DIOInitTag_t *leds);
void RTLEDSet(int ledNum, bool lit);
void RTLEDToggle(int ledNum);

#endif
