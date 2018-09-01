#include <stdio.h>

#include "rtisan_internal.h"

int CDC_Xmit(const char *buf, int len);

static int stdout_impl(char c, FILE *ign)
{
	(void) ign;

	CDC_Xmit(&c, 1);
//	USART1->CR1 = USART_CR1_UE_Msk | USART_CR1_TE_Msk;
//	USART1->DR = c;
	return 0;
}

static FILE outFile =
	FDEV_SETUP_STREAM(stdout_impl, NULL, NULL, _FDEV_SETUP_WRITE);

FILE * const __iob[] = {
	NULL,

	&outFile,
	&outFile,
};
