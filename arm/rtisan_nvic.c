#include <rtisan_nvic.h>

#include <rtisan_internal.h>

void RTNVICEnable(int irqn)
{
	int idx = irqn >> 5;
	uint32_t bit = 1 << (irqn & 0x1f);

	// XXX	NVIC->IP[irqn] = ;
	NVIC->ISER[idx] = bit;
}
