#ifndef __RTISAN_FLASH_H
#define __RTISAN_FLASH_H

#include <rtisan_internal.h>
#include <rtisan_flash.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static int flashFd;
#define FLASH_SIZE 262144
#define FLASH_PAGE_SIZE 2048

static void OpenFlash(void)
{
	if (flashFd > 0) {
		return;
	}

	flashFd = open("theflash.bin", O_RDWR | O_CREAT, 0600);

	assert(flashFd >= 0);

	int res;

	res = ftruncate(flashFd, FLASH_SIZE);

	assert(!res);
}

int RTFlashAddressToPage(uint16_t *addr)
{
	printf("GP %p\n", addr);
	return (uintptr_t) addr;
}

void RTFlashRead(uint16_t *dest, uint16_t *tgt, size_t len)
{
	uintptr_t pos = (uintptr_t) tgt;

	printf("PGR %p %p %d\n", tgt, dest, (int) len);

	OpenFlash();

	if (pos > 0x08000000) {
		pos -= 0x08000000;
	}

	assert(pos < FLASH_SIZE);
	assert(pos + (len * 2) < FLASH_SIZE);

	int res;

	res = lseek(flashFd, pos, SEEK_SET);
	assert(res == pos);

	res = read(flashFd, dest, len * 2);
	assert(res == (len * 2));
}

void RTFlashProgram(uint16_t *dest, uint16_t *source, int len)
{
	printf("PGW %p %p %d\n", dest, source, (int) len);
	OpenFlash();

	uintptr_t pos = (uintptr_t) dest;
	if (pos > 0x08000000) {
		pos -= 0x08000000;
	}

	assert(pos < FLASH_SIZE);
	assert(pos + (len * 2) < FLASH_SIZE);

	int res;

	res = lseek(flashFd, pos, SEEK_SET);
	assert(res == pos);

	res = write(flashFd, source, len * 2);
	assert(res == (len * 2));
}

void RTFlashErasePage(int page)
{
	printf("ERASE PG %d\n", (int) page);

	uintptr_t pageAddr = page;

	OpenFlash();

	uint16_t pg[FLASH_PAGE_SIZE / 2];

	memset(pg, 0xff, sizeof(pg));

	RTFlashProgram((uint16_t *) pageAddr, pg, sizeof(pg) / 2);
}

#endif
