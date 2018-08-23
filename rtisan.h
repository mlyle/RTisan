#ifndef __RTIASN_H
#define __RTIASN_H

#include <stdint.h>

#define TASK_MAX 8

struct RTTask_s;
typedef struct RTTask_s *RTTask;

void RTTasksInit();

typedef uint8_t WakeCounter_t;
typedef uint8_t RTPrio_t;

RTTask RTTaskCreate(RTPrio_t prio, void (*task)(void *), void *arg);
WakeCounter_t RTGetWakeCount(void);
void RTWait(WakeCounter_t wakeThreshold);
void RTSleep(uint32_t ticks);

void RTHeapInit();

#endif /* __RTISAN_H */
