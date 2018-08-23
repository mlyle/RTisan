// Dummy RTisan systick handler.  (C) Copyright 2018 Michael Lyle

#include "systick_handler.h"

static void systick_handler() __attribute__((interrupt));

static void systick_handler()
{
	systick_cnt++;
	/* XXX proper request resched */
	*((uint32_t volatile *)0xE000ED04) = 0x10000000;
}

void RTEnableSystick(void)
{
	*((uint32_t *) 0xE000E014) = 50000;
	*((uint32_t *) 0xE000E010) = 0x00000007;
}

const void *_systick_vector __attribute((section(".systick_vector"))) = systick_handler;

volatile uint32_t systick_cnt;
