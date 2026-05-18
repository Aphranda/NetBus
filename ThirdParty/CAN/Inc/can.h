/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.h
  * @brief   CAN driver for FDCAN1 (classic CAN mode)
  *
  *          Hardware:
  *            FDCAN1: PD0 (RX), PD1 (TX), AF9
  *            Mode:   Classic CAN (FDCAN_FRAME_CLASSIC)
  *
  *          Architecture:
  *            - Uses HAL_FDCAN driver on top of MX_FDCAN1_Init().
  *            - RX via Rx FIFO 0 with interrupt notification.
  *            - TX via Tx FIFO/Queue (blocking or non-blocking).
  *            - Application registers a callback via CAN_SetRxCallback()
  *              or polls CAN_GetRxMessage() from the main loop.
  *
  *          Reference: RS485 driver (ThirdParty/RS485) for the same
  *                     callback + polling dual-path pattern.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __CAN_H__
#define __CAN_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "fdcan.h"

/* Exported constants --------------------------------------------------------*/

/**
  * @brief CAN standard ID mask (11-bit)
  */
#define CAN_STD_ID_MASK         0x7FFU

/**
  * @brief Maximum CAN data length (classic CAN)
  */
#define CAN_MAX_DATA_LEN        8U

/**
  * @brief Default CAN timeout for blocking transmit (milliseconds)
  */
#define CAN_TX_TIMEOUT_MS       100U

/* Exported types ------------------------------------------------------------*/

/**
  * @brief CAN message structure (classic CAN / 11-bit ID)
  */
typedef struct {
    uint32_t id;                    /*!< Standard CAN ID (11-bit, 0x000-0x7FF) */
    uint8_t  data[CAN_MAX_DATA_LEN]; /*!< Data bytes                             */
    uint8_t  dlc;                   /*!< Data length code (0-8)                 */
} CAN_Msg_t;

/**
  * @brief CAN RX frame callback
  * @param msg  Pointer to the received CAN message
  * @note  Called from HAL_FDCAN_RxFifo0Callback() (interrupt context).
  *        Keep processing minimal — defer heavy work to the main loop.
  */
typedef void (*CAN_RxCallback_t)(const CAN_Msg_t *msg);

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize the CAN driver
  * @note   Must be called after MX_FDCAN1_Init().
  *         Configures global filter (accept all into FIFO 0),
  *         starts FDCAN1, and enables Rx FIFO 0 new message interrupt.
  * @retval HAL_OK on success
  * @retval HAL_ERROR if init fails
  */
HAL_StatusTypeDef CAN_Init(void);

/**
  * @brief  Send a CAN message (blocking)
  * @note   Adds message to Tx FIFO and waits for completion.
  * @param  msg  Pointer to CAN message to send
  * @retval HAL_OK on success
  * @retval HAL_ERROR on failure (e.g. bus off, invalid args)
  * @retval HAL_TIMEOUT if TX does not complete within CAN_TX_TIMEOUT_MS
  */
HAL_StatusTypeDef CAN_Send(const CAN_Msg_t *msg);

/**
  * @brief  Send a CAN message (non-blocking, interrupt-based)
  * @note   Adds message to Tx FIFO. Returns immediately.
  *         Completion is signaled via HAL_FDCAN_TxFifoQueueCallback().
  * @param  msg  Pointer to CAN message to send (must remain valid)
  * @retval HAL_OK on success
  * @retval HAL_BUSY if Tx FIFO is full
  * @retval HAL_ERROR on failure
  */
HAL_StatusTypeDef CAN_Send_IT(const CAN_Msg_t *msg);

/**
  * @brief  Register a callback for received CAN messages
  * @param  callback  Function pointer, or NULL to unregister
  * @note   The callback is invoked from HAL_FDCAN_RxFifo0Callback()
  *         in interrupt context. Keep it short.
  */
void CAN_SetRxCallback(CAN_RxCallback_t callback);

/**
  * @brief  Get number of messages pending in Rx FIFO 0
  * @retval Number of pending messages (0 = empty)
  */
uint16_t CAN_GetRxCount(void);

/**
  * @brief  Get a received message from Rx FIFO 0 (polling)
  * @param  msg  Output pointer to receive the CAN message
  * @retval HAL_OK on success
  * @retval HAL_ERROR if FIFO is empty or msg is NULL
  */
HAL_StatusTypeDef CAN_GetRxMessage(CAN_Msg_t *msg);

/**
  * @brief  Get FDCAN error counters
  * @param  tx_err  Output: transmit error counter
  * @param  rx_err  Output: receive error counter
  * @retval HAL_OK on success
  */
HAL_StatusTypeDef CAN_GetErrorCounters(uint32_t *tx_err, uint32_t *rx_err);

/**
  * @brief  Check if CAN controller is in Bus-Off state
  * @retval 1 if bus-off, 0 otherwise
  */
uint8_t CAN_IsBusOff(void);

/**
  * @brief  Recover from Bus-Off state
  * @note   Clears the bus-off condition and restarts CAN communication.
  *         Application should wait before retrying transmission.
  * @retval HAL_OK on success
  */
HAL_StatusTypeDef CAN_RecoverBusOff(void);

/**
  * @brief  FDCAN Rx FIFO 0 new message callback
  * @note   Called by HAL_FDCAN_RxFifo0Callback() via irq_router dispatcher.
  *         Do NOT call directly.
  * @param  hfdcan  FDCAN handle
  */
void CAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan);

/**
  * @brief  FDCAN Tx FIFO/Queue complete callback
  * @note   Called by HAL_FDCAN_TxFifoQueueCallback() via irq_router dispatcher.
  *         Do NOT call directly.
  * @param  hfdcan  FDCAN handle
  */
void CAN_TxFifoQueueCallback(FDCAN_HandleTypeDef *hfdcan);

/**
  * @brief  FDCAN error/status callback
  * @note   Called by HAL_FDCAN_ErrorCallback() via irq_router dispatcher.
  *         Do NOT call directly.
  * @param  hfdcan  FDCAN handle
  */
void CAN_ErrorCallback(FDCAN_HandleTypeDef *hfdcan);

/**
  * @brief  Get the FDCAN handle used by the CAN driver
  * @retval Pointer to hfdcan1
  */
FDCAN_HandleTypeDef *CAN_GetHandle(void);

#ifdef __cplusplus
}
#endif

#endif /* __CAN_H__ */
