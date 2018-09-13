// Dummy RTisan systick handler.  (C) Copyright 2018 Michael Lyle

#include "rtisan_internal.h"

static void systick_handler() __attribute__((interrupt));

static void systick_handler()
{
	systick_cnt++;
	/* XXX proper request resched */
	*((uint32_t volatile *)0xE000ED04) = 0x10000000;
}

void RTSystickTrim(int amount) {
	if (amount < -20) {
		amount = -20;
	} else if (amount > 20) {
		amount = 20;
	}

	SysTick->LOAD = 72000 - 1 + amount;
}

void RTEnableSystick(void)
{
	SysTick->LOAD = 72000 - 1;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
		SysTick_CTRL_TICKINT_Msk |
		SysTick_CTRL_ENABLE_Msk;
}

const void *_systick_vector __attribute((section(".systick_vector"))) = systick_handler;

volatile uint32_t systick_cnt;
