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
	volatile TaskId_t waitingForTx;

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

static inline void WakeTX(RTStream_t stream)
{
	TaskId_t waiting = stream->waitingForTx;

	if (waiting) {
		RTWake(waiting);
	}
}

int RTStreamGetTXChunk(RTStream_t stream, char *buf, int len)
{
	assert(stream->magic == RTSTREAM_MAGIC);

	int retLen = RTCQRead(stream->txCircQueue, buf, len);
	
	WakeTX(stream);

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

	WakeTX(stream);
}

int RTStreamSetTXCallback(RTStream_t stream, RTStreamWakeHandler_t cb,
		void *ctx)
{
	assert(stream->magic == RTSTREAM_MAGIC);

	stream->txCtx = ctx;
	stream->txCb = cb;

	/* If someone has been waiting to transmit, they may not have
	 * been able to wake the handler.  On the other hand, doing it
	 * synchronously here is problematic.  Therefore wake them so
	 * they can wake the transmitter.
	 */
	WakeTX(stream);

	return 0;
}

int RTStreamSend(RTStream_t stream, const char *buf,
		int len, bool block)
{
	assert(stream->magic == RTSTREAM_MAGIC);

	int doneSoFar = 0;

	RTLockLock(stream->lock);

	while (true) {
		WakeCounter_t wc;

		if (block) {
			stream->waitingForTx = RTGetTaskId();

			wc = RTGetWakeCount();
		}

		doneSoFar += RTCQWrite(stream->txCircQueue, buf + doneSoFar,
				len - doneSoFar);

		if (stream->txCb) {
			stream->txCb(stream, stream->txCtx);
		}
	
		if ((!block) || (doneSoFar >= len)) {
			stream->waitingForTx = 0;

			break;
		}

		RTWait(wc + 1);
	}

	RTLockUnlock(stream->lock);

	return doneSoFar;
}

int RTStreamReceive(RTStream_t stream, const char *buf,
		int len, bool block)
{
	assert(stream->magic == RTSTREAM_MAGIC);

	return 0;
}
