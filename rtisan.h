#ifndef __RTIASN_H
#define __RTIASN_H

#include <stdint.h>

#define TASK_MAX 8

struct RTTask_s;
typedef struct RTTask_s *RTTask;

void RTTasksInit();

RTTask RTTaskCreate(void (*task)(void *), void *arg);

__attribute__((naked)) void task_switch(void);

static inline void RTYield(void)
{
	/* PENDSVSET */
	*((uint32_t volatile *)0xE000ED04) = 0x10000000;
}

void RTHeapInit();

#endif /* __RTISAN_H */
