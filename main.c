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

RTStream_t cdcStreams[2];

/* Begin interim USB stuff */
#ifndef __linux__
#include <rtisan_nvic.h>
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_interface.h"
USBD_HandleTypeDef hUSBDDevice;

static void USB_LP_IRQHandler(void) __attribute__((interrupt));

static void USB_LP_IRQHandler(void)
{
	extern PCD_HandleTypeDef hpcd;
	HAL_PCD_IRQHandler(&hpcd);
}

const void *interrupt_vectors[] __attribute((section(".interrupt_vectors"))) =
{
	[DMA1_Channel2_IRQn] = RTSPIDMAF3IRQDMA1Chan2,
	[DMA1_Channel4_IRQn] = RTSPIDMAF3IRQDMA1Chan4,
	[DMA2_Channel1_IRQn] = RTSPIDMAF3IRQDMA2Chan1,
	[USB_LP_CAN_RX0_IRQn] = USB_LP_IRQHandler,
};
#else
#include <rtisan_tcp.h>
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

	RTTasksInit();

	cdcStreams[0] = RTStreamCreate(1, 193, 193, true);
	cdcStreams[1] = RTStreamCreate(1, 257, 257, true);

#ifndef __linux__
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
#else
	RTTCPAttach(cdcStreams[1], 3133);
#endif

#if defined(MAINFUNC)
	struct mainargs args = { argc, argv };
	RTTaskCreate(10, maintask, &args);
#endif

	RTGo();
}
