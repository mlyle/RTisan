#include <rtisan.h>
#include <rtisan_circqueue.h>
#include <rtisan_internal.h>

#include <rtisan_spi.h>

#include <stdlib.h>

#define MAXNUM_TRANSFERS 7

struct RTSPIPeriph_s {
#define RTSPI_MAGIC 0x53504970
	uint32_t magic;

	RTCircQueue_t transfers;
	RTLock_t addLock;

	RTSPIStartHandler_t startHandler;
	void *ctx;
};

RTSPIPeriph_t RTSPICreate(RTSPIStartHandler_t startHandler, void *ctx)
{
	RTSPIPeriph_t periph = malloc(sizeof(*periph));

	assert(periph);

	periph->magic = RTSPI_MAGIC;
	periph->transfers = RTCQCreate(sizeof(RTSPITransfer_t *),
			MAXNUM_TRANSFERS + 1);

	assert(periph->transfers);

	periph->addLock = RTLockCreate();

	assert(periph->addLock);

	periph->startHandler = startHandler;
	periph->ctx = ctx;

	return periph;
}

void RTSPIWakeupTaskCallback(TaskId_t task, void *ctx)
{
	RTWake(task);
	RTResched();
}

/* Drivers are expected to:
 * In a loop, call RTSPITransferNext, then if not NULL, call
 * RTSPITransferCompleted.
 *
 * End/suspend this loop upon getting a NULL.
 *
 * Start this loop upon getting a start callback, and be appropriately
 * idempotent (e.g. it's not bad if this is called when running)
 */
void RTSPITransferCompleted(RTSPIPeriph_t periph,
		RTSPITransfer_t *transfer, bool success)
{
	assert(transfer->dispatched);
	assert(!transfer->completed);

	transfer->completed = true;

	/* call completion callback if present. */
	if (transfer->completionCb) {
		transfer->completionCb(transfer->originTask,
				transfer->ctx);
	}
}

RTSPITransfer_t *RTSPITransferNext(RTSPIPeriph_t periph)
{
	RTSPITransfer_t *result;

	if (RTCQRead(periph->transfers, &result, 1) != 1) {
		return NULL;
	}

	assert(!result->dispatched);

	result->dispatched = true;

	assert(!result->completed);

	return result;
}

int RTSPIDoTransfers(RTSPIPeriph_t periph, RTSPITransfer_t **transfers,
		int numTransfers)
{
	int ret = -1;

	TaskId_t task = RTGetTaskId();

	for (int i = 0; i < numTransfers; i++) {
		transfers[i]->originTask = task;
		transfers[i]->completed = false;
		transfers[i]->dispatched = false;

		assert(transfers[i]->xferLen > 0);
	}

	RTLockLock(periph->addLock);

	int avail;

	RTCQWritePos(periph->transfers, NULL, &avail);

	if (avail >= numTransfers) {
		RTCQWrite(periph->transfers, transfers, numTransfers);

		ret = 0;

		/* Start peripheral; implementation must be idempotent */
		if (periph->startHandler) {
			if (periph->startHandler(periph, periph->ctx)) {
				abort();
			}
		}
	}

	RTLockUnlock(periph->addLock);

	return ret;
}
