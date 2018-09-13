// RTisan task switching implementation.  (C) Copyright 2018 Michael Lyle

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>

#include "rtisan_internal.h"

struct RTTask_s {
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;

	bool ticksCreateWakes;

	RTPrio_t priority;

	WakeCounter_t wakes, thresh;
};

struct RTLock_s {
	pthread_mutex_t mutex;
};

static RTTask_t taskTable[TASK_MAX + 1];

void RTTasksInit()
{
}

/* XXX racy */
RTTask_t RTTaskCreate(RTPrio_t prio, void (*task)(void *), void *arg)
{
	int i = 1;

	for(i = 1; i <= TASK_MAX; i++) {
		if (taskTable[i]) {
			continue;
		}

		RTTask_t newTask;

		newTask = calloc(1, sizeof(struct RTTask_s));
		newTask->priority = prio;
		pthread_mutex_init(&newTask->mutex, NULL);
		pthread_cond_init(&newTask->cond, NULL);

		taskTable[i] = newTask;

		pthread_create(&newTask->thread, NULL, (void *)task, arg);

		return taskTable[i];
	}

	return NULL;
}

inline TaskId_t RTGetTaskId(void)
{
	pthread_t self = pthread_self();

	for (int i = 1; i <= TASK_MAX; i++) {
		if (!taskTable[i]) continue;

		if (pthread_equal(self, taskTable[i]->thread)) {
			return i;
		}
	}

	return 0;
}

static inline RTTask_t RTTaskSelected(void)
{
	return taskTable[RTGetTaskId()];
}

static bool RTTaskIsWoke(RTTask_t task)
{
	WakeCounter_t wakeConsider = task->thresh - task->wakes;

	return wakeConsider <= 0;
}

static void IncrementWakes(RTTask_t task)
{
	task->wakes++;

	WakeCounter_t wakeConsider = task->thresh - task->wakes;

	if (wakeConsider == INT8_MIN) {
		task->thresh++;
	}

	pthread_cond_broadcast(&task->cond);
}

void RTWake(TaskId_t task)
{
	assert(task > 0);
	assert(task <= TASK_MAX);

	RTTask_t t = taskTable[task];

	assert(t);

	pthread_mutex_lock(&t->mutex);
	IncrementWakes(t);
	pthread_mutex_unlock(&t->mutex);
}

WakeCounter_t RTGetWakeCount(void)
{
	RTTask_t task = RTTaskSelected();

	pthread_mutex_lock(&task->mutex);
	WakeCounter_t wakes = task->wakes;
	pthread_mutex_unlock(&task->mutex);

	return wakes;
}

volatile uint32_t systick_cnt;
void RTGo(void)
{
	while (1) {
		usleep(1000);
		systick_cnt++;

		for (int i = 0; i < TASK_MAX; i++) {
			if (!taskTable[i]) continue;

			pthread_mutex_lock(&taskTable[i]->mutex);
			if (taskTable[i]->ticksCreateWakes) {
				IncrementWakes(taskTable[i]);
			}
			pthread_mutex_unlock(&taskTable[i]->mutex);
		}
	}
}

void RTWait(WakeCounter_t wakeThreshold)
{
	RTTask_t task = RTTaskSelected();

	pthread_mutex_lock(&task->mutex);

	task->thresh = wakeThreshold;

	while (!RTTaskIsWoke(task)) {
		pthread_cond_wait(&task->cond, &task->mutex);
	}

	pthread_mutex_unlock(&task->mutex);
}

void RTYield(void)
{
	RTWait(RTGetWakeCount());
}

#define MAX_SLEEP_CHUNK 127
void RTSleep(uint32_t ticks)
{
	uint32_t completion = systick_cnt + ticks;

	int32_t remain;

	while ((remain = (completion - systick_cnt)) > 0) {
		if (remain > MAX_SLEEP_CHUNK) {
			remain = MAX_SLEEP_CHUNK;
		}

		RTTask_t task = RTTaskSelected();

		task->ticksCreateWakes = true;

		RTWait(RTGetWakeCount() + remain);

		task->ticksCreateWakes = false;
	};
}

/* Locking, overall theory.
 *
 * When a higher priority task awaits a lock, it needs to sleep and let the
 * lower priority tasks run so that the lock may be released.  The lower
 * priority tasks should be raised to the priority of the highest prio
 * waiter.
 *
 * When a lower priority task desires a lock, it's sufficient to just wait.
 * No priority twiddling is necessary.
 *
 * When a task releases the lock, it's sufficient to wake all waiting tasks
 * and then schedule in descending priority order.
 */

RTLock_t RTLockCreate(void)
{
	RTLock_t lock = calloc(1, sizeof(*lock));

	if (!lock) {
		return NULL;
	}

	pthread_mutex_init(&lock->mutex, NULL);

	return lock;
}

void RTResched(void)
{
}

void RTLockUnlock(RTLock_t lock)
{
	pthread_mutex_unlock(&lock->mutex);
}

int RTLockTryLock(RTLock_t lock)
{
	pthread_mutex_trylock(&lock->mutex);
	return 0;
}

int RTLockLock(RTLock_t lock)
{
	pthread_mutex_lock(&lock->mutex);
	return 0;
}

uint32_t RTHeapFree(uint32_t *heapSize)
{
	if (heapSize) {
		*heapSize = 1048576;
	}

	return 1048576;
}

void RTSystickTrim(int amount) {
	(void) amount;
}
