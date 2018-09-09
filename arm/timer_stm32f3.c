#include <rtisan_timer.h>

#include <stdlib.h>

struct RTTimer_s {
#define RTTIMER_MAGIC 0x724D4954	/* 'TIMr' */
	int magic;

	TIM_TypeDef *timer;

	uint32_t refClk;

	bool is32Bit;
};

RTTimer_t RTTimerCreate(TIM_TypeDef *timer)
{
	RTTimer_t ret = calloc(1, sizeof(*ret));
	
	ret->magic = RTTIMER_MAGIC;
	ret->timer = timer;

	/* XXX 32 bitness */
	ret->is32Bit = true;
	/* XXX refclock */
	ret->refClk = 72000000;

	return ret;
}

bool RTTimerIs32Bit(RTTimer_t timer)
{
	assert(timer->magic == RTTIMER_MAGIC);
	return timer->is32Bit;
}

#if 0
/* XXX missing implementations */
void RTTimerSetPeriod(RTTimer_t timer, float period);
void RTTimerSetWrapFrequency(RTTimer_t timer, float wrapFrequency);

float RTTimerGetPeriod(RTTimer_t timer);
float RTTimerGetWrapFrequency(RTTimer_t timer);
#endif

void RTTimerSetTickFrequency(RTTimer_t timer, float frequency,
		uint32_t maxVal)
{
	assert(timer->magic == RTTIMER_MAGIC);

	timer->timer->PSC = (timer->refClk / frequency) - 0.5f;

	timer->timer->ARR = maxVal;

	/* Trigger an update event so the new prescaler value is
	 * applied.
	 */
	timer->timer->EGR = TIM_EGR_UG;
}

void RTTimerEnable(RTTimer_t timer, bool enabled, bool oneshot)
{
	assert(timer->magic == RTTIMER_MAGIC);

	timer->timer->SMCR = 0;	/* no slave mode */

	uint16_t tmp = 0;

	if (enabled) {
		tmp |= TIM_CR1_CEN;
	}

	if (oneshot) {
		tmp |= TIM_CR1_OPM;
	}

	timer->timer->CR1 = tmp;
}

float RTTimerGetTickFrequency(RTTimer_t timer)
{
	assert(timer->magic == RTTIMER_MAGIC);

	return (timer->refClk / (timer->timer->PSC + 1.0f));
}

void RTTimerSetCaptureMode(RTTimer_t timer, int capIdx, bool capture)
{
	assert(timer->magic == RTTIMER_MAGIC);

	/* XXX impl */
}

uint32_t RTTimerGetCaptureValue(RTTimer_t timer, int capIdx)
{
	assert(timer->magic == RTTIMER_MAGIC);

	switch (capIdx) {
		case 1:
			return timer->timer->CCR1;
		case 2:
			return timer->timer->CCR2;
		case 3:
			return timer->timer->CCR3;
		case 4:
			return timer->timer->CCR4;
		case 5:
			return timer->timer->CCR5;
		case 6:
			return timer->timer->CCR6;
		default:
			abort();
	}
}

uint32_t RTTimerGetTimerValue(RTTimer_t timer)
{
	assert(timer->magic == RTTIMER_MAGIC);

	return timer->timer->CNT;
}
