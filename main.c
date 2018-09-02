// Dummy RTisan main function.  (C) Copyright 2018 Michael Lyle

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <rtisan.h>
#include <rtisan_led.h>
#include <systick_handler.h>
#include <rtisan_stream.h>

#define FPU_IRQn 9
#if 0
const void *_interrupt_vectors[FPU_IRQn] __attribute((section(".interrupt_vectors"))) = {
};
#endif

const DIOInitTag_t leds[8] = {
	GPIOE_DIO(8) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOE_DIO(9) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOE_DIO(10) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOE_DIO(11) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOE_DIO(12) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOE_DIO(13) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOE_DIO(14) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOE_DIO(15) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
};

/* Begin interim USB stuff */
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_interface.h"
USBD_HandleTypeDef hUSBDDevice;

RTStream_t cdcStream;

void EnableInterrupt(int irqn)
{
	int idx = irqn >> 5;
	uint32_t bit = 1 << (irqn & 0x1f);

	//	NVIC->IP[irqn] = ;
	NVIC->ISER[idx] = bit;
}

static void USB_LP_IRQHandler(void)
{
	extern PCD_HandleTypeDef hpcd;
	HAL_PCD_IRQHandler(&hpcd);
}

const void *interrupt_vectors[] __attribute((section(".interrupt_vectors"))) =
{
	[USB_LP_CAN_RX0_IRQn] = USB_LP_IRQHandler,
};
/* End interim USB stuff */

RTLock_t lock;

void *malloc(size_t size);

void idletask(void *unused)
{
	(void) unused;

	while (true);
}

void othertask(void *ctx)
{
	printf("Task start, %p\r\n", ctx);
	while (true) {
		if (((uintptr_t) ctx) == 128) {
			RTLEDToggle(1);
		}

		printf("%02d Pre-lock %p %lu\r\n", RTGetTaskId(), ctx,
				systick_cnt);
		RTLockLock(lock);
		printf("%02d Presleep %p %lu\r\n", RTGetTaskId(), ctx,
				systick_cnt);
		RTSleep((uintptr_t) ctx);
		printf("%02d Preunlck %p %lu\r\n", RTGetTaskId(), ctx,
				systick_cnt);
		RTLockUnlock(lock);
		printf("%02d Preslp2  %p %lu\r\n", RTGetTaskId(), ctx,
				systick_cnt);
		RTSleep((uintptr_t) ctx);
		printf("%02d Postall  %p %lu\r\n", RTGetTaskId(), ctx,
				systick_cnt);
	}
}

void ClockConfiguration(void); /* XXX */

int main() {
	ClockConfiguration();

	RTLEDInit(8, leds);
	RTLEDSet(0, true);
	RTLEDSet(2, true);

	DIOInit(GPIOA_DIO(11) |
			INIT_DIO_ALTFUNC_OUT(DIO_PULL_NONE, DIO_DRIVE_STRONG, false, 14));
	DIOInit(GPIOA_DIO(12) |
			INIT_DIO_ALTFUNC_OUT(DIO_PULL_NONE, DIO_DRIVE_STRONG, false, 14));

	RTHeapInit();

	/* XXX begin USB chunk 2 */
	EnableInterrupt(USB_LP_CAN_RX0_IRQn);

	cdcStream = RTStreamCreate(1, 128, 128);

	/* Init Device Library */
	USBD_Init(&hUSBDDevice, &VCP_Desc, 0);

	/* Add Supported Class */
	USBD_RegisterClass(&hUSBDDevice, &USBD_CDC);

	/* Add CDC Interface Class */
	USBD_CDC_RegisterInterface(&hUSBDDevice, &USBD_CDC_fops);

	/* Start Device Process */
	USBD_Start(&hUSBDDevice);

	/* XXX end USB chunk 2 */

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
