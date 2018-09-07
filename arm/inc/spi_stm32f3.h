#ifndef __SPI_STM32F3_H
#define __SPI_STM32F3_H

#include <rtisan_spi.h>
#include <rtisan_dio.h>

struct RTSPIF3Pins_s {
	DIOInitTag_t miso, mosi, sck;
	DIOInitTag_t slaves[];		/* Null terminated */
};

RTSPIPeriph_t RTSPIF3Create(int spiIdx, const struct RTSPIF3Pins_s *pins);


#endif /* __SPI_STM32F3_H */
