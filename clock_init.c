#include "rtisan_internal.h"

void ClockConfiguration(void)
{
	// Turn on external oscillator.
	RCC->CR |= RCC_CR_HSEON_Msk;

	// Wait for it to be ticking nice.
	while (!(RCC->CR & RCC_CR_HSERDY_Msk));

	// Program the PLL.
	RCC->PLLCFGR =
			(4 << RCC_PLLCFGR_PLLM_Pos) |
			(336 << RCC_PLLCFGR_PLLN_Pos) |
			(2 << RCC_PLLCFGR_PLLP_Pos) |
			(7 << RCC_PLLCFGR_PLLQ_Pos);

	// Enable it.
	RCC->CR |= RCC_CR_PLLON_Msk;

	// Wait for it to be ticking nice.
	while (!(RCC->CR &  RCC_CR_PLLRDY_Msk));

	RCC->CFGR =
			(RCC_CFGR_SW_PLL) |
			(RCC_CFGR_HPRE_DIV1) |
			(RCC_CFGR_PPRE1_DIV4) |
			(RCC_CFGR_PPRE2_DIV2);

	RCC->APB2ENR |= RCC_APB2ENR_USART1EN_Msk;
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN_Msk;
#if 0
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
#endif
}

