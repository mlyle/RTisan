// Minimal rtisan allocation impl.  (C) Copyright 2018 Michael Lyle

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

extern char _sheap, _eheap;
static uint32_t HeapSize;
static volatile uint32_t HeapPos;

uint32_t RTHeapFree(uint32_t *heapSize)
{
	if (heapSize) {
		*heapSize = HeapSize;
	}

	return HeapSize - HeapPos;
}

void RTHeapInit()
{
	HeapSize = ((uintptr_t) &_eheap) - ((uintptr_t) &_sheap);
}

/* alignment must be a power of 2 */
void *RTaligned_alloc(size_t alignment, size_t size)
{
	/* We don't implement negative sbrk, etc */
	if (size < 0) {
		return NULL;
	}

	if (alignment == 0) {
		alignment = 1;
	}

	/* bump up size to be aligned */
	uint8_t alignFudge = size & (alignment - 1);

	if (alignFudge) {
		size += alignment - alignFudge;
	}

	/* Use atomics for an efficient heap allocator */
	while (true) {
		uint32_t myPos = HeapPos;
		uint32_t newPos = myPos + size;

		if (newPos > HeapSize) {
			return NULL;
		}

		if (__sync_bool_compare_and_swap(&HeapPos, myPos, newPos)) {
			return &_sheap + myPos;
		}
	}
}

void *RTmalloc(size_t size)
{
	return RTaligned_alloc(8, size);
}

void *RTcalloc(size_t nmemb, size_t size)
{
	/* XXX: could overrun */
	size_t fullSize = nmemb * size;

	void *ret = RTmalloc(fullSize);

	if (ret) {
		memset(ret, 0, fullSize);
	}

	return ret;
}

void *_malloc_r(struct _reent *re, size_t n)
{
	(void) re;

	return RTmalloc(n);
}

void *_calloc_r(struct _reent *re, size_t n, size_t m)
{
	(void) re;

	return RTcalloc(n, m);
}

void _free_r(struct _reent *re, void *ptr)
{
	(void) re;

	(void) ptr;
}

void *_sbrk_r(struct _reent *re, intptr_t increment)
{
	return RTmalloc(increment);
}

void abort(void)
{
	/* Disable interrupts, so no other tasks run. */
	asm volatile("CPSID i");

	while (1);
}

