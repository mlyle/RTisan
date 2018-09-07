#include <spi_stm32f3.h>
#include <rtisan_internal.h>

#include <rtisan_dio.h>
#include <rtisan_nvic.h>

#include <stdint.h>
#include <stdlib.h>

typedef struct RTSPIF3Periph_s {
#define RTSPIF3_MAGIC 0x33495053 /* SPI3 */
	uint32_t magic;

	SPI_TypeDef *spi;
	const struct RTSPIF3Pins_s *pins;

	int refClock;

	uint16_t txPos;
	uint16_t rxPos;

	int selectedSlave;
	int numSlaves;

	RTSPITransfer_t *transfer;

	RTSPIPeriph_t wrapper;
} *RTSPIF3Periph_t;

static RTSPIF3Periph_t F3SPIHandles[3];

static inline void RTSPIF3Deselect(RTSPIF3Periph_t periph)
{
	DIOHigh(periph->pins->slaves[periph->selectedSlave]);

	periph->selectedSlave = -1;
}

static inline void RTSPIF3Select(RTSPIF3Periph_t periph,
		RTSPITransfer_t *transfer)
{
	/* No one should be selected by this point */
	assert(periph->selectedSlave == -1);

	assert(transfer->slave >= 0);
	assert(transfer->slave < periph->numSlaves);

	periph->selectedSlave = transfer->slave;

	DIOLow(periph->pins->slaves[periph->selectedSlave]);
}

static void RTSPIF3BeginTransfer(RTSPIF3Periph_t periph)
{
	RTSPITransfer_t *transfer = periph->transfer;

	assert(transfer);

	if ((periph->selectedSlave != -1) &&
			(periph->selectedSlave != transfer->slave)) {
		/*
		 * Disable any chip select that isn't ours.
		 * (We can only get here if the previous transfer
		 * requested no deselect, but now we're using a
		 * different device).
		 */
		RTSPIF3Deselect(periph);
	}

	/* Not supported: bidirectional mode, CRC mode, "RX only" mode,
	 * slave, LSB-first ordering */
	SPI_TypeDef *spi = periph->spi;

	/* First program SPI parameters */
	uint16_t tmp = SPI_CR1_MSTR_Msk | SPI_CR1_SSM_Msk | SPI_CR1_SSI_Msk;

	if (transfer->cpol) {
		tmp |= SPI_CR1_CPOL_Msk;
	}

	if (transfer->cpha) {
		tmp |= SPI_CR1_CPHA_Msk;
	}

	int speed = periph->refClock / 2;

	uint16_t divisor = 0;

	while (speed > transfer->speed) {
		speed /= 2;
		divisor += 1;
	}

	divisor <<= SPI_CR1_BR_Pos;

	assert(!(divisor & (~SPI_CR1_BR_Msk)));

	tmp |= divisor;

	spi->CR1 = tmp;

	if (periph->selectedSlave != transfer->slave) {
		/* if our chip select isn't enabled, do so */
		RTSPIF3Select(periph, transfer);
	}

	/* CR2 -- data width & interrupt configuration */
	spi->CR2 = (7 << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH |
		SPI_CR2_TXEIE | SPI_CR2_RXNEIE;

	/* Finally, enable SPI */
	tmp |= SPI_CR1_SPE_Msk;

	spi->CR1 = tmp;
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
	periph->rxPos = 0;
	periph->txPos = 0;

	/*
	 * Otherwise initialize SPI for what we're doing, and do
	 * chip select work and enable interrupt
	 */
	RTSPIF3BeginTransfer(periph);
}

static void RTSPIF3DoWork(int idx)
{
	RTSPIF3Periph_t periph = F3SPIHandles[idx];

	assert(periph);
	assert(periph->magic == RTSPIF3_MAGIC);

	SPI_TypeDef *spi = periph->spi;

	assert(periph->transfer);

	/* XXX overrun: think about it */

	/* While the RXNE flag is set, receive bytes. */
	while (spi->SR & SPI_SR_RXNE_Msk) {
		/* Ensure we have not received more than expected */
		assert(periph->rxPos < periph->transfer->xferLen);
		assert(periph->rxPos < periph->txPos);

		/* It's necessary to force 8 bit accesses here. */
		uint8_t b = *((volatile uint8_t *) &(spi->DR));

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

	/* If the transmit buffer has space (and receive buffer is OK)
	 * try to fill transmit buffer */
	while ((spi->SR & (SPI_SR_TXE_Msk | SPI_SR_RXNE_Msk)) ==
			SPI_SR_TXE_Msk) {
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


		*((volatile uint8_t *) &(spi->DR)) = b;

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
	if (!RTSPIF3InterruptsEnabled(periph)) {
		/* if not, start next transfer */
		RTSPIF3NextMessage(periph);
	}

	return 0;
}

void RTSPIF3IRQSPI1(void)
{
	RTSPIF3DoWork(0);
}

void RTSPIF3IRQSPI2(void)
{
	RTSPIF3DoWork(1);
}

void RTSPIF3IRQSPI3(void)
{
	RTSPIF3DoWork(2);
}

RTSPIPeriph_t RTSPIF3Create(int spiIdx, const struct RTSPIF3Pins_s *pins)
{
	RTSPIF3Periph_t periph = calloc(1, sizeof(*periph));

	periph->magic = RTSPIF3_MAGIC;
	periph->wrapper = RTSPICreate(RTSPIF3Start, periph);
	periph->selectedSlave = -1;
	
	/* XXX HACK */
	periph->refClock = 72000000;

	assert(periph->wrapper);

	int irqn;

	switch (spiIdx) {
		case 1:
			periph->spi = SPI1;
			irqn = SPI1_IRQn;
			break;
		case 2:
			periph->spi = SPI2;
			irqn = SPI2_IRQn;
			break;
		case 3:
			periph->spi = SPI3;
			irqn = SPI3_IRQn;
			break;
		default:
			abort();
	}

	spiIdx--;

	DIOInit(pins->mosi);
	DIOInit(pins->miso);
	DIOInit(pins->sck);

	int numSlaves = 0;

	while (pins->slaves[numSlaves] != DIO_NULL) {
		assert(numSlaves < 16);		/* Let's be reasonable */
		DIOInit(pins->slaves[numSlaves++]);
	}

	if (numSlaves == 0) {
		/*
		 * Special purpose:  If there's just a null slave, still
		 * allow SPI to work without chip select.  For things like
		 * encoders.
		 *
		 * The actual access code will use DIO_NULL for selecting
		 * and unselecting-- effectively NOPs.
		 */
		numSlaves = 1;
	}

	assert(numSlaves > 0);

	periph->pins = pins;
	periph->numSlaves = numSlaves;

	F3SPIHandles[spiIdx] = periph;

	/* Enable the interrupt */
	RTNVICEnable(irqn);

	return periph->wrapper;
}
