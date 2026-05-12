/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rs485.h
  * @brief   RS485 driver for UART8 — idle-line DMA, ring buffer
  *
  *          Hardware:
  *            UART8: PE0 (RX), PE1 (TX)
  *            DE/RE: PE3 (UART8_DE_Pin)
  *            DMA:   DMA1 Stream2 (RX, CIRCULAR), DMA1 Stream3 (TX, NORMAL)
  *
  *          Architecture:
  *            - RX uses HAL_UARTEx_ReceiveToIdle_DMA() with CIRCULAR DMA
  *            - IDLE line interrupt triggers HAL_UARTEx_RxEventCallback()
  *            - Received data is stored in a ring buffer
  *            - DE pin is automatically set HIGH before TX, LOW after TX
  *            - Application polls RS485_Available() / RS485_Receive() or
  *              registers a callback via RS485_SetRxCallback()
  *
  *          Reference: UART7 Log module (ThirdParty/Log) for the same
  *                     idle-line + CIRCULAR DMA pattern.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __RS485_H__
#define __RS485_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "usart.h"

/* Exported constants --------------------------------------------------------*/

/**
  * @brief RS485 ring buffer size (must be a power of 2 for efficient masking)
  */
#define RS485_RX_BUF_SIZE    256U

/**
  * @brief Maximum frame payload length
  */
#define RS485_MAX_FRAME_LEN  256U

/**
  * @brief DE pin settling time after direction change (in microseconds)
  *        Adjust based on your RS485 transceiver's driver enable time.
  */
#define RS485_DE_SETTLE_US   10U

/* Exported types ------------------------------------------------------------*/

/**
  * @brief RS485 RX frame callback
  * @param data  Pointer to received data buffer
  * @param len   Number of bytes received in this frame
  * @note  Called from HAL_UARTEx_RxEventCallback() (interrupt context).
  *        Keep processing minimal — defer heavy work to the main loop.
  */
typedef void (*RS485_RxCallback_t)(uint8_t *data, uint16_t len);

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize the RS485 driver
  * @note   Must be called after MX_UART8_Init() and MX_DMA_Init().
  *         Starts DMA RX in CIRCULAR mode with IDLE line detection.
  * @retval HAL_OK on success
  * @retval HAL_ERROR if init fails (e.g. DMA start failure)
  */
HAL_StatusTypeDef RS485_Init(void);

/**
  * @brief  Send data over RS485 bus
  * @note   Automatically asserts DE pin before transmission and
  *         de-asserts it after completion. Blocks until TX completes.
  * @param  data  Pointer to data buffer to transmit
  * @param  len   Number of bytes to transmit
  * @retval HAL_OK on success
  * @retval HAL_ERROR on failure
  */
HAL_StatusTypeDef RS485_Send(const uint8_t *data, uint16_t len);

/**
  * @brief  Send data over RS485 bus (non-blocking, DMA-based)
  * @note   Automatically asserts DE pin before transmission.
  *         DE is de-asserted in the TX complete callback.
  *         Application must check HAL_UART_GetState() or use callback.
  * @param  data  Pointer to data buffer to transmit (must remain valid)
  * @param  len   Number of bytes to transmit
  * @retval HAL_OK on success
  * @retval HAL_BUSY if a TX is already in progress
  * @retval HAL_ERROR on failure
  */
HAL_StatusTypeDef RS485_Send_IT(const uint8_t *data, uint16_t len);

/**
  * @brief  Transmit data over RS485 bus (non-blocking, DMA-based)
  * @note   Alias for RS485_Send_IT(). Provided for compatibility with
  *         Modbus master/slave modules that use this function name.
  *         Automatically asserts DE pin before transmission.
  *         DE is de-asserted in the TX complete callback.
  * @param  data  Pointer to data buffer to transmit (must remain valid)
  * @param  len   Number of bytes to transmit
  * @retval HAL_OK on success
  * @retval HAL_BUSY if a TX is already in progress
  * @retval HAL_ERROR on failure
  */
HAL_StatusTypeDef RS485_Transmit(const uint8_t *data, uint16_t len);

/**
  * @brief  Get number of bytes available in the RX ring buffer
  * @retval Number of bytes available to read
  */
uint16_t RS485_Available(void);

/**
  * @brief  Read received data from the RX ring buffer
  * @param  buf      Output buffer to copy received data into
  * @param  max_len  Maximum number of bytes to copy
  * @return Number of bytes actually copied
  */
uint16_t RS485_Receive(uint8_t *buf, uint16_t max_len);

/**
  * @brief  Read received data in-place (zero-copy) from the ring buffer
  * @note   Only reads the contiguous segment starting at the read pointer.
  *         May not return all available data if the buffer wraps around.
  *         Use RS485_Receive() for a full copy.
  * @param  out_len  Output pointer to receive the length of contiguous data
  * @return Pointer to the data in the ring buffer, or NULL if empty
  */
uint8_t *RS485_ReceivePtr(uint16_t *out_len);

/**
  * @brief  Advance the ring buffer read pointer after RS485_ReceivePtr()
  * @param  len  Number of bytes to consume
  */
void RS485_ReceiveAdvance(uint16_t len);

/**
  * @brief  Register a callback for received RS485 frames
  * @param  callback  Function pointer, or NULL to unregister
  * @note   The callback is invoked from HAL_UARTEx_RxEventCallback()
  *         in interrupt context. Keep it short (e.g. set a flag, push
  *         to a queue). Do NOT call RS485_Send() from within the callback.
  */
void RS485_SetRxCallback(RS485_RxCallback_t callback);

/**
  * @brief  Flush (discard) all data in the RX ring buffer
  */
void RS485_Flush(void);

/**
  * @brief  Get the UART handle used by RS485
  * @retval Pointer to huart8
  */
UART_HandleTypeDef *RS485_GetUartHandle(void);

/**
  * @brief  RS485 UART (UART8) RX Event callback
  * @note   Processes IDLE line / DMA TC events for RS485 frame reception.
  *         Called by irq_router.c dispatcher — do NOT call directly.
  * @param  huart  UART handle (must be huart8)
  * @param  Size   Number of bytes transferred (unused)
  */
void RS485_UART_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);

#ifdef __cplusplus
}
#endif

#endif /* __RS485_H__ */
