#include <stdio.h>

#include <rtisan_internal.h>
#include <rtisan_stream.h>

extern RTStream_t cdcStreams[2];
int CDC_Xmit(const char *buf, int len);

static int stdout_impl(char c, FILE *ign)
{
	(void) ign;

	if (c == '\n') {
		RTStreamSend(cdcStreams[0], "\r\n", 2, true);
	} else {
		RTStreamSend(cdcStreams[0], &c, 1, true);
	}

	return 0;
}

static int stdin_impl(FILE *ign)
{
	(void) ign;
	char c;

	RTStreamReceive(cdcStreams[0], &c, sizeof(c), true, false);

	return (unsigned char) c;
}

static FILE outFile =
	FDEV_SETUP_STREAM(stdout_impl, NULL, NULL, _FDEV_SETUP_WRITE);

static FILE inFile =
	FDEV_SETUP_STREAM(NULL, stdin_impl, NULL, _FDEV_SETUP_READ);

FILE * const __iob[] = {
	&inFile,

	&outFile,
	&outFile,
};
