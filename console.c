#include <stdio.h>

#define UART0_R ((volatile uint32_t *) 0x4000C000)

static int stdout_impl(char c, FILE *ign)
{
	(void) ign;

        *UART0_R = c;
	return 0;
}

static FILE outFile =
	FDEV_SETUP_STREAM(stdout_impl, NULL, NULL, _FDEV_SETUP_WRITE);

FILE * const __iob[] = {
	NULL,

	&outFile,
	&outFile,
};
