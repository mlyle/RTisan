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

void RTTimerSetCaptureMode(RTTimer_t timer, int capIdx, bool capture,
		bool posEdge)
{
	assert(timer->magic == RTTIMER_MAGIC);

	/* CCMR* fields */
	uint16_t ccs = 0x1; /* Capture, normal "mapping" */
	uint16_t icf = 0x5; /* Input capture filter fdts/2 N=8 */
	uint16_t psc = 0;   /* No input capture prescaling */

	/* CCER fields */
	uint16_t cce = 1;               /* This is an output */
	uint16_t ccp = posEdge ? 0 : 1; /* Pick the edge */
	uint16_t ccnp = 0;              /* Only be sensitive to one edge */

	switch (capIdx) {
		case 1:
			timer->timer->CCMR1 =
				(timer->timer->CCMR1 & (~
					(TIM_CCMR1_CC1S_Msk |
					 TIM_CCMR1_IC1F_Msk |
					 TIM_CCMR1_IC1PSC_Msk))) |
				(ccs << TIM_CCMR1_CC1S_Pos) |
				(icf << TIM_CCMR1_IC1F_Pos) |
				(psc << TIM_CCMR1_IC1PSC_Pos);
			timer->timer->CCER =
				(timer->timer->CCER & (~
					(TIM_CCER_CC1E_Msk |
					 TIM_CCER_CC1P_Msk |
					 TIM_CCER_CC1NP_Msk))) |
				(cce << TIM_CCER_CC1E_Pos) |
				(ccp << TIM_CCER_CC1P_Pos) |
				(ccnp << TIM_CCER_CC1NP_Pos);
			break;
		case 2:
			timer->timer->CCMR1 =
				(timer->timer->CCMR1 & (~
					(TIM_CCMR1_CC2S_Msk |
					 TIM_CCMR1_IC2F_Msk |
					 TIM_CCMR1_IC2PSC_Msk))) |
				(ccs << TIM_CCMR1_CC2S_Pos) |
				(icf << TIM_CCMR1_IC2F_Pos) |
				(psc << TIM_CCMR1_IC2PSC_Pos);
			timer->timer->CCER =
				(timer->timer->CCER & (~
					(TIM_CCER_CC2E_Msk |
					 TIM_CCER_CC2P_Msk |
					 TIM_CCER_CC2NP_Msk))) |
				(cce << TIM_CCER_CC2E_Pos) |
				(ccp << TIM_CCER_CC2P_Pos) |
				(ccnp << TIM_CCER_CC2NP_Pos);
			break;
		case 3:
			timer->timer->CCMR2 =
				(timer->timer->CCMR2 & (~
					(TIM_CCMR2_CC3S_Msk |
					 TIM_CCMR2_IC3F_Msk |
					 TIM_CCMR2_IC3PSC_Msk))) |
				(ccs << TIM_CCMR2_CC3S_Pos) |
				(icf << TIM_CCMR2_IC3F_Pos) |
				(psc << TIM_CCMR2_IC3PSC_Pos);
			timer->timer->CCER =
				(timer->timer->CCER & (~
					(TIM_CCER_CC3E_Msk |
					 TIM_CCER_CC3P_Msk |
					 TIM_CCER_CC3NP_Msk))) |
				(cce << TIM_CCER_CC3E_Pos) |
				(ccp << TIM_CCER_CC3P_Pos) |
				(ccnp << TIM_CCER_CC3NP_Pos);
			break;
		case 4:
			timer->timer->CCMR2 =
				(timer->timer->CCMR2 & (~
					(TIM_CCMR2_CC4S_Msk |
					 TIM_CCMR2_IC4F_Msk |
					 TIM_CCMR2_IC4PSC_Msk))) |
				(ccs << TIM_CCMR2_CC4S_Pos) |
				(icf << TIM_CCMR2_IC4F_Pos) |
				(psc << TIM_CCMR2_IC4PSC_Pos);
			timer->timer->CCER =
				(timer->timer->CCER & (~
					(TIM_CCER_CC4E_Msk |
					 TIM_CCER_CC4P_Msk |
					 TIM_CCER_CC4NP_Msk))) |
				(cce << TIM_CCER_CC4E_Pos) |
				(ccp << TIM_CCER_CC4P_Pos) |
				(ccnp << TIM_CCER_CC4NP_Pos);
			break;
		default:
			abort();
	}

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
