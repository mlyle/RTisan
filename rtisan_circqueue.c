#include "rtisan_circqueue.h"

#include <stdlib.h>
#include <string.h>

#include "rtisan_assert.h"

struct RTCircQueue_s {
	uint16_t elemSize;	/**< Element size in octets */
	uint16_t numElem;	/**< Number of elements in circqueue (capacity+1) */
	volatile uint16_t writeHead;	/**< Element position writer is at */
	volatile uint16_t readTail;	/**< Element position reader is at */

	/* head == tail: empty.
	 * head == tail-1: full.
	 */

	/* This is declared as a uint32_t for alignment reasons. */
	uint32_t contents[];		/**< Contents of the circular queue */
};

/** Allocate a new circular queue.
 * @param[in] elemSize The size of each element, as obtained from sizeof().
 * @param[in] numElem The number of elements in the queue.  The capacity is
 * one less than this (it may not be completely filled).
 * @returns The handle to the circular queue.
 */
RTCircQueue_t RTCQCreate(uint16_t elemSize, uint16_t numElem) {
	assert(elemSize > 0);
	assert(numElem >= 2);

	uint32_t size = elemSize * numElem;

	/* PIOS_malloc_no_dma may not be safe for some later uses.. hmmm */
	RTCircQueue_t ret = malloc(sizeof(*ret) + size);

	if (!ret)
		return NULL;

	memset(ret, 0, sizeof(*ret) + size);

	ret->elemSize = elemSize;
	ret->numElem = numElem;

	return ret;
}

/** Get a pointer to the current queue write position.
 * This position is unavailable to the reader and may be filled in
 * with the desired data without respect to any synchronization.
 * No promise is made that RTCQadvance_write will succeed, though.
 *
 * Alternatively, in *contig we return the number of things that can be
 * filled in.  We promise that you can RTCQadvance_write_multi that
 * many.  You can also always write one, but no promises you'll be able to
 * advance write.
 *
 * @param[in] q Handle to circular queue.
 * @param[out] contig The num elements available for contiguous write.
 * @param[out] avail The num elements available before a reader has
 * freed up more space.  (Includes wraparound/non-contig elems).
 * @returns The position for new data to be written to (of size elemSize).
 */
void *RTCQWritePos(RTCircQueue_t q, uint16_t *contig,
		uint16_t *avail) {
	void *contents = q->contents;
	uint16_t wrHead = q->writeHead;
	uint16_t rdTail = q->readTail;

	if (contig) {
		if (rdTail <= wrHead) {
			/* Avail is the num elems to the end of the buf */
			int16_t offset = 0;

			if (rdTail == 0) {
				/* Can't advance to the beginning, because
				 * we'd catch our tail.  [only when tail
				 * perfectly wrapped] */
				offset = -1;
			}
			*contig = q->numElem - wrHead + offset;
		} else {
			/* rd_tail > wr_head */
			/* wr_head is not allowed to advance to meet tail,
			 * so minus one */
			*contig = rdTail - wrHead - 1;
		}
	}

	if (avail) {
		if (rdTail <= wrHead) {
			/* To end of buf, to rd_tail, minus one. */
			*avail = q->numElem - wrHead + rdTail - 1;
		} else {
			/* Otherwise just to 1 before rd_tail. */
			*avail = rdTail - wrHead - 1;
		}
	}

	return contents + wrHead * q->elemSize;
}

static inline uint16_t AdvanceByN(uint16_t numPos, uint16_t currentPos,
		uint16_t numToAdvance) {
	assert(currentPos < numPos);
	assert(numToAdvance <= numPos);

	uint32_t pos = currentPos + numToAdvance;

	if (pos >= numPos) {
		pos -= numPos;
	}

	return pos;
}

static inline uint16_t NextPos(uint16_t numPos, uint16_t currentPos) {
	return AdvanceByN(numPos, currentPos, 1);
}

/** Makes multiple elements available to the reader.  Amt is expected to be
 * equal or less to an 'avail' returned by RTCQcur_write_pos.
 *
 * @param[in] q The circular q handle.
 * @param[in] amt The number of bytes we've filled in for the reader.
 * @returns 0 if the write succeeded
 */
int RTCQWriteAdvance(RTCircQueue_t q, uint16_t amt) {
	if (amt == 0) {
		return 0;
	}

	uint16_t origWrHead = q->writeHead;

	uint16_t newWriteHead = AdvanceByN(q->numElem, origWrHead, amt);

	/* Legal states at the end of this are---
	 * a "later place" in the buffer without wrapping.
	 * or the 0th position-- if we've consumed all to the end.
	 */

	// assert((newWriteHead > origWrHead) || (newWriteHead == 0));

	/* the head is not allowed to advance to meet the tail */
	if (newWriteHead == q->readTail) {
		/* This is only sane if they're trying to return one, like
		 * advance_write does */
		// assert(amt == 1);

		return -1;	/* Full */

		/* Caller can either let the data go away, or try again to
		 * advance later. */
	}

	q->writeHead = newWriteHead;

	return 0;
}

