// Minimal rtisan allocation impl.  (C) Copyright 2018 Michael Lyle

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

extern char _sheap, _eheap;
static uint32_t HeapSize;

static volatile uint32_t HeapPos;

void RTHeapInit()
{
	HeapSize = ((uintptr_t) &_eheap) - ((uintptr_t) &_sheap);
}

void *malloc(size_t size)
{
	/* We don't implement negative sbrk, etc */
	if (size < 0) {
		return NULL;
	}

	/* bump up size to be aligned */
	uint8_t alignFudge = size & 0x03;

	if (alignFudge) {
		size += 4 - alignFudge;
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

void *calloc(size_t nmemb, size_t size)
{
	size_t fullSize = nmemb * size;

	void *ret = malloc(fullSize);

	if (ret) {
		memset(ret, 0, fullSize);
	}

	return ret;
}

void *_sbrk(intptr_t increment)
{
	return malloc(increment);
}
