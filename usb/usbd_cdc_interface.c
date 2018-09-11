/**
  ******************************************************************************
  * @file    USB_Device/CDC_Standalone/Src/usbd_cdc_interface.c
  * @author  MCD Application Team
  * @brief   Source file for USBD CDC interface
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics International N.V.
  * All rights reserved.</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice,
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other
  *    contributors to this software may be used to endorse or promote products
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under
  *    this license is void and will automatically terminate your rights under
  *    this license.
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f3xx_hal.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_interface.h"

#include <rtisan.h>
#include <rtisan_assert.h>
#include <rtisan_circqueue.h>
#include <rtisan_stream.h>

#include <stdbool.h>

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @{
  */

/** @defgroup USBD_CDC
  * @brief usbd core module
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define APP_RX_DATA_SIZE  64

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

struct CDCInterface_s {
	uint32_t magic;
#define CDCINTERFACE_MAGIC 0x69434443   /* 'CDCi' */
	uint8_t UserRxBuffer[APP_RX_DATA_SIZE];
	RTStream_t stream;
	USBD_CDC_LineCodingTypeDef LineCoding;
};

#define MAX_INTERFACES 2

static struct CDCInterface_s *instances[MAX_INTERFACES];

/* USB handler declaration */
extern USBD_HandleTypeDef hUSBDDevice;

