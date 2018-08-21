// RTisan task switching implementation.  (C) Copyright 2018 Michael Lyle

#include <stdint.h>
#include <stdlib.h>
#include "rtisan.h"

#define THUMB_PC_MASK 0xfffffffe
#define EXC_RET_THREAD 0xfffffffd

#define PROC_STACK_SIZE 800

struct RTTask_s {
	uint32_t *sp;
	uint32_t canary;
};

static RTTask task_table[TASK_MAX];
static int curTask;

void RTTasksInit()
{
}

RTTask RTTaskCreate(void (*task)(void *), void *arg)
{
	int i = 0;

	for(i = 0; i < TASK_MAX; i++) {
		if (task_table[i]) {
			continue;
		}

		task_table[i] = malloc(
			sizeof(struct RTTask_s) + PROC_STACK_SIZE);

		void *endStack = ((void *) task_table[i]) +
			sizeof(struct RTTask_s) + PROC_STACK_SIZE;

		task_table[i]->sp = endStack;

		/* setup initial stack frame */
		*(--task_table[i]->sp) = 0x21000000; /* Initial PSR */
		*(--task_table[i]->sp) = (uint32_t) task & THUMB_PC_MASK;
		*(--task_table[i]->sp) = 0;           /* lr */ 
		*(--task_table[i]->sp) = 0;           /* r12 */
		*(--task_table[i]->sp) = 0;           /* r3  */
		*(--task_table[i]->sp) = 0;           /* r2  */
		*(--task_table[i]->sp) = 0;           /* r1  */
		*(--task_table[i]->sp) = (uint32_t) arg; /* r0  */

		*(--task_table[i]->sp) = 0;           /* r11  */
		*(--task_table[i]->sp) = 0;           /* r10  */
		*(--task_table[i]->sp) = 0;           /* r9   */
		*(--task_table[i]->sp) = 0;           /* r8   */
		*(--task_table[i]->sp) = 0;           /* r7   */
		*(--task_table[i]->sp) = 0;           /* r6   */
		*(--task_table[i]->sp) = 0;           /* r5   */
		*(--task_table[i]->sp) = 0;           /* r4   */
		*(--task_table[i]->sp) = EXC_RET_THREAD;  /* ret vec  */

		task_table[i]->canary = 5678;
		return task_table[i];
        } 
	
	return NULL;
}

static inline RTTask RTTaskSelected(void)
{
	return task_table[curTask];
}

void RTTaskSetPSP(void *psp)
{
	RTTask ourTask = RTTaskSelected();
	ourTask->sp = psp;
}

static RTTask RTTaskToRun(void)
{
	curTask++;

	if ((curTask >= TASK_MAX) || (!task_table[curTask])) {
		curTask = 0;
	}

	return RTTaskSelected();
}

void *RTStackToRun(void)
{
	return RTTaskToRun()->sp;
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
