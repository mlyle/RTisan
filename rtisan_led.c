#include "rtisan_led.h"

static const DIOInitTag_t *rtLeds;
static int rtLedCount;

void RTLEDInit(int numLed, const DIOInitTag_t *leds)
{
	rtLeds = leds;
	rtLedCount = numLed;

	for (int i = 0; i < numLed; i++) {
		DIOInit(rtLeds[i]);
	}
}

void RTLEDSet(int ledNum, bool lit)
{
	if (ledNum >= rtLedCount) {
		return;
	}

	if (ledNum < 0) {
		return;
	}

	DIOInitTag_t led = rtLeds[ledNum];

	/* Use initial value to infer whether it's active high */
	if (GET_DIO_INITIAL(led)) {
		DIOWrite(led, !lit);
	} else {
		DIOWrite(led, lit);
	}
}

void RTLEDToggle(int ledNum)
{
	if (ledNum >= rtLedCount) {
		return;
	}

	if (ledNum < 0) {
		return;
	}

	DIOInitTag_t led = rtLeds[ledNum];

	DIOToggle(led);
}
