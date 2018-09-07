// Dummy RTisan main function.  (C) Copyright 2018 Michael Lyle

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <rtisan.h>
#include <rtisan_nvic.h>
#include <rtisan_led.h>
#include <systick_handler.h>
#include <rtisan_stream.h>

#ifndef __linux__
#include <spi_stm32f3.h>

/* XXX dac clear, dac load, nail to values ... */

static const struct RTSPIF3Pins_s SPI1Pins = {
	.miso = GPIOA_DIO(6)
		| INIT_DIO_ALTFUNC_IN(DIO_PULL_UP, 5),
	.mosi = GPIOA_DIO(7)
		| INIT_DIO_ALTFUNC_OUT(DIO_PULL_UP, DIO_DRIVE_MEDIUM, false, 5),
	.sck = GPIOA_DIO(5)
		| INIT_DIO_ALTFUNC_OUT(DIO_PULL_UP, DIO_DRIVE_MEDIUM, false, 5),
	.slaves = {
		GPIOA_DIO(4)
			| INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
		DIO_NULL
	},
};

static const struct RTSPIF3Pins_s SPI2Pins = {
	.miso = GPIOB_DIO(14)
		| INIT_DIO_ALTFUNC_IN(DIO_PULL_UP, 5),
	.mosi = DIO_NULL,
	.sck = GPIOB_DIO(13)
		| INIT_DIO_ALTFUNC_OUT(DIO_PULL_UP, DIO_DRIVE_MEDIUM, false, 5),
	.slaves = {
		DIO_NULL
	},
};

static const struct RTSPIF3Pins_s SPI3Pins = {
	.miso = GPIOB_DIO(4)
		| INIT_DIO_ALTFUNC_IN(DIO_PULL_UP, 6),
	.mosi = DIO_NULL,
	.sck = GPIOB_DIO(3)
		| INIT_DIO_ALTFUNC_OUT(DIO_PULL_UP, DIO_DRIVE_MEDIUM, false, 6),
	.slaves = {
		DIO_NULL
	},
};
#endif

static const DIOInitTag_t leds[8] = {
#ifndef __linux__
	GPIOE_DIO(8) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOE_DIO(9) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOE_DIO(10) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOE_DIO(11) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOE_DIO(12) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOE_DIO(13) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOE_DIO(14) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
	GPIOE_DIO(15) | INIT_DIO_OUTPUT(DIO_DRIVE_MEDIUM, false, false),
#endif
};

RTStream_t cdcStream;

/* Begin interim USB stuff */
#ifndef __linux__
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

	RTLEDInit(8, leds);
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
	DIOInit(GPIOA_DIO(11) |
			INIT_DIO_ALTFUNC_OUT(DIO_PULL_NONE, DIO_DRIVE_STRONG, false, 14));
	DIOInit(GPIOA_DIO(12) |
			INIT_DIO_ALTFUNC_OUT(DIO_PULL_NONE, DIO_DRIVE_STRONG, false, 14));

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
