#ifndef __RTISAN_FLASH_H
#define __RTISAN_FLASH_H

#include <stdint.h>

int RTFlashAddressToPage(uint16_t *addr);
void RTFlashErasePage(int page);
void RTFlashProgram(uint16_t *dest, uint16_t *source, int len);

#ifdef __linux__
void RTFlashRead(uint16_t *dest, uint16_t *tgt, size_t len);
#endif

#endif
