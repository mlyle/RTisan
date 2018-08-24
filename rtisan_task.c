// RTisan task switching implementation.  (C) Copyright 2018 Michael Lyle

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "rtisan.h"
#include "systick_handler.h"

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

static RTTask taskTable[TASK_MAX];
static int curTask;

void RTTasksInit()
{
}

RTTask RTTaskCreate(RTPrio_t prio, void (*task)(void *), void *arg)
{
	int i = 0;

	for(i = 0; i < TASK_MAX; i++) {
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

		*(--taskTable[i]->sp) = 0;           /* r11  */
		*(--taskTable[i]->sp) = 0;           /* r10  */
		*(--taskTable[i]->sp) = 0;           /* r9   */
		*(--taskTable[i]->sp) = 0;           /* r8   */
		*(--taskTable[i]->sp) = 0;           /* r7   */
		*(--taskTable[i]->sp) = 0;           /* r6   */
		*(--taskTable[i]->sp) = 0;           /* r5   */
		*(--taskTable[i]->sp) = 0;           /* r4   */
		*(--taskTable[i]->sp) = EXC_RET_THREAD;  /* ret vec  */

		taskTable[i]->canary = 5678;
		return taskTable[i];
        } 
	
	return NULL;
}

static inline RTTask RTTaskSelected(void)
{
	return taskTable[curTask];
}

void RTTaskSetPSP(void *psp)
{
	RTTask ourTask = RTTaskSelected();
	ourTask->sp = psp;
}

static bool BlockingInfoIsWoke(union BlockingInfo *bInfo)
{
	int8_t wakeConsider = bInfo->fields.wakeThreshold -
		bInfo->fields.wakeCount;

	return wakeConsider <= 0;
}

static bool RTTaskIsWoke(RTTask task)
{
	union BlockingInfo bInfo;

	bInfo.val32 = task->blockingInfo.val32;

	return BlockingInfoIsWoke(&bInfo);
}

static void IncrementWakes(RTTask task)
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

static RTTask RTTaskToRun(bool ticking)
{
	int bestTask = 0;
	int bestPrio = -1;

	for (int i = 0; i < TASK_MAX; i++) {
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

	curTask = bestTask;

	return RTTaskSelected();
}

void *RTStackToRun(void)
{
	static uint32_t lastSystick;
	uint32_t nowSystick = systick_cnt;
	bool ticking = false;

	if (nowSystick != lastSystick) {
		lastSystick = nowSystick;
		ticking = true;
	}

	return RTTaskToRun(ticking)->sp;
}

WakeCounter_t RTGetWakeCount(void)
{
	return RTTaskSelected()->blockingInfo.fields.wakeCount;
}

void RTWait(WakeCounter_t wakeThreshold)
{
	RTTask task = RTTaskSelected();

	uint32_t original;
	union BlockingInfo bInfo;

	do {
		original = task->blockingInfo.val32;

		bInfo.val32 = original;

		bInfo.fields.wakeThreshold = wakeThreshold;

#if 0
		if (BlockingInfoIsWoke(&bInfo)) {
			/* Don't yield: we're still eligible to run. */
			return;
		}
#endif
	} while (!__sync_bool_compare_and_swap(
					&task->blockingInfo.val32,
					original,
					bInfo.val32));

	/* PENDSVSET */
	*((uint32_t volatile *)0xE000ED04) = 0x10000000;
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

		RTTask task = RTTaskSelected();

		task->ticksCreateWakes = true;

		RTWait(RTGetWakeCount() + remain);

		task->ticksCreateWakes = false;
	};
}

__attribute__((naked)) void RTTaskSave(void *task_sp,
		void *old_lr)
{
	asm volatile(
			/* Save current stack (MSP) */
			"mov r2, sp\n\t"

			/* Install task stack (PSP), copy registers */
			"mov sp, r0\n\t"
			"push {r4-r11}\n\t"
			"push {r1}\n\t"

			/* Preserve new task stack value (PSP) */
			"mov r0, sp\n\t"

			/* Restore original (MSP) stack */
			"mov sp, r2\n\t"

			/* Really save psp, tail call opt */
			"b RTTaskSetPSP\n\t"
		    );
}

static __attribute__((naked, noreturn)) void RTTaskSwitch()
{
	/* XXX short-circuit if desired task to run is current task */
	/* XXX the way initial startup works is pessimal */
	asm volatile(
			/* Capture current task's stack */
			"mrs r0, psp\n\t" 
			/* And the current LR-- the 'exception exit'
			 * value
			 */
			"mov r1, lr\n\t"

			/* If current stack defined, save context */
       			"cmp r0, #0\n\t"
       			"it ne\n\t"
        		"blne RTTaskSave\n\t"

			/* Identify stack pointer of where to switch */
			"bl RTStackToRun\n\t"

			/* Capture current stack, save to MSP */
			"mov r1, sp\n\t"
			"msr msp, r1\n\t"

			/* Install task stackpointer */
			"mov sp, r0\n\t"

			/* Restore task registers */
			"pop {lr}\n\t"
			"pop {r4-r11}\n\t"

			/* Store new stack pointer to PSP */
			"mov r0, sp\n\t"
			"msr psp, r0\n\t"

			/* Restore original exception stack */
			"mov sp, r1\n\t"

			/* Return from exception into user task;
			 * CPU pops last few registers, etc.
			 */
			"bx lr\n\t");
}

const void *_pendsv_vector __attribute((section(".pendsv_vector"))) =
		RTTaskSwitch;
