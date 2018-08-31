#ifndef __RTISAN_CIRCQUEUE_H
#define __RTISAN_CIRCQUEUE_H

#include <stdint.h>

struct RTCircQueue_s;

typedef struct RTCircQueue_s *RTCircQueue_t;

/** Allocate a new circular queue.
 * @param[in] elemSize The size of each element, as obtained from sizeof().
 * @param[in] numElem The number of elements in the queue.  The capacity is
 * one less than this (it may not be completely filled).
 * @returns The handle to the circular queue.
 */
RTCircQueue_t RTCQCreate(uint16_t elemSize, uint16_t numElem);

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
void *RTCQWritePos(RTCircQueue_t q, uint16_t *contig, uint16_t *avail);

/** Makes multiple elements available to the reader.  Amt is expected to be
 * equal or less to an 'avail' returned by RTCQcur_write_pos.
 *
 * @param[in] q The circular q handle.
 * @param[in] amt The number of bytes we've filled in for the reader.
 * @returns 0 if the write succeeded
 */
int RTCQWriteAdvance(RTCircQueue_t q, uint16_t amt);

/** Makes the current block of data available to the reader and advances write pos.
 * This may fail if the queue contain num_elems -1 elements, in which case the
 * advance may be retried in the future.  In this case, data already written to
 * write_pos is preserved and the advance may be retried (or overwritten with
 * new data).
 *
 * @param[in] q Handle to circular queue.
 * @returns 0 if the write succeeded, nonzero on error.
 */
int RTCQWriteAdvanceOne(RTCircQueue_t q);

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
void *RTCQReadPos(RTCircQueue_t q, uint16_t *contig, uint16_t *avail);

/** Empties all elements from the queue. */
void RTCQClear(RTCircQueue_t q);

/** Releases an element of read data obtained by RTCQread_pos.
 * Behavior is undefined if RTCQread_pos did not previously return
 * a block of data.
 *
 * @param[in] q Handle to the circular queue.
 */
void RTCQReadDone(RTCircQueue_t q);

/** Releases multiple elements of read data obtained by RTCQread_pos.
 * Behavior is undefined if returning more than RTCQread_pos
 * previously signaled in contig.
 *
 * @param[in] q Handle to the circula queue.
 * @param[in] num Number of elements to release.
 */
void RTCQReadDoneMulti(RTCircQueue_t q, uint16_t num);

uint16_t RTCQWrite(RTCircQueue_t q, const void *buf, uint16_t num);

uint16_t RTCQRead(RTCircQueue_t q, void *buf, uint16_t num);

#endif /* __RTISAN_CIRCQUEUE_H */
