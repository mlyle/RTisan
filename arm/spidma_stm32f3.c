#include <spi_stm32f3.h>
#include <rtisan_internal.h>

#include <rtisan_dio.h>
#include <rtisan_nvic.h>

#include <stdint.h>
#include <stdlib.h>

static char allOnes = 0xff;
static char dontCare;

typedef struct RTSPIDMAF3Periph_s {
#define RTSPIDMAF3_MAGIC 0x33645053 /* SPd3 */
	uint32_t magic;

	SPI_TypeDef *spi;
	const struct RTSPIF3Pins_s *pins;

	DMA_Channel_TypeDef *dmaTX, *dmaRX;
	uint32_t tcif;

	DMA_TypeDef *dma;

	int refClock;

	int selectedSlave;
	int numSlaves;

	RTSPITransfer_t *transfer;

	RTSPIPeriph_t wrapper;
} *RTSPIDMAF3Periph_t;

static RTSPIDMAF3Periph_t F3SPIHandles[3];

static inline void RTSPIDMAF3Deselect(RTSPIDMAF3Periph_t periph)
{
	DIOHigh(periph->pins->slaves[periph->selectedSlave]);

	periph->selectedSlave = -1;
}

static inline void RTSPIDMAF3Select(RTSPIDMAF3Periph_t periph,
		RTSPITransfer_t *transfer)
{
	/* No one should be selected by this point */
	assert(periph->selectedSlave == -1);

	assert(transfer->slave >= 0);
	assert(transfer->slave < periph->numSlaves);

	periph->selectedSlave = transfer->slave;

	DIOLow(periph->pins->slaves[periph->selectedSlave]);
}

static void RTSPIDMAF3BeginTransfer(RTSPIDMAF3Periph_t periph)
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
		RTSPIDMAF3Deselect(periph);
	}

	/*
	 * Wait while busy flag is set-- just out of an abundance of
	 * caution (we should never get here with it set).
	 */
	while (periph->spi->SR & SPI_SR_BSY_Msk);

	/* Deconfig DMA channels */
	periph->dmaTX->CCR = 0;
	periph->dmaRX->CCR = 0;

	/* Not supported: bidirectional mode, CRC mode, "RX only" mode,
	 * slave, LSB-first ordering */
	SPI_TypeDef *spi = periph->spi;

	/* First program SPI parameters */
	uint16_t cr1tmp = SPI_CR1_MSTR_Msk | SPI_CR1_SSM_Msk | SPI_CR1_SSI_Msk;

	if (transfer->cpol) {
		cr1tmp |= SPI_CR1_CPOL_Msk;
	}

	if (transfer->cpha) {
		cr1tmp |= SPI_CR1_CPHA_Msk;
	}

	/* XXX consider caching speed */

	int speed = periph->refClock / 2;

	uint16_t scaler = 0;

	while (speed > transfer->speed) {
		speed /= 2;
		scaler++;
	}

	scaler <<= SPI_CR1_BR_Pos;

	assert(!(scaler & (~SPI_CR1_BR_Msk)));

	cr1tmp |= scaler;

	/* This disables the SPI, if we were in a previous transaction */
	spi->CR1 = cr1tmp;

	if (periph->selectedSlave != transfer->slave) {
		/* if our chip select isn't enabled, do so */
		RTSPIDMAF3Select(periph, transfer);
	}

	/* CR2 -- data width, DMA & interrupt configuration */

	/* This disables the DMA streams, first. */
	uint16_t cr2tmp = (7 << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;

	spi->CR2 = cr2tmp;

	/* Then ST says we need to enable the RX buffer */
	cr2tmp |= SPI_CR2_RXDMAEN;
	spi->CR2 = cr2tmp;

	/* Next, clear the DMA completion interrupt flag */
	periph->dma->ISR = periph->tcif;

	/*
	 * Next we configure both DMA streams
	 * The DMA peripheral address is already programmed into each.
	 * Set each memory address, and set each CCR channel config.
	 */ 

#define DMA_CCR_PL_HIGH (DMA_CCR_PL_1)
#define DMA_CCR_PL_VERYHIGH (DMA_CCR_PL_1 | DMA_CCR_PL_0)

	uint32_t txCCR = DMA_CCR_PL_HIGH | DMA_CCR_DIR;
	uint32_t rxCCR = DMA_CCR_PL_VERYHIGH | DMA_CCR_TCIE;

	if (transfer->tx) {
		txCCR |= DMA_CCR_MINC;
		periph->dmaTX->CMAR = (uintptr_t) transfer->tx;
	} else {
		/* Just stuff the same byte of all ones in each time */
		periph->dmaTX->CMAR = (uintptr_t) &allOnes;
	}

	periph->dmaTX->CCR = txCCR;

	if (transfer->rx) {
		rxCCR |= DMA_CCR_MINC;
		periph->dmaRX->CMAR = (uintptr_t) transfer->rx;
	} else {
		/*
		 * Each byte received is just discarded.  Why still use
		 * DMA?  We use it to count the number of bytes received
		 * to detect the true completion of transaction.
		 */
		periph->dmaRX->CMAR = (uintptr_t) &dontCare;
	}

	periph->dmaRX->CCR = rxCCR;

	/* configure the number of data to be transferred into each
	 * CNDTR register
	 */

	periph->dmaRX->CNDTR = transfer->xferLen;
	periph->dmaTX->CNDTR = transfer->xferLen;

	/*
	 * Finally, activate the DMA channel by setting the enable bit in
	 * each CCR.
	 */

	periph->dmaTX->CCR = txCCR | DMA_CCR_EN;
	periph->dmaRX->CCR = rxCCR | DMA_CCR_EN;

	/* Next, we enable the DMA TX buffer */
	cr2tmp |= SPI_CR2_TXDMAEN;

	spi->CR2 = cr2tmp;

	/* Finally, enable SPI */
	cr1tmp |= SPI_CR1_SPE_Msk;

	spi->CR1 = cr1tmp;
}

