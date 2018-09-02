#include <stdio.h>

#include <rtisan_internal.h>
#include <rtisan_stream.h>

extern RTStream_t cdcStream;
int CDC_Xmit(const char *buf, int len);

static int stdout_impl(char c, FILE *ign)
{
	(void) ign;

	RTStreamSend(cdcStream, &c, 1, true);
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
