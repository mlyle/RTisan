// RTisan task switching implementation.  (C) Copyright 2018 Michael Lyle

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "rtisan.h"
#include "systick_handler.h"

#include <stm32f303xc.h>
#include <core_cm4.h>

#define THUMB_PC_MASK 0xfffffffe
#define EXC_RET_THREAD 0xfffffffd

#define PROC_STACK_SIZE 800

union BlockingInfo {
	struct {
		uint8_t wakeThreshold;
		uint8_t wakeCount;
	} fields;

	uint32_t val32;
};

struct RTTask_s {
	uint32_t *sp;

	volatile union BlockingInfo blockingInfo;

	bool ticksCreateWakes;

	RTPrio_t priority;

	uint32_t canary;
};

union LockInfo {
	struct {
		TaskId_t holder;
	} fields;

	uint32_t val32;
};

struct RTLock_s {
	volatile union LockInfo lockInfo;
	volatile uint16_t waiters[LOCK_WAITERS_MAX];
};

static RTTask_t taskTable[TASK_MAX + 1];
static TaskId_t curTask;

void RTTasksInit()
{
}

RTTask_t RTTaskCreate(RTPrio_t prio, void (*task)(void *), void *arg)
{
	int i = 1;

	for(i = 1; i <= TASK_MAX; i++) {
		if (taskTable[i]) {
			continue;
		}

		taskTable[i] = malloc(
			sizeof(struct RTTask_s) + PROC_STACK_SIZE);

		void *endStack = ((void *) taskTable[i]) +
			sizeof(struct RTTask_s) + PROC_STACK_SIZE;

		taskTable[i]->sp = endStack;
		taskTable[i]->priority = prio;
		taskTable[i]->blockingInfo.val32 = 0;
		taskTable[i]->ticksCreateWakes = false;

		/* setup initial stack frame */
		*(--taskTable[i]->sp) = 0x21000000; /* Initial PSR */
		*(--taskTable[i]->sp) = (uint32_t) task & THUMB_PC_MASK;
		*(--taskTable[i]->sp) = 0;           /* lr */
		*(--taskTable[i]->sp) = 0;           /* r12 */
		*(--taskTable[i]->sp) = 0;           /* r3  */
		*(--taskTable[i]->sp) = 0;           /* r2  */
		*(--taskTable[i]->sp) = 0;           /* r1  */
		*(--taskTable[i]->sp) = (uint32_t) arg; /* r0  */

		*(--taskTable[i]->sp) = EXC_RET_THREAD;  /* ret vec  */
		*(--taskTable[i]->sp) = 0;           /* r11  */
		*(--taskTable[i]->sp) = 0;           /* r10  */
		*(--taskTable[i]->sp) = 0;           /* r9   */
		*(--taskTable[i]->sp) = 0;           /* r8   */
		*(--taskTable[i]->sp) = 0;           /* r7   */
		*(--taskTable[i]->sp) = 0;           /* r6   */
		*(--taskTable[i]->sp) = 0;           /* r5   */
		*(--taskTable[i]->sp) = 0;           /* r4   */

		taskTable[i]->canary = 5678;
		return taskTable[i];
        }

	return NULL;
}

inline TaskId_t RTGetTaskId(void)
{
	return curTask;
}

static inline RTTask_t RTTaskSelected(void)
{
	return taskTable[curTask];
}

static bool BlockingInfoIsWoke(union BlockingInfo *bInfo)
{
	int8_t wakeConsider = bInfo->fields.wakeThreshold -
		bInfo->fields.wakeCount;

	return wakeConsider <= 0;
}

static bool RTTaskIsWoke(RTTask_t task)
{
	union BlockingInfo bInfo;

	bInfo.val32 = task->blockingInfo.val32;

	return BlockingInfoIsWoke(&bInfo);
}

static void IncrementWakes(RTTask_t task)
{
	uint32_t original;
	union BlockingInfo bInfo;

	do {
		original = task->blockingInfo.val32;

		bInfo.val32 = original;

		bInfo.fields.wakeCount++;
	} while (!__sync_bool_compare_and_swap(
					&task->blockingInfo.val32,
					original,
					bInfo.val32));
}

TaskId_t RTTaskToRun(void)
{
	static uint32_t lastSystick;
	uint32_t nowSystick = systick_cnt;
	bool ticking = false;

	if (nowSystick != lastSystick) {
		lastSystick = nowSystick;
		ticking = true;
	}

	TaskId_t bestTask = 0;
	int bestPrio = -1;

	for (int i = 1; i <= TASK_MAX; i++) {
		if (!taskTable[i]) {
			continue;
		}

		if (ticking && taskTable[i]->ticksCreateWakes) {
			IncrementWakes(taskTable[i]);
		}

		if (taskTable[i]->priority < bestPrio) {
			continue;
		}

		/* Prefer to keep current task running. */
		if ((curTask != i) &&
				(taskTable[i]->priority == bestPrio)) {
			continue;
		}

		if (RTTaskIsWoke(taskTable[i])) {
			bestTask = i;
			bestPrio = taskTable[i]->priority;
		}
	}

	if (bestTask == curTask) {
		return 0;
	}

	return bestTask;
}

/* Saves PSP into current task.  Selects newTask and returns its SP.
 */