/* Private function prototypes -----------------------------------------------*/
static int8_t CDC_Itf_Init(int instId, void **ctx);
static int8_t CDC_Itf_DeInit(void *ctx);
static int8_t CDC_Itf_Control(void *ctx, uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Itf_Receive(void *ctx, uint8_t *pbuf, uint32_t *Len);
static int8_t CDC_Itf_TxDone(void *ctx);

static void CDC_TXBegin(RTStream_t stream, void *ctx);
static void CDC_RXBegin(RTStream_t stream, void *ctx);

USBD_CDC_ItfTypeDef USBD_CDC_fops = {
	CDC_Itf_Init,
	CDC_Itf_DeInit,
	CDC_Itf_Control,
	CDC_Itf_Receive,
	CDC_Itf_TxDone,
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  CDC_Itf_Init
  *         Initializes the CDC media low layer
  * @param  None
  * @retval Result of the opeartion: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_Init(int instId, void **ctx)
{
/* XXX */
	extern RTStream_t cdcStream;
/* XXX return back the context pointer for that instance id */
/* XXX Set TX buffer / RX Buffer / TransmitPacket / Receive packet need to
 * take the instance id
 */
	if (instId > MAX_INTERFACES) {
		return USBD_FAIL;
	}
	if (instances[instId]) {
		*ctx = instances[instId];
		return USBD_OK;
	}

	struct CDCInterface_s *iface = malloc(sizeof(*iface));
	assert(iface);

	instances[instId] = iface;
	iface->magic = CDCINTERFACE_MAGIC;
	iface->LineCoding = (USBD_CDC_LineCodingTypeDef) {
		115200, /* baud rate*/
		0x00, /* stop bits-1*/
		0x00, /* parity - none*/
		0x08 /* nb. of bits 8*/
	};

	*ctx = iface;

	if (instId != 0) {
		/* XXX */
		return USBD_OK;
	}

	/* XXX */
	iface->stream = cdcStream;

	USBD_CDC_SetRxBuffer(&hUSBDDevice, iface->UserRxBuffer);

	RTStreamSetTXCallback(iface->stream, CDC_TXBegin, iface);
	RTStreamSetRXCallback(iface->stream, CDC_RXBegin, iface);

	return (USBD_OK);
}

/* XXX add an instance ID to all other calls */
static void CDC_StartTx(void *ctx, bool finished)
{
	struct CDCInterface_s *iface = ctx;
	assert(iface->magic == CDCINTERFACE_MAGIC);

	static volatile int inProg = 0;
	static int numBytes = 0;

	/* XXX safety */
	if (finished) {
		if (numBytes) {
			RTStreamZeroCopyTXDone(iface->stream, numBytes);
			numBytes = 0;
		}

		inProg = 0;
	} else if (inProg) {
		return;
	}

	const char *toXmit = RTStreamZeroCopyTXPos(iface->stream, &numBytes);

	/* XXX cork */
	/* XXX handles */
	/* XXX abstraction */
	if (numBytes) {
		inProg = 1;

		if (numBytes > 64) {
			numBytes = 64;
		}

		USBD_CDC_SetTxBuffer(&hUSBDDevice, (const uint8_t *) toXmit,
				numBytes);
		USBD_CDC_TransmitPacket(&hUSBDDevice);
	}
}

static void CDC_StartRx(void *ctx, bool finished)
{
	struct CDCInterface_s *iface = ctx;
	assert(iface->magic == CDCINTERFACE_MAGIC);

	static volatile int inProg;

	if (finished) {
		inProg = 0;
	} else if (inProg) {
		return;
	}

	int recvSpace = RTStreamGetRXAvailable(iface->stream);

	if (recvSpace >= 64) {
		inProg = 1;
		USBD_CDC_ReceivePacket(&hUSBDDevice);
	}
}

static void CDC_RXBegin(RTStream_t stream, void *ctx)
{
	(void) stream;

	CDC_StartRx(ctx, false);
}

static void CDC_TXBegin(RTStream_t stream, void *ctx)
{
	(void) stream;

	CDC_StartTx(ctx, false);
}

static int8_t CDC_Itf_TxDone(void *ctx)
{
	CDC_StartTx(ctx, true);

	return (USBD_OK);
}

/**
  * @brief  CDC_Itf_DeInit
  *         DeInitializes the CDC media low layer
  * @param  None
  * @retval Result of the opeartion: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_DeInit(void *ctx)
{
	struct CDCInterface_s *iface = ctx;
	assert(iface->magic == CDCINTERFACE_MAGIC);

	return (USBD_OK);
}

/**
  * @brief  CDC_Itf_Control
  *         Manage the CDC class requests
  * @param  Cmd: Command code
  * @param  Buf: Buffer containing command data (request parameters)
  * @param  Len: Number of data to be sent (in bytes)
  * @retval Result of the opeartion: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_Control(void *ctx, uint8_t cmd, uint8_t *pbuf,
		uint16_t length)
{
	struct CDCInterface_s *iface = ctx;
	assert(iface->magic == CDCINTERFACE_MAGIC);

	switch (cmd)
	{
	case CDC_SEND_ENCAPSULATED_COMMAND:
	case CDC_GET_ENCAPSULATED_RESPONSE:
	case CDC_SET_COMM_FEATURE:
	case CDC_GET_COMM_FEATURE:
	case CDC_CLEAR_COMM_FEATURE:
	case CDC_SET_CONTROL_LINE_STATE:
		break;

	case CDC_SET_LINE_CODING:
		iface->LineCoding.bitrate    = (uint32_t)(pbuf[0] | (pbuf[1] << 8) | \
				(pbuf[2] << 16) | (pbuf[3] << 24));
		iface->LineCoding.format     = pbuf[4];
		iface->LineCoding.paritytype = pbuf[5];
		iface->LineCoding.datatype   = pbuf[6];

		/* Set the new configuration (N/A) */
		break;

	case CDC_GET_LINE_CODING:
		pbuf[0] = (uint8_t)(iface->LineCoding.bitrate);
		pbuf[1] = (uint8_t)(iface->LineCoding.bitrate >> 8);
		pbuf[2] = (uint8_t)(iface->LineCoding.bitrate >> 16);
		pbuf[3] = (uint8_t)(iface->LineCoding.bitrate >> 24);
		pbuf[4] = iface->LineCoding.format;
		pbuf[5] = iface->LineCoding.paritytype;
		pbuf[6] = iface->LineCoding.datatype;

		break;

	default:
		break;
	}

	return (USBD_OK);
}

/**
  * @brief  CDC_Itf_DataRx
  *         Data received over USB OUT endpoint are sent over CDC interface
  *         through this function.
  * @param  Buf: Buffer of data to be transmitted
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the opeartion: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_Receive(void *ctx, uint8_t *Buf, uint32_t *Len)
{
	struct CDCInterface_s *iface = ctx;
	assert(iface->magic == CDCINTERFACE_MAGIC);

	RTStreamDoRXChunk(iface->stream, (char *) Buf, *Len);
	CDC_StartRx(ctx, true);

	return (USBD_OK);
}

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