/** Makes the current block of data available to the reader and advances write pos.
 * This may fail if the queue contain numElems -1 elements, in which case the
 * advance may be retried in the future.  In this case, data already written to
 * write_pos is preserved and the advance may be retried (or overwritten with
 * new data).
 *
 * @param[in] q Handle to circular queue.
 * @returns 0 if the write succeeded, nonzero on error.
 */
int RTCQWriteAdvanceOne(RTCircQueue_t q) {
	return RTCQWriteAdvance(q, 1);
}

/** Returns a block of data to the reader.
 * The block is "claimed" until released with RTCQread_completed.
 * No new data is available until that call is made (instead the same
 * block-in-progress will be returned).
 *
 * @param[in] q Handle to circular queue.
 * @param[out] contig Returns number of contig elements that can be
 * read at once.
 * @param[out] avail Returns number of elements available to read
 * without any further writer activity.
 * @returns pointer to the data, or NULL if the queue is empty.
 */
void *RTCQReadPos(RTCircQueue_t q, uint16_t *contig, uint16_t *avail) {
	uint16_t readTail = q->readTail;
	uint16_t wr_head = q->writeHead;

	void *contents = q->contents;

	if (contig) {
		if (wr_head >= readTail) {
			/* readTail is allowed to advance to meet head,
			 * so no minus one here. */
			*contig = wr_head - readTail;
		} else {
			/* Number of contiguous elements to end of the buf,
			 * otherwise. */
			*contig = q->numElem - readTail;
		}
	}

	if (avail) {
		if (wr_head >= readTail) {
			/* Same as immediately above; no wrap avail */
			*avail = wr_head - readTail;
		} else {
			/* Distance to end, plus distance from beginning
			 * to wr_head */
			*avail = q->numElem - readTail + wr_head;
		}
	}

	if (wr_head == readTail) {
		/* There is nothing new to read.  */
		return NULL;
	}

	return contents + q->readTail * q->elemSize;
}

/** Empties all elements from the queue. */
void RTCQClear(RTCircQueue_t q) {
	q->readTail = q->writeHead;
}

/** Releases an element of read data obtained by RTCQread_pos.
 * Behavior is undefined if RTCQread_pos did not previously return
 * a block of data.
 *
 * @param[in] q Handle to the circular queue.
 */
void RTCQReadDone(RTCircQueue_t q) {
	/* Avoid multiple accesses to a volatile */
	uint16_t readTail = q->readTail;

	/* If this is being called, the queue had better not be empty--
	 * we're supposed to finish consuming this element after a prior call
	 * to RTCQread_pos.
	 */
	assert(readTail != q->writeHead);

	q->readTail = NextPos(q->numElem, readTail);
}

/** Releases multiple elements of read data obtained by RTCQread_pos.
 * Behavior is undefined if returning more than RTCQread_pos
 * previously signaled in contig.
 *
 * @param[in] q Handle to the circula queue.
 * @param[in] num Number of elements to release.
 */
void RTCQReadDoneMulti(RTCircQueue_t q, uint16_t num) {
	if (num == 0) {
		return;
	}

	/* Avoid multiple accesses to a volatile */
	uint16_t origReadTail = q->readTail;

	/* If this is being called, the queue had better not be empty--
	 * we're supposed to finish consuming this element after a prior call
	 * to RTCQread_pos.
	 */
	assert(origReadTail != q->writeHead);

	uint16_t readTail = AdvanceByN(q->numElem, origReadTail, num);

	/* Legal states at the end of this are---
	 * a "later place" in the buffer without wrapping.
	 * or the 0th position-- if we've consumed all to the end.
	 */

	assert((readTail > origReadTail) || (readTail == 0));

	q->readTail = readTail;
}

uint16_t RTCQWrite(RTCircQueue_t q, const void *buf, uint16_t num) {
	uint16_t total_put = 0;
	uint16_t putThisTime = 0;

	const void *bufPos = buf;

	do {
		void *wr_pos = RTCQWritePos(q, &putThisTime, NULL);

		if (!putThisTime) break;

		if (putThisTime > (num - total_put)) {
			putThisTime = num - total_put;
		}

		uint32_t sz = putThisTime * q->elemSize;

		memcpy(wr_pos, bufPos, sz);

		RTCQWriteAdvance(q, putThisTime);

		bufPos += sz;

		total_put += putThisTime;
	} while (total_put < num);

	return total_put;
}

uint16_t RTCQRead(RTCircQueue_t q, void *buf, uint16_t num) {
	uint16_t totalRead = 0;
	uint16_t readThisTime = 0;

	void *bufPos = buf;

	do {
		void *rdPos = RTCQReadPos(q, &readThisTime, NULL);

		if (!readThisTime) break;

		if (readThisTime > (num - totalRead)) {
			readThisTime = num - totalRead;
		}

		uint32_t sz = readThisTime * q->elemSize;

		memcpy(bufPos, rdPos, sz);

		RTCQReadDoneMulti(q, readThisTime);

		bufPos += sz;

		totalRead += readThisTime;
	} while (totalRead < num);

	return totalRead;
}

