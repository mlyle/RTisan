// RTisan startup code and interrupt vectors.  (C) Copyright 2018 Michael Lyle

#include <string.h>
#include <strings.h>

#include <rtisan_internal.h>

extern char _sbss, _ebss, _sidata, _sdata, _edata, _stack_top,
		_start_vectors /*, _sfast, _efast */;

typedef const void (vector)(void);

extern int main(void);

void _start(void) __attribute__((noreturn, naked, no_instrument_function));

void _start(void)
{
//	SCB->VTOR = (uintptr_t) &_start_vectors;

	/* Copy data segment */
	memcpy(&_sdata, &_sidata, &_edata - &_sdata);

	/* Zero BSS segment */
	bzero(&_sbss, &_ebss - &_sbss);

	/* Setup CCM SRAM XXX */
	/* bzero(&_sfast, &_efast - &_sfast); */

#ifdef HAVE_FPU
	/* Enable the FPU in the coprocessor state register; CP10/11 */
	SCB->CPACR |= 0x00f00000;

	/* FPU interrupt handling: lazy save of FP state */
	FPU->FPCCR |= FPU_FPCCR_ASPEN_Msk | FPU_FPCCR_LSPEN_Msk;

	/* Default NaN mode (don't propagate; just make NaN) and
	 * flush-to-zero handling for subnormals
	 */
	FPU->FPDSCR |= FPU_FPDSCR_DN_Msk | FPU_FPDSCR_FZ_Msk;
#endif

	/* Invoke main. */
	asm volatile ("bl	main");
	__builtin_unreachable();
}

/* This ends one early, so other code can provide systick handler */
const void *_vectors[] __attribute((section(".vectors"))) =
{
	&_stack_top,
	_start,
	0,      /* XXX: NMI_Handler */
	0,      /* XXX: HardFault_Handler */
	0,      /* XXX: MemManage_Handler */
	0,      /* XXX: BusFault_Handler */
	0,      /* XXX: UsageFault_Handler */
	0,      /* 4 reserved */
	0,
	0,
	0,
	0,      /* SVCall */
	0,      /* Reserved for debug */
	0,      /* Reserved */
};
