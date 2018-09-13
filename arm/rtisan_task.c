// RTisan task switching implementation.  (C) Copyright 2018 Michael Lyle

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "rtisan_internal.h"

#define THUMB_PC_MASK 0xfffffffe
#define EXC_RET_THREAD 0xffffffed

/* XXX Must be multiple of 8 */
#define PROC_STACK_SIZE 1600

struct _reent default_reent;
struct _reent *_impure_ptr = &default_reent;

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

	struct _reent reent;

	/* Ensures doubleword align of stack */
	uint64_t canary;
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

static struct RTTask_s * volatile taskTable[TASK_MAX + 1];
static TaskId_t curTask;

static void RTTaskCreateImpl(RTTask_t taskRec, void (*task)(void *), void *arg);

static volatile uint32_t idleCounter;

void RTTasksInit()
{
}

static inline int GetTaskAllocSize()
{
	return sizeof(struct RTTask_s) + PROC_STACK_SIZE;
}

RTTask_t RTTaskCreate(RTPrio_t prio, void (*func)(void *), void *arg)
{
	int i = 1;

	for(i = 1; i <= TASK_MAX; i++) {
		if (taskTable[i]) {
			continue;
		}

		RTTask_t task = malloc(GetTaskAllocSize());

		task = malloc(GetTaskAllocSize());
		task->priority = prio;
		task->blockingInfo.val32 = 0;
		task->ticksCreateWakes = false;

		RTTaskCreateImpl(task, func, arg);

		__sync_synchronize();

		taskTable[i] = task;

		return task;
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

		int8_t wakeConsider = bInfo.fields.wakeThreshold -
			bInfo.fields.wakeCount;

		if (wakeConsider == INT8_MIN) {
			/* This means we're wrapping around, from being
			 * fully woken, to "asleep".
			 *
			 * A busy task that keeps running a tight loop
			 * and getting wakes can end up ineligible to
			 * run in this way.  So we need to handle
			 * this as a special case.
			 */

			bInfo.fields.wakeThreshold++;
		}
	} while (!__sync_bool_compare_and_swap(
					&task->blockingInfo.val32,
					original,
					bInfo.val32));
}

void RTWake(TaskId_t task)
{
	assert(task > 0);
	assert(task <= TASK_MAX);

	RTTask_t t = taskTable[task];

	assert(t);

	IncrementWakes(t);
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

	_impure_ptr = &ourTask->reent;

	return ourTask->sp;
}

WakeCounter_t RTGetWakeCount(void)
{
	return RTTaskSelected()->blockingInfo.fields.wakeCount;
}

static void RTIdleTask(void *unused)
{
	(void) unused;

	while (true) {
		idleCounter++;
	}
}

void RTGo(void)
{
	RTTaskCreate(0, RTIdleTask, NULL);
	RTEnableSystick();

#if 0
	SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
#endif
	/* Next systick will change task. */

	while (1);
}

static inline void Resched(void)
{
	/* Ensure PendSV is set */
	SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}

void RTResched(void)
{
	Resched();
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

	if (!BlockingInfoIsWoke(&bInfo)) {
		Resched();
	}
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

static void RTTaskCreateImpl(RTTask_t taskRec, void (*task)(void *), void *arg)
{
	memset(&taskRec->reent, 0, sizeof(taskRec->reent));

	void *endStack = ((void *) taskRec) +
		GetTaskAllocSize();

	/* XXX stack alignment  confirm !! */

	taskRec->sp = endStack;
	/* setup initial stack frame */

	/* XXX */
	--taskRec->sp;

#ifdef HAVE_FPU
	*(--taskRec->sp) = FPU->FPDSCR; /* FPSCR: default values */
	*(--taskRec->sp) = 0;           /* s15 */
	*(--taskRec->sp) = 0;           /* s14 */
	*(--taskRec->sp) = 0;           /* s13 */
	*(--taskRec->sp) = 0;           /* s12 */
	*(--taskRec->sp) = 0;           /* s11 */
	*(--taskRec->sp) = 0;           /* s10 */
	*(--taskRec->sp) = 0;           /* s9 */
	*(--taskRec->sp) = 0;           /* s8 */
	*(--taskRec->sp) = 0;           /* s7 */
	*(--taskRec->sp) = 0;           /* s6 */
	*(--taskRec->sp) = 0;           /* s5 */
	*(--taskRec->sp) = 0;           /* s4 */
	*(--taskRec->sp) = 0;           /* s3 */
	*(--taskRec->sp) = 0;           /* s2 */
	*(--taskRec->sp) = 0;           /* s1 */
	*(--taskRec->sp) = 0x12341234;  /* s0 */
#endif

	*(--taskRec->sp) = 0x21000000; /* Initial PSR */
	*(--taskRec->sp) = (uint32_t) task & THUMB_PC_MASK;
	*(--taskRec->sp) = 0;           /* lr */
	*(--taskRec->sp) = 0;           /* r12 */
	*(--taskRec->sp) = 0;           /* r3  */
	*(--taskRec->sp) = 0;           /* r2  */
	*(--taskRec->sp) = 0;           /* r1  */
	*(--taskRec->sp) = (uint32_t) arg; /* r0  */

	*(--taskRec->sp) = EXC_RET_THREAD;  /* ret vec  */
	*(--taskRec->sp) = 0;           /* r11  */
	*(--taskRec->sp) = 0;           /* r10  */
	*(--taskRec->sp) = 0;           /* r9   */
	*(--taskRec->sp) = 0;           /* r8   */
	*(--taskRec->sp) = 0;           /* r7   */
	*(--taskRec->sp) = 0;           /* r6   */
	*(--taskRec->sp) = 0;           /* r5   */
	*(--taskRec->sp) = 0;           /* r4   */

#ifdef HAVE_FPU
	*(--taskRec->sp) = 0;           /* s31 */
	*(--taskRec->sp) = 0;           /* s30 */
	*(--taskRec->sp) = 0;           /* s29 */
	*(--taskRec->sp) = 0;           /* s28 */
	*(--taskRec->sp) = 0;           /* s27 */
	*(--taskRec->sp) = 0;           /* s26 */
	*(--taskRec->sp) = 0;           /* s25 */
	*(--taskRec->sp) = 0;           /* s24 */
	*(--taskRec->sp) = 0;           /* s23 */
	*(--taskRec->sp) = 0;           /* s22 */
	*(--taskRec->sp) = 0;           /* s21 */
	*(--taskRec->sp) = 0;           /* s20 */
	*(--taskRec->sp) = 0;           /* s19 */
	*(--taskRec->sp) = 0;           /* s18 */
	*(--taskRec->sp) = 0;           /* s17 */
	*(--taskRec->sp) = 0;           /* s16 */
#endif

	taskRec->canary = 5678;
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

#ifdef HAVE_FPU
			"vpush {s16-s31}\n\t"
#endif

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
#ifdef HAVE_FPU
			"vpop {s16-s31}\n\t"
#endif
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
			if (task) {
				RTWake(task);

				wokeSomeone = true;
			}
		} else {
			lock->waiters[i] = 0;
		}
	}

	if (wokeSomeone) {
		RTResched();	/* Let the scheduler figure out if they
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
