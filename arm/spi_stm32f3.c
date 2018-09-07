#include <spi_stm32f3.h>
#include <rtisan_internal.h>

#include <stdint.h>
#include <stdlib.h>

typedef struct RTSPIF3Periph_s {
#define RTSPIF3_MAGIC 0x33495053 /* SPI3 */
	uint32_t magic;

	SPI_TypeDef *spi;

	int refClock;

	uint16_t txPos;
	uint16_t rxPos;

	int selectedSlave;
	int numSlaves;

	RTSPITransfer_t *transfer;

	RTSPIPeriph_t wrapper;
} *RTSPIF3Periph_t;

static inline void RTSPIF3Deselect(RTSPIF3Periph_t periph)
{
	/* XXX do chip select work */
	periph->selectedSlave = -1;
}

static inline void RTSPIF3Select(RTSPIF3Periph_t periph,
		RTSPITransfer_t *transfer)
{
	/* No one should be selected by this point */
	assert(periph->selectedSlave == -1);

	assert(transfer->slave >= 0);
	assert(transfer->slave < periph->numSlaves);

	/* XXX do chip select work */

	periph->selectedSlave = transfer->slave;
}

static void RTSPIF3BeginTransfer(RTSPIF3Periph_t periph)
{
	RTSPITransfer_t *transfer = periph->transfer;

	assert(transfer);

	if ((periph->selectedSlave != -1) &&
			(periph->selectedSlave != transfer->slave)) {
		/* XXX disable any chip select that isn't ours */
		RTSPIF3Deselect(periph);
	}

	/* Not supported: bidirectional mode, CRC mode, "RX only" mode,
	 * slave, LSB-first ordering */
	SPI_TypeDef *spi = periph->spi;

	/* First program SPI parameters */
	uint32_t tmp = SPI_CR1_MSTR_Msk;

	if (transfer->cpol) {
		tmp |= SPI_CR1_CPOL_Msk;
	}

	if (transfer->cpha) {
		tmp |= SPI_CR1_CPHA_Msk;
	}

	int speed = transfer->speed / 2;

	uint16_t divisor = 0;

	while (speed > periph->refClock) {
		speed /= 2;
		divisor += 1;
	}

	divisor <<= SPI_CR1_BR_Pos;

	assert(!(divisor & (~SPI_CR1_BR_Msk)));

	tmp |= divisor;

	spi->CR1 = tmp;

	/* Next enable SPI */	
	tmp |= SPI_CR1_SPE_Msk;

	spi->CR1 = tmp;

	if (periph->selectedSlave != transfer->slave) {
		/* if our chip select isn't enabled, do so */
		RTSPIF3Select(periph, transfer);
	}

	/* CR2 -- data width & interrupt configuration */
	spi->CR2 = (7 << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH |
		SPI_CR2_TXEIE | SPI_CR2_RXNEIE;
}

static inline bool RTSPIF3InterruptsEnabled(RTSPIF3Periph_t periph)
{
	if (periph->spi->CR2 & 
			(SPI_CR2_TXEIE | SPI_CR2_RXNEIE)) {
		return true;
	}

	return false;
}

static void RTSPIF3NextMessage(RTSPIF3Periph_t periph)
{
	assert(!periph->transfer);

	RTSPITransfer_t *nextMessage;

	/* Deque a message */
	nextMessage = RTSPITransferNext(periph->wrapper);

	if (!nextMessage) {
		/* if nothing dequed, disable interrupt, and return. */
		periph->spi->CR2 = (7 << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;

		return;
	}

	periph->transfer = nextMessage;

	/*
	 * Otherwise initialize SPI for what we're doing, and do
	 * chip select work and enable interrupt
	 */
	RTSPIF3BeginTransfer(periph);
}

void RTSPIF3DoWork(RTSPIF3Periph_t periph)
{
	SPI_TypeDef *spi = periph->spi;

	assert(periph->transfer);

	/* XXX overrun: think about it */

	/* While the RXNE flag is set, receive bytes. */
	while (spi->SR & SPI_SR_RXNE_Msk) {
		/* Ensure we have not received more than expected */
		assert(periph->rxPos < periph->transfer->xferLen);
		uint8_t b = spi->DR;

		/* If there is a receive buffer, fill in the byte */
		if (periph->transfer->rx) {
			((uint8_t *) periph->transfer->rx)[periph->rxPos] = b;
		}

		periph->rxPos++;
	}

	/* If we have received the expected amount, complete the
	 * periph->transfer.
	 */
	if (periph->rxPos == periph->transfer->xferLen) {
		assert(periph->txPos == periph->transfer->xferLen);

		if (!periph->transfer->noDeselect) {
			RTSPIF3Deselect(periph);
		}

		/* set completed status */
		RTSPITransferCompleted(periph->wrapper,
				periph->transfer, true);

		periph->transfer = NULL;

		RTSPIF3NextMessage(periph);
	}

	if (!periph->transfer) {
		return;
	}

	/* If the transmit buffer has space, try to fill it */
	while (spi->SR & SPI_SR_TXE_Msk) {
		/* But not if we have sent all we need to for this
		 * txn.
		 */
		if (periph->txPos >= periph->transfer->xferLen) {
			return;
		}

		/* Default to sending all 1's */
		uint8_t b = 0xff;

		if (periph->transfer->tx) {
			b = ((uint8_t *) periph->transfer->tx)[periph->txPos];
		}

		spi->DR = b;

		periph->txPos++;
	}
}

static int RTSPIF3Start(RTSPIPeriph_t wrapper, void *ctx)
{
	RTSPIF3Periph_t periph = ctx;

	assert(periph->magic == RTSPIF3_MAGIC);
	assert(periph->wrapper == wrapper);

	__sync_synchronize();

	/* At this point, the work item has already been posted to the
	 * queue.  The interrupt handler is "atomic" from our point of
	 * view.
	 *
	 * So if the interrupt handler is disabled, let's set up the
	 * transfer, which in turn enables it.
	 */

	/* Check if interrupt handler enabled */
	if (RTSPIF3InterruptsEnabled(periph)) {
		/* if not, start next transfer */
		RTSPIF3NextMessage(periph);
	}

	return 0;
}

RTSPIPeriph_t RTSPIF3Create(void)
{
	RTSPIF3Periph_t periph = calloc(1, sizeof(*periph));

	periph->magic = RTSPIF3_MAGIC;
	periph->wrapper = RTSPICreate(RTSPIF3Start, periph);
	periph->selectedSlave = -1;

	assert(periph->wrapper);

	/* XXX specifying port */
	/* XXX specifying slaves */
	/* XXX NVIC */
	/* XXX hooking interrupt path */

	return periph->wrapper;
}