static inline bool RTSPIDMAF3InterruptsEnabled(RTSPIDMAF3Periph_t periph)
{
	/* Check if is installed DMA reception interrupt */

	if (periph->dmaTX->CCR & DMA_CCR_TCIE) {
		return true;
	}

	return false;
}

static void RTSPIDMAF3NextMessage(RTSPIDMAF3Periph_t periph)
{
	assert(!periph->transfer);

	RTSPITransfer_t *nextMessage;

	/* Deque a message */
	nextMessage = RTSPITransferNext(periph->wrapper);

	if (!nextMessage) {
		/* if nothing dequed, disable interrupt, and return. */
		periph->dmaTX->CCR &= ~DMA_CCR_TCIE;

		return;
	}

	periph->transfer = nextMessage;

	/*
	 * Otherwise initialize SPI for what we're doing, and do
	 * chip select work and enable interrupt
	 */
	RTSPIDMAF3BeginTransfer(periph);
}

static void RTSPIDMAF3DoWork(int idx)
{
	RTSPIDMAF3Periph_t periph = F3SPIHandles[idx];

	assert(periph);
	assert(periph->magic == RTSPIDMAF3_MAGIC);
	assert(periph->transfer);

	if (!periph->transfer->noDeselect) {
		RTSPIDMAF3Deselect(periph);
	}

	/* set completed status */
	RTSPITransferCompleted(periph->wrapper,
			periph->transfer, true);

	periph->transfer = NULL;

	RTSPIDMAF3NextMessage(periph);
}

static int RTSPIDMAF3Start(RTSPIPeriph_t wrapper, void *ctx)
{
	RTSPIDMAF3Periph_t periph = ctx;

	assert(periph->magic == RTSPIDMAF3_MAGIC);
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
	if (!RTSPIDMAF3InterruptsEnabled(periph)) {
		/* if not, start next transfer */
		RTSPIDMAF3NextMessage(periph);
	}

	return 0;
}

void RTSPIDMAF3IRQDMA1Chan2(void)
{
	RTSPIDMAF3DoWork(0);
}

void RTSPIDMAF3IRQDMA1Chan4(void)
{
	RTSPIDMAF3DoWork(1);
}

void RTSPIDMAF3IRQDMA2Chan1(void)
{
	RTSPIDMAF3DoWork(2);
}

RTSPIPeriph_t RTSPIDMAF3Create(int spiIdx, const struct RTSPIF3Pins_s *pins)
{
	RTSPIDMAF3Periph_t periph = calloc(1, sizeof(*periph));

	periph->magic = RTSPIDMAF3_MAGIC;
	periph->wrapper = RTSPICreate(RTSPIDMAF3Start, periph);
	periph->selectedSlave = -1;
	
	/* XXX HACK */
	periph->refClock = 72000000;
	if (spiIdx != 1) {
		/* SPI2 & SPI3 are on APB1 which runs at half clock */
		periph->refClock /= 2;
	}

	assert(periph->wrapper);

	int irqn;
	int dmaRXIdx;

	switch (spiIdx) {
		case 1:
			periph->dma = DMA1;
			periph->dmaTX = DMA1_Channel3;
			periph->dmaRX = DMA1_Channel2;
			dmaRXIdx = 2;

			periph->spi = SPI1;
			irqn = DMA1_Channel2_IRQn;
			break;
		case 2:
			periph->dma = DMA1;
			periph->dmaTX = DMA1_Channel5;
			periph->dmaRX = DMA1_Channel4;
			dmaRXIdx = 4;

			periph->spi = SPI2;
			irqn = DMA1_Channel4_IRQn;
			break;
		case 3:
			periph->dma = DMA2;
			periph->dmaTX = DMA2_Channel2;
			periph->dmaRX = DMA2_Channel1;
			dmaRXIdx = 1;

			periph->spi = SPI3;
			irqn = DMA2_Channel1_IRQn;
			break;
		default:
			abort();
	}

	periph->tcif = DMA_ISR_TCIF1_Msk << (4 * (dmaRXIdx - 1));

	/*
	 * Program DMA peripheral address (SPI DR) into channels, because
	 * there's no reason to do this per-transfer.
	 */
	periph->dmaRX->CPAR = (uintptr_t) (&periph->spi->DR);
	periph->dmaTX->CPAR = (uintptr_t) (&periph->spi->DR);

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
