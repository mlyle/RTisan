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

void RTFlashErasePage(uint16_t *page)
{
	UnlockFlash();
	AssertNotBusy();

	/* Select page erase operation, and page, and initiate op */
	FLASH->CR = (FLASH->CR & (~FLASHOPMASK)) | FLASH_CR_PER_Msk;
	FLASH->AR = (uintptr_t) page;
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

	LockFlash();
}

void RTFlashProgram(uint16_t *dest, uint16_t *source, int len)
{
	UnlockFlash();
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
	LockFlash();
}
