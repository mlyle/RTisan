#ifndef __RTISAN_STREAM_H
#define __RTISAN_STREAM_H

#include <stdbool.h>

struct RTStream_s;

typedef struct RTStream_s *RTStream_t;

typedef void (RTStreamWakeHandler_t)(RTStream_t stream, void *ctx);


RTStream_t RTStreamCreate(int elemSize, int txBufSize, int rxBufSize);
int RTStreamSend(RTStream_t stream, const char *buf,
		int len, bool block);
int RTStreamReceive(RTStream_t stream, char *buf,
		int len, bool block, bool all);

int RTStreamGetTXChunk(RTStream_t stream, char *buf, int len);
int RTStreamSetTXCallback(RTStream_t stream, RTStreamWakeHandler_t cb,
		void *ctx);
const char *RTStreamZeroCopyTXPos(RTStream_t stream, int *numBytes);
void RTStreamZeroCopyTXDone(RTStream_t stream, int numBytes);

int RTStreamGetRXAvailable(RTStream_t stream);
int RTStreamDoRXChunk(RTStream_t stream, const char *buf, int len);
int RTStreamSetRXCallback(RTStream_t stream, RTStreamWakeHandler_t cb,
		void *ctx);
char *RTStreamZeroCopyRXPos(RTStream_t stream, int *numAvail);
void RTStreamZeroCopyRXDone(RTStream_t stream, int numBytes);

#endif /* __RTISAN_STREAM_H */
