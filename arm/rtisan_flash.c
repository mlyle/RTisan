#include <rtisan_flash.h>
#include <rtisan_internal.h>

static inline void UnlockFlash(void)
{
	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;
}

static inline void LockFlash(void)
{
	FLASH->CR |= FLASH_CR_LOCK_Msk;
}

static inline void WaitBusy(void)
{
	while (FLASH->SR & FLASH_SR_BSY_Msk);
}

static inline void AssertNotBusy(void)
{
	assert(!(FLASH->SR & FLASH_SR_BSY_Msk));
}

#define FLASHOPMASK ( FLASH_CR_PG_Msk | FLASH_CR_PER_Msk | FLASH_CR_PG_Msk )
#define FLASHPAGESIZE 2048

int RTFlashAddressToPage(uint16_t *addr)
{
	uintptr_t ptr = (uintptr_t) addr;

	assert(!(ptr & (~(FLASH_BASE | 0x3fffff))));

	ptr -= FLASH_BASE;

	ptr /= FLASHPAGESIZE;

	return ptr;
}

void RTFlashErasePage(int page)
{
	LockFlash();
	AssertNotBusy();

	/* Select page erase operation, and page, and initiate op */
	FLASH->CR = (FLASH->CR & (~FLASHOPMASK)) | FLASH_CR_PER_Msk;
	FLASH->AR = page;
	FLASH->CR |= FLASH_CR_STRT_Msk;

	/* Wait for hardware completion */
	WaitBusy();
	
	/* Make sure just end-of-program bit is set */
	assert((FLASH->SR &
			(FLASH_SR_BSY_Msk | FLASH_SR_PGERR_Msk |
			 FLASH_SR_WRPERR_Msk | FLASH_SR_EOP_Msk)) ==
			FLASH_SR_EOP_Msk);

	/* Reset end-of-program bit by storing 1 to it. */
	FLASH->SR = FLASH_SR_EOP_Msk;

	UnlockFlash();
}

void RTFlashProgram(uint16_t *dest, uint16_t *source, int len)
{
	LockFlash();
	AssertNotBusy();

	FLASH->CR = (FLASH->CR & (~FLASHOPMASK)) | FLASH_CR_PG_Msk;

	while (len--) {
		*(dest++) = *(source++);

		/* Wait for hardware completion */
		WaitBusy();

		/* Make sure just end-of-program bit is set */
		assert((FLASH->SR &
				(FLASH_SR_BSY_Msk | FLASH_SR_PGERR_Msk |
				 FLASH_SR_WRPERR_Msk | FLASH_SR_EOP_Msk)) ==
				FLASH_SR_EOP_Msk);

		/* Reset end-of-program bit by storing 1 to it. */
		FLASH->SR = FLASH_SR_EOP_Msk;
	}

	AssertNotBusy();
	UnlockFlash();
}
