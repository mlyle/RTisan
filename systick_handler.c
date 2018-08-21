// Dummy RTisan systick handler.  (C) Copyright 2018 Michael Lyle

#include "systick_handler.h"

static void systick_handler() __attribute__((interrupt));

static void systick_handler()
{
	systick_cnt++;
}

const void *_systick_vector __attribute((section(".systick_vector"))) = systick_handler;

volatile uint32_t systick_cnt;
