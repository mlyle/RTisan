#ifndef __RTISAN_SPI_H
#define __RTISAN_SPI_H

#include <stdbool.h>

struct RTSPIPeriph_s;
typedef struct RTSPIPeriph_s *RTSPIPeriph_t;

typedef int (*RTSPIStartHandler_t)(RTSPIPeriph_t periph, void *ctx);
typedef void (*RTSPICompletionHandler_t)(TaskId_t originTask, void *ctx);

typedef struct RTSPITransfer_s {
	const void *tx;
	void *rx;

	int slave;
	int xferLen;
	int speed;

	int cpol : 1;
	int cpha : 1;
	int noDeselect : 1;

	volatile bool completed;
	volatile bool dispatched;

	RTSPICompletionHandler_t completionCb;
	void *ctx;

	TaskId_t originTask;
} RTSPITransfer_t;

RTSPIPeriph_t RTSPICreate(RTSPIStartHandler_t startHandler, void *ctx);

int RTSPIDoTransfers(RTSPIPeriph_t periph, RTSPITransfer_t **transfers,
		int numTransfers);

void RTSPIWakeupTaskCallback(TaskId_t task, void *ctx);

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
		RTSPITransfer_t *transfer, bool success);
RTSPITransfer_t *RTSPITransferNext(RTSPIPeriph_t periph);

#endif /* __RTISAN_SPI_H */