void *RTTaskStackSave(TaskId_t newTask, void *psp)
{
	RTTask_t ourTask = RTTaskSelected();

	if (psp) {
		ourTask->sp = psp;
	}

	curTask = newTask;

	ourTask = RTTaskSelected();

	return ourTask->sp;
}

WakeCounter_t RTGetWakeCount(void)
{
	return RTTaskSelected()->blockingInfo.fields.wakeCount;
}

void RTWait(WakeCounter_t wakeThreshold)
{
	RTTask_t task = RTTaskSelected();

	uint32_t original;
	union BlockingInfo bInfo;

	do {
		original = task->blockingInfo.val32;

		bInfo.val32 = original;

		bInfo.fields.wakeThreshold = wakeThreshold;
	} while ((original != bInfo.val32) &&
			(!__sync_bool_compare_and_swap(
					&task->blockingInfo.val32,
					original,
					bInfo.val32)));

	/* Ensure PendSV is set */
	SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
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

static __attribute__((naked, noreturn)) void RTTaskSwitch()
{
	/* XXX the way initial startup works is pessimal */
	asm volatile(
			// Identify where to switch
			"push {lr}\n\t"
			"bl RTTaskToRun\n\t"
			"pop {lr}\n\t"

			// If it's the same as where we are now, escape
			"cmp r0, #0\n\t"
			"it eq\n\t"
			"beq alldone\n\t"

			// Capture the stack pointer from current user task
			"mrs r1, psp\n\t"

			// If it's undefined, e.g. startup, skip saving
			// context.
			// XXX the way initial startup works is pessimal.
			"cmp r1, #0\n\t"
			"it eq\n\t"
			"beq savedone\n\t"

			// Set aside current exception stack
			"mov r3, sp\n\t"

			// Install current task stack, save registers that
			// the exception entry didn't save.
			"mov sp, r1\n\t"
			"push {r4-r11, lr}\n\t"

			// Capture this stack value in R1
			"mov r1, sp\n\t"

			// Restore the exception stack into place
			"mov sp, r3\n\t"

		"savedone:\n\t"
			// void *RTTaskStackSave(TaskId_t newTask, void *psp)
			// r0 = newTask   r1 = old task stack val
			// r0 comes from ret of RTTaskToRun and is untouched
			// r1 is either: 0 if we're in the startup case, or
			// the new user stack value after saving all the
			// registers above.
			//
			// This saves the old stack (if applicable),
			// switches to the new task identifier, and returns
			// the user task stack pointer.
			"bl RTTaskStackSave\n\t"

			// Store current stack pointer in R1 & MSP
			"mov r1, sp\n\t"
			"msr msp, r1\n\t"

			// Install the stack pointer of the switched-to task
			"mov sp, r0\n\t"

			// Restore switched-to task registers
			"pop {r4-r11, lr}\n\t"

			// Install current task stack value into PSP
			"mov r0, sp\n\t"
			"msr psp, r0\n\t"

			// Restore the exception stack.
			"mov sp, r1\n\t"

		"alldone:\n\t"
			// Return from exception into user task;
			// CPU restores task stack,
			// pops r0-r3, r12, lr, pc
			"bx lr\n\t");
}

const void *_pendsv_vector __attribute((section(".pendsv_vector"))) =
		RTTaskSwitch;

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

	return lock;
}

void RTLockUnlock(RTLock_t lock)
{
	TaskId_t ourTask = RTGetTaskId();

	lock->lockInfo.val32 = 0;

	bool wokeSomeone;

	/* Remove ourselves from waiters, wake all others */
	for (int i = 0; i < LOCK_WAITERS_MAX; i++) {
		TaskId_t task = lock->waiters[i];

		if (task != ourTask) {
			/* Can result in extra wakes */
			IncrementWakes(taskTable[task]);

			wokeSomeone = true;
		} else {
			lock->waiters[i] = 0;
		}
	}

	if (wokeSomeone) {
		RTYield();	/* Let the scheduler figure out if they
				 * should run. */
	}
}

int RTLockTryLock(RTLock_t lock)
{
	union LockInfo lockInfo = {};

	lockInfo.fields.holder = RTGetTaskId();

	if (__sync_bool_compare_and_swap(
					&lock->lockInfo.val32,
					0,
					lockInfo.val32)) {
		return 0;
	} else {
		return -1;
	}
}

int RTLockLock(RTLock_t lock)
{
	TaskId_t ourTask = RTGetTaskId();

	if (RTLockTryLock(lock)) {
		/* Alas, not able to get lock.  So let's add ourselves
		 * to set of waiters.
		 *
		 * First, we add ourselves to the wake list.
		 *
		 * Second, we get our current wake count, before checking
		 * if we can get the lock.
		 *
		 * Third, if we couldn't, we wait for that wake count
		 * to increment.
		 */

		bool added = false;

		for (int i = 0; i < LOCK_WAITERS_MAX; i++) {
			if (__sync_bool_compare_and_swap(&lock->waiters[i],
						0,
						ourTask)) {
				added = true;
				break;
			}
		}

		if (!added) {
			/* Couldn't lock.  Waiter list full. */
			return -1;
		}

		WakeCounter_t count = RTGetWakeCount();

		while (RTLockTryLock(lock)) {
			/* XXX Prio Inh */
			RTWait(++count);

			count = RTGetWakeCount();
		}

		/* We'll be removed from waiters when we unlock */
	}

	return 0;
}
