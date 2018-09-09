#ifndef __RTISAN_TIMER_H
#define __RTISAN_TIMER_H

#include <rtisan_internal.h>

struct RTTimer_s;

typedef struct RTTimer_s *RTTimer_t;

RTTimer_t RTTimerCreate(TIM_TypeDef *timer);

void RTTimerSetPeriod(RTTimer_t timer, float period);
void RTTimerSetWrapFrequency(RTTimer_t timer, float wrapFrequency);
/* This is the variant we'll use */
void RTTimerSetTickFrequency(RTTimer_t timer, float frequency,
		uint32_t maxVal);

void RTTimerEnable(RTTimer_t timer, bool enabled, bool oneshot);
void RTTimerSetupCapture(RTTimer_t timer, int capIdx);

float RTTimerGetPeriod(RTTimer_t timer);
float RTTimerGetWrapFrequency(RTTimer_t timer);
float RTTimerGetTickFrequency(RTTimer_t timer);

bool RTTimerIs32Bit(RTTimer_t timer);

uint32_t RTTimerGetCaptureValue(RTTimer_t timer, int capIdx);
uint32_t RTTimerGetTimerValue(RTTimer_t timer);

#endif /* __RTISAN_TIMER_H */
