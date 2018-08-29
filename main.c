// Dummy RTisan main function.  (C) Copyright 2018 Michael Lyle

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtisan.h"
#include "rtisan_led.h"
#include "systick_handler.h"

#define FPU_IRQn 9

const void *_interrupt_vectors[FPU_IRQn] __attribute((section(".interrupt_vectors"))) = {
};

const DIOInitTag_t leds[4] = {
	GPIOD_DIO(12) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOD_DIO(13) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOD_DIO(14) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOD_DIO(15) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
};

RTLock_t lock;

void *malloc(size_t size);

void idletask(void *unused)
{
	(void) unused;

	while (true);
}

void othertask(void *ctx)
{
	printf("Task start, %p\n", ctx);
	while (true) {
		if (((uintptr_t) ctx) == 128) {
			RTLEDToggle(1);
		}

		printf("%02d Pre-lock %p %lu\n", RTGetTaskId(), ctx,
				systick_cnt);
		RTLockLock(lock);
		printf("%02d Presleep %p %lu\n", RTGetTaskId(), ctx,
				systick_cnt);
		RTSleep((uintptr_t) ctx);
		printf("%02d Preunlck %p %lu\n", RTGetTaskId(), ctx,
				systick_cnt);
		RTLockUnlock(lock);
		printf("%02d Preslp2  %p %lu\n", RTGetTaskId(), ctx,
				systick_cnt);
		RTSleep((uintptr_t) ctx);
		printf("%02d Postall  %p %lu\n", RTGetTaskId(), ctx,
				systick_cnt);
	}
}

void ClockConfiguration(void); /* XXX */

int main() {
	ClockConfiguration();

	RTLEDInit(4, leds);
	RTLEDSet(0, true);
	RTLEDSet(2, true);

	printf("Initial startup, initing heap\n");
	RTHeapInit();

	printf("Initializing tasks\n");

	RTTasksInit();

	printf("Enabling systick\n");
	RTEnableSystick();

	lock = RTLockCreate();

	RTTaskCreate(1, idletask, NULL);
	RTTaskCreate(10, othertask, (void *) 3);
	RTTaskCreate(11, othertask, (void *) 7);
	RTTaskCreate(12, othertask, (void *) 128);

	printf("Waiting from \"main task\"\n");
	RTGo();
}
