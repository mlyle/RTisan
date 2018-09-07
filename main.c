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

#include "board.h"

RTStream_t cdcStream;

/* Begin interim USB stuff */
#ifndef __linux__
#include <rtisan_nvic.h>
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_interface.h"
USBD_HandleTypeDef hUSBDDevice;

static void USB_LP_IRQHandler(void)
{
	extern PCD_HandleTypeDef hpcd;
	HAL_PCD_IRQHandler(&hpcd);
}

const void *interrupt_vectors[] __attribute((section(".interrupt_vectors"))) =
{
	[SPI1_IRQn] = RTSPIF3IRQSPI1,
	[SPI2_IRQn] = RTSPIF3IRQSPI2,
	[SPI3_IRQn] = RTSPIF3IRQSPI3,
	[USB_LP_CAN_RX0_IRQn] = USB_LP_IRQHandler,
};

#endif

/* End interim USB stuff */

void ClockConfiguration(void); /* XXX */

#if defined(MAINFUNC)
void MAINFUNC(int argc, char *argv[]);

struct mainargs {
	int argc;
	char **argv;
};

void maintask(void *ctx)
{
	struct mainargs *args = ctx;

	MAINFUNC(args->argc, args->argv);
}
#endif

#ifdef __linux__
int main(int argc, char *argv[])
{
#else
int main(void)
{
	int argc=0;
	char *argv[] = { NULL };

	(void) argc; (void) argv;
#endif
#ifndef __linux__
	ClockConfiguration();
	RTHeapInit();
#endif

	RTLEDInit(RTNUMELEM(LEDPins), LEDPins);
	RTLEDSet(0, true);
	RTLEDSet(2, true);

	cdcStream = RTStreamCreate(1, 129, 129, true);

#ifndef __linux__
	RTSPIPeriph_t SPI1Periph = RTSPIF3Create(1, &SPI1Pins);
	assert(SPI1Periph);

	RTSPIPeriph_t SPI2Periph = RTSPIF3Create(2, &SPI2Pins);
	assert(SPI2Periph);

	RTSPIPeriph_t SPI3Periph = RTSPIF3Create(3, &SPI3Pins);
	assert(SPI3Periph);

	/* XXX begin USB chunk 2 */
	for (int i = 0; i < RTNUMELEM(USBPins); i++) {
		DIOInit(USBPins[i]);
	}

	RTNVICEnable(USB_LP_CAN_RX0_IRQn);

	/* Init Device Library */
	USBD_Init(&hUSBDDevice, &VCP_Desc, 0);

	/* Add Supported Class */
	USBD_RegisterClass(&hUSBDDevice, &USBD_CDC);

	/* Add CDC Interface Class */
	USBD_CDC_RegisterInterface(&hUSBDDevice, &USBD_CDC_fops);

	/* Start Device Process */
	USBD_Start(&hUSBDDevice);
	/* XXX end USB chunk 2 */
#endif

	RTTasksInit();

#if 0
	lock = RTLockCreate();

	RTTaskCreate(10, othertask, (void *) 3);
	RTTaskCreate(11, othertask, (void *) 7);
	RTTaskCreate(12, othertask, (void *) 1000);
#endif
//	RTTaskCreate(10, factortask, (void *) 3);

#if defined(MAINFUNC)

	struct mainargs args = { argc, argv };
	RTTaskCreate(10, maintask, &args);
#endif

	RTGo();
}
