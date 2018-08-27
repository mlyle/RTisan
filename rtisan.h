#ifndef __RTIASN_H
#define __RTIASN_H

#include <stdint.h>

#define TASK_MAX 8
#define LOCK_WAITERS_MAX 4

struct RTTask_s;
typedef struct RTTask_s *RTTask;
typedef struct RTLock_s *RTLock;

void RTTasksInit();

typedef uint8_t WakeCounter_t;
typedef uint8_t RTPrio_t;
typedef uint16_t TaskId_t;

RTTask RTTaskCreate(RTPrio_t prio, void (*task)(void *), void *arg);
WakeCounter_t RTGetWakeCount(void);
void RTWait(WakeCounter_t wakeThreshold);
void RTSleep(uint32_t ticks);
TaskId_t RTGetTaskId(void);

void RTHeapInit();

RTLock RTLockCreate(void);
void RTLockUnlock(RTLock lock);
int RTLockTryLock(RTLock lock);
int RTLockLock(RTLock lock);

#endif /* __RTISAN_H */
