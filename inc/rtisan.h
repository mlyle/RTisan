#ifndef __RTIASN_H
#define __RTIASN_H

#include <stdint.h>

#define TASK_MAX 8
#define LOCK_WAITERS_MAX 4

struct RTTask_s;
typedef struct RTTask_s *RTTask_t;
typedef struct RTLock_s *RTLock_t;

void RTTasksInit();

typedef uint8_t WakeCounter_t;
typedef uint8_t RTPrio_t;
typedef uint16_t TaskId_t;

RTTask_t RTTaskCreate(RTPrio_t prio, void (*task)(void *), void *arg);
WakeCounter_t RTGetWakeCount(void);
void RTWait(WakeCounter_t wakeThreshold);
void RTGo(void);
void RTSleep(uint32_t ticks);
void RTWake(TaskId_t task);
TaskId_t RTGetTaskId(void);
uint32_t RTHeapFree(uint32_t *heapSize);

#define RT_NAME "rTisan RTOS by Michael Lyle"
#define RT_VERSIONSTR "0.10A"

#define RTNUMELEM(x) (sizeof(x) / sizeof(*(x)))
#define RTUNUSED __attribute__ ((unused))

void RTHeapInit();

RTLock_t RTLockCreate(void);
void RTLockUnlock(RTLock_t lock);
int RTLockTryLock(RTLock_t lock);
int RTLockLock(RTLock_t lock);

#endif /* __RTISAN_H */
