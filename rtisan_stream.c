#include <rtisan_internal.h>
#include <rtisan_circqueue.h>
#include <rtisan_stream.h>

#include <stdlib.h>

/* XXX caveat: zero copy variants return contiguous region, and this
 * may not be suitable for things that want to atomically receive e.g.
 * 64 bytes or whatever, or things that want "cork" type behavior.
 *
 * XXX any cork type behavior should be impl'd here.
 */

struct RTStream_s {
	uint32_t magic;
#define RTSTREAM_MAGIC 0x74735452 /* 'RTst' */

	int elemSize;

	RTCircQueue_t txCircQueue;
	RTCircQueue_t rxCircQueue;

	RTStreamWakeHandler_t * volatile txCb;
	void * volatile txCtx;
	volatile TaskId_t waitingForTx;
	RTLock_t txLock;

	RTStreamWakeHandler_t * volatile rxCb;
	void * volatile rxCtx;
	volatile TaskId_t waitingForRx;
	RTLock_t rxLock;
};

RTStream_t RTStreamCreate(int elemSize, int txBufSize, int rxBufSize)
{
	assert(elemSize >= 1);
	assert(txBufSize || rxBufSize);

	RTStream_t stream = calloc(1, sizeof(*stream));

	if (!stream) return NULL;

	stream->magic = RTSTREAM_MAGIC;
	stream->elemSize = elemSize;

	if (txBufSize) {
		stream->txLock = RTLockCreate();

		if (!stream->txLock) {
			return NULL;
		}

		stream->txCircQueue = RTCQCreate(elemSize, txBufSize);

		if (!stream->txCircQueue) {
			return NULL;
		}
	}

	if (rxBufSize) {
		stream->rxLock = RTLockCreate();

		if (!stream->rxLock) {
			return NULL;
		}

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
	assert(stream->txCircQueue);

	int retLen = RTCQRead(stream->txCircQueue, buf, len);

	WakeTX(stream);

	return retLen;
}

const char *RTStreamZeroCopyTXPos(RTStream_t stream, int *numBytes)
{
	assert(stream->magic == RTSTREAM_MAGIC);
	assert(stream->txCircQueue);

	return RTCQReadPos(stream->txCircQueue, numBytes, NULL);
}

void RTStreamZeroCopyTXDone(RTStream_t stream, int numBytes)
{
	assert(stream->magic == RTSTREAM_MAGIC);
	assert(stream->txCircQueue);

	RTCQReadDoneMulti(stream->txCircQueue, numBytes);

	WakeTX(stream);
}

int RTStreamSetTXCallback(RTStream_t stream, RTStreamWakeHandler_t cb,
		void *ctx)
{
	assert(stream->magic == RTSTREAM_MAGIC);
	assert(stream->txCircQueue);

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
	assert(stream->txCircQueue);

	int doneSoFar = 0;

	RTLockLock(stream->txLock);

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

	RTLockUnlock(stream->txLock);

	return doneSoFar;
}

int RTStreamReceive(RTStream_t stream, char *buf,
		int len, bool block, bool all)
{
	assert(stream->magic == RTSTREAM_MAGIC);
	assert(stream->rxCircQueue);

	int doneSoFar = 0;

	RTLockLock(stream->rxLock);

	while (true) {
		WakeCounter_t wc;

		if (block) {
			stream->waitingForRx = RTGetTaskId();

			wc = RTGetWakeCount();
		}

		doneSoFar += RTCQRead(stream->rxCircQueue, buf + doneSoFar,
				len - doneSoFar);

		if (stream->rxCb) {
			stream->rxCb(stream, stream->rxCtx);
		}

		if ((!block) ||
				(doneSoFar >= len) ||
				((!all) && doneSoFar)) {
			stream->waitingForRx = 0;

			break;
		}

		RTWait(wc + 1);
	}

	RTLockUnlock(stream->rxLock);

	return doneSoFar;
}

static inline void WakeRX(RTStream_t stream)
{
	TaskId_t waiting = stream->waitingForRx;

	if (waiting) {
		RTWake(waiting);
	}
}

int RTStreamGetRXAvailable(RTStream_t stream)
{
	assert(stream->magic == RTSTREAM_MAGIC);
	assert(stream->rxCircQueue);

	int ret;

	RTStreamZeroCopyRXPos(stream, &ret);

	return ret;
}

int RTStreamDoRXChunk(RTStream_t stream, const char *buf, int len)
{
	assert(stream->magic == RTSTREAM_MAGIC);
	assert(stream->rxCircQueue);

	int ret = RTCQWrite(stream->rxCircQueue, buf, len);

	WakeRX(stream);

	return ret;
}

int RTStreamSetRXCallback(RTStream_t stream, RTStreamWakeHandler_t cb,
		void *ctx)
{
	assert(stream->magic == RTSTREAM_MAGIC);
	assert(stream->rxCircQueue);

	stream->rxCtx = ctx;
	stream->rxCb = cb;

	WakeRX(stream);

	return 0;
}

char *RTStreamZeroCopyRXPos(RTStream_t stream, int *numAvail)
{
	assert(stream->magic == RTSTREAM_MAGIC);
	assert(stream->rxCircQueue);

	char *pos = RTCQWritePos(stream->rxCircQueue,
			numAvail, NULL);

	return pos;
}

void RTStreamZeroCopyRXDone(RTStream_t stream, int numBytes)
{
	assert(stream->magic == RTSTREAM_MAGIC);
	assert(stream->rxCircQueue);

	RTCQWriteAdvance(stream->rxCircQueue, numBytes);

	WakeRX(stream);
}
