#include <rtisan_internal.h>
#include <rtisan_circqueue.h>
#include <rtisan_stream.h>

#include <stdlib.h>

struct RTStream_s {
	uint32_t magic;
#define RTSTREAM_MAGIC 0x74735452 /* 'RTst' */

	int elemSize;

	RTCircQueue_t txCircQueue;
	RTCircQueue_t rxCircQueue;

	RTStreamWakeHandler_t * volatile txCb;
	void * volatile txCtx;

	RTLock_t lock;
};

RTStream_t RTStreamCreate(int elemSize, int txBufSize, int rxBufSize)
{
	assert(elemSize >= 1);
	assert(txBufSize || rxBufSize);

	RTStream_t stream = calloc(1, sizeof(*stream));

	if (!stream) return NULL;

	stream->magic = RTSTREAM_MAGIC;
	stream->elemSize = elemSize;

	stream->lock = RTLockCreate();

	if (!stream->lock) {
		return NULL;
	}

	if (txBufSize) {
		stream->txCircQueue = RTCQCreate(elemSize, txBufSize);

		if (!stream->txCircQueue) {
			return NULL;
		}
	}

	if (rxBufSize) {
		stream->rxCircQueue = RTCQCreate(elemSize, rxBufSize);

		if (!stream->rxCircQueue) {
			return NULL;
		}
	}

	return stream;
}

int RTStreamGetTXChunk(RTStream_t stream, char *buf, int len)
{
	assert(stream->magic == RTSTREAM_MAGIC);

	int retLen = RTCQRead(stream->txCircQueue, buf, len);

	return retLen;
}

const char *RTStreamZeroCopyTXPos(RTStream_t stream, int *numBytes)
{
	assert(stream->magic == RTSTREAM_MAGIC);

	return RTCQReadPos(stream->txCircQueue, numBytes, NULL);
}

void RTStreamZeroCopyTXDone(RTStream_t stream, int numBytes)
{
	assert(stream->magic == RTSTREAM_MAGIC);

	RTCQReadDoneMulti(stream->txCircQueue, numBytes);
}

int RTStreamSetTXCallback(RTStream_t stream, RTStreamWakeHandler_t cb,
		void *ctx)
{
	assert(stream->magic == RTSTREAM_MAGIC);

	stream->txCtx = ctx;
	stream->txCb = cb;

	return 0;
}

int RTStreamSend(RTStream_t stream, const char *buf,
		int len, bool block)
{
	block = false;		/* XXX */
	assert(stream->magic == RTSTREAM_MAGIC);

	int doneSoFar = 0;

	RTLockLock(stream->lock);

	while (true) {
		doneSoFar += RTCQWrite(stream->txCircQueue, buf + doneSoFar,
				len - doneSoFar);

		if ((!block) || (doneSoFar >= len)) {
			break;
		}

		/* XXX block / sleep / wakeup */
	}

	RTLockUnlock(stream->lock);

	if (doneSoFar) {
		if (stream->txCb) {
			stream->txCb(stream, stream->txCtx);
		}
	}
	
	return doneSoFar;
}

int RTStreamReceive(RTStream_t stream, const char *buf,
		int len, bool block)
{
	assert(stream->magic == RTSTREAM_MAGIC);

	return 0;
}
