#ifndef __SPIDMA_STM32F3_H
#define __SPIDMA_STM32F3_H

#include <rtisan_spi.h>
#include <rtisan_dio.h>

#include <spi_stm32f3.h>	/* For pin structure */

RTSPIPeriph_t RTSPIDMAF3Create(int spiIdx, const struct RTSPIF3Pins_s *pins);

void RTSPIDMAF3IRQDMA1Chan2(void);
void RTSPIDMAF3IRQDMA1Chan4(void);
void RTSPIDMAF3IRQDMA2Chan1(void);

#endif /* __SPIDMA_STM32F3_H */
