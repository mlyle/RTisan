// Dummy RTisan main function.  (C) Copyright 2018 Michael Lyle

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtisan.h"

#define FPU_IRQn 9

const void *_interrupt_vectors[FPU_IRQn] __attribute((section(".interrupt_vectors"))) = {
};

#define UART0_R ((volatile uint32_t *) 0x4000C000)

int myputchar(int c) 
{
        *UART0_R = c;

	return 0;
}

int doPrintf(const char *fmt, ...)
{
	va_list ap;

	/* Determine required size */

	va_start(ap, fmt);

	int size = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	if (size < 0)
		return size;

	if (size > 256)
		return -1;

	size++;             /* For '\0' */

	char buf[size];

	va_start(ap, fmt);
	size = vsnprintf(buf, size, fmt, ap);
	va_end(ap);

	if (size < 0) {
		return -1;
	}

	for (int i = 0; i < size; i++) {
		myputchar(buf[i]);
	}

	return 0;
}

void *malloc(size_t size);

void othertask(void *unused)
{
	(void) unused;

	while (true) {
		doPrintf("HEllo world %p\n", unused);
		RTYield();
		doPrintf("Post %p\n", unused);
	}
}

int main() {
#if 0
	RCC_DeInit();

	// Wait for internal oscillator settle.
	while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET);

	// Turn on the external oscillator
	RCC_HSEConfig(RCC_HSE_ON);

	if (RCC_WaitForHSEStartUp() == ERROR) {
		// Settle for HSI, and flag error.

		// Program the PLL.
		RCC_PLLConfig(RCC_PLLSource_HSI,
				8,	/* PLLM = /8 = 2MHz */
				96,	/* PLLN = *96 = 192MHz */
				2,	/* PLLP = /2 = 96MHz, slight underclock */
				5	/* PLLQ = /5 = 38.4MHz, underclock SDIO
					 * (Maximum is 48MHz)  Will get a 19.2MHz
					 * SD card clock from dividing by 2, or
					 * 9.6MBps at 4 bits wide.
					 */
			);

		osc_err = true;
	} else {
		// Program the PLL.
		RCC_PLLConfig(RCC_PLLSource_HSE,
				4,	/* PLLM = /4 = 2MHz */
				96,	/* PLLN = *96 = 192MHz */
				2,	/* PLLP = /2 = 96MHz, slight underclock */
				5	/* PLLQ = /5 = 38.4MHz, underclock SDIO
					 * (Maximum is 48MHz)  Will get a 19.2MHz
					 * SD card clock from dividing by 2, or
					 * 9.6MBps at 4 bits wide.
					 */
			);
	}

	// Get the PLL starting.
	RCC_PLLCmd(ENABLE);

	// Program this first, just in case we coasted in here with other periphs
	// already enabled.  The loader does all of this stuff, but who knows,
	// maybe in the future our startup code will do less of this.

	RCC_HCLKConfig(RCC_SYSCLK_Div1);	/* AHB = 96MHz */
	RCC_PCLK1Config(RCC_HCLK_Div2);		/* APB1 = 48MHz (lowspeed domain) */
	RCC_PCLK2Config(RCC_HCLK_Div1);		/* APB2 = 96MHz (fast domain) */
	RCC_TIMCLKPresConfig(RCC_TIMPrescDesactivated);
			/* "Desactivate"... the timer prescaler */

	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

	/* SDIO peripheral clocked at 38.4MHz. * 3/8 = minimum APB2 = 14.4MHz,
	 * and we have 96MHz.. so we're good ;) */

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA |
			RCC_AHB1Periph_GPIOB |
			RCC_AHB1Periph_GPIOC |
			RCC_AHB1Periph_GPIOD |
			RCC_AHB1Periph_GPIOE |
			RCC_AHB1Periph_DMA2,
			ENABLE);

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 |
			RCC_APB1Periph_TIM3 |
			RCC_APB1Periph_TIM4 |
			RCC_APB1Periph_TIM5,
			ENABLE);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1 |
			RCC_APB2Periph_USART1 |
			RCC_APB2Periph_SPI1 |
			RCC_APB2Periph_SYSCFG |
			RCC_APB2Periph_SDIO,
			ENABLE);

	/* Seize PA14/PA13 from SWD. */
	GPIO_InitTypeDef swd_def = {
		.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_13,
		.GPIO_Mode = GPIO_Mode_IN,	// Input, not AF
		.GPIO_Speed = GPIO_Low_Speed,
		.GPIO_OType = GPIO_OType_PP,
		.GPIO_PuPd = GPIO_PuPd_NOPULL
	};

	GPIO_Init(GPIOA, &swd_def);

	// Program 3 wait states as necessary at >2.7V for 96MHz
	FLASH_SetLatency(FLASH_Latency_3);

	// Wait for the PLL to be ready.
	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

	SysTick_Config(96000000/250);	/* 250Hz systick */
#endif
	RTHeapInit();

	doPrintf("%d\n", strlen("testing\n"));

	RTTasksInit();

	doPrintf("%d\n", strlen("testing\n"));
	RTTaskCreate(othertask, (void *) 1);
	RTTaskCreate(othertask, (void *) 2);
	RTTaskCreate(othertask, (void *) 3);

	doPrintf("%d\n", strlen("testing\n"));
	doPrintf("testing\n");

	char *a = malloc(16);
	doPrintf("%p\n", a);
	a = malloc(15);
	doPrintf("%p\n", a);
	a = malloc(1048576);
	doPrintf("%p\n", a);
	a = malloc(16);
	doPrintf("%p\n", a);

	RTYield();
}
