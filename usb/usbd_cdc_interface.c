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
#define APP_TX_DATA_SIZE  128

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static USBD_CDC_LineCodingTypeDef LineCoding =
  {
    115200, /* baud rate*/
    0x00,   /* stop bits-1*/
    0x00,   /* parity - none*/
    0x08    /* nb. of bits 8*/
  };

static uint8_t UserRxBuffer[APP_RX_DATA_SIZE];/* Received Data over USB are stored in this buffer */

/* USB handler declaration */
extern USBD_HandleTypeDef  hUSBDDevice;

/* Private function prototypes -----------------------------------------------*/
static int8_t CDC_Itf_Init     (void);
static int8_t CDC_Itf_DeInit   (void);
static int8_t CDC_Itf_Control  (uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Itf_Receive  (uint8_t* pbuf, uint32_t *Len);
static int8_t CDC_Itf_TxDone   (void);

static void CDC_TXBegin(RTStream_t stream, void *ctx);

USBD_CDC_ItfTypeDef USBD_CDC_fops = 
{
  CDC_Itf_Init,
  CDC_Itf_DeInit,
  CDC_Itf_Control,
  CDC_Itf_Receive,
  CDC_Itf_TxDone,
};

/* Private functions ---------------------------------------------------------*/

/* XXX */
extern RTStream_t cdcStream;

/**
  * @brief  CDC_Itf_Init
  *         Initializes the CDC media low layer
  * @param  None
  * @retval Result of the opeartion: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_Init(void)
{
  USBD_CDC_SetRxBuffer(&hUSBDDevice, UserRxBuffer);

  RTStreamSetTXCallback(cdcStream, CDC_TXBegin, NULL);
  CDC_TXBegin(NULL, NULL);

  /* XXX Make safe with startup */

  return (USBD_OK);
}

static void CDC_StartTx(bool finished)
{
	static volatile int inProg = 0;
	static int numBytes = 0;

	/* XXX safety */
	if (finished) {
		if (numBytes) {
			RTStreamZeroCopyTXDone(cdcStream, numBytes);
			numBytes = 0;
		}

		inProg = 0;
	} else if (inProg) {
		return;
	}

	const char *toXmit = RTStreamZeroCopyTXPos(cdcStream, &numBytes);

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

static void CDC_TXBegin(RTStream_t stream, void *ctx)
{
	(void) stream; (void) ctx;

	CDC_StartTx(false);
}

static int8_t CDC_Itf_TxDone   (void)
{
    CDC_StartTx(true);

    return (USBD_OK);
}

/**
  * @brief  CDC_Itf_DeInit
  *         DeInitializes the CDC media low layer
  * @param  None
  * @retval Result of the opeartion: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_DeInit(void)
{
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
static int8_t CDC_Itf_Control (uint8_t cmd, uint8_t* pbuf, uint16_t length)
{ 
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
    LineCoding.bitrate    = (uint32_t)(pbuf[0] | (pbuf[1] << 8) |\
                            (pbuf[2] << 16) | (pbuf[3] << 24));
    LineCoding.format     = pbuf[4];
    LineCoding.paritytype = pbuf[5];
    LineCoding.datatype   = pbuf[6];
    
    /* Set the new configuration (N/A) */
    break;

  case CDC_GET_LINE_CODING:
    pbuf[0] = (uint8_t)(LineCoding.bitrate);
    pbuf[1] = (uint8_t)(LineCoding.bitrate >> 8);
    pbuf[2] = (uint8_t)(LineCoding.bitrate >> 16);
    pbuf[3] = (uint8_t)(LineCoding.bitrate >> 24);
    pbuf[4] = LineCoding.format;
    pbuf[5] = LineCoding.paritytype;
    pbuf[6] = LineCoding.datatype;     
    
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
static int8_t CDC_Itf_Receive(uint8_t* Buf, uint32_t *Len)
{
  //HAL_UART_Transmit_DMA(&UartHandle, Buf, *Len);
  USBD_CDC_ReceivePacket(&hUSBDDevice);  /* If we ate this data and are done with it*/
  return (USBD_OK);
}

/**
  * @}
  */ 

/**
  * @}
  */ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

