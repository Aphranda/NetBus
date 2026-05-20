/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.c
  * @brief   CAN driver implementation — FDCAN1, CAN FD mode
  *
  *          Architecture (follows RS485 module pattern):
  *
  *            ┌─ MX_FDCAN1_Init()                          ← CubeMX generated
  *            │    (configured in fdcan.c with CAN FD + BRS, 64-byte data)
  *            │
  *            ├─ CAN_Init()
  *            │    ├─ ConfigGlobalFilter(accept all → Rx FIFO 0)
  *            │    ├─ HAL_FDCAN_Start()
  *            │    └─ HAL_FDCAN_ActivateNotification(RX_FIFO0_NEW_MSG)
  *            │
  *            ├─ CAN_Send() / CAN_Send_IT()
  *            │    └─ HAL_FDCAN_AddMessageToTxFifoQ()
  *            │
  *            ├─ Rx FIFO 0 interrupt → CAN_RxFifo0Callback()
  *            │    ├─ HAL_FDCAN_GetRxMessage()
  *            │    ├─ stores to internal ring buffer (if no callback)
  *            │    └─ invokes user callback (if registered)
  *            │
  *            ├─ CAN_GetRxMessage() / CAN_GetRxCount()
  *            │    └─ Polling reads from internal ring buffer
  *            │
  *            └─ CAN_GetErrorCounters() / CAN_IsBusOff()
  *                 └─ HAL_FDCAN_GetErrorCounters() / HAL_FDCAN_GetProtocolStatus()
  *
  *          Reference:
  *            RS485 driver (ThirdParty/RS485) uses the same callback +
  *            polling dual-path pattern for RX data handling.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "can.h"
#include <string.h>

/* Private typedef -----------------------------------------------------------*/

/**
  * @brief  Ring buffer structure for CAN RX messages
  * @note   Stores fully decoded CAN_Msg_t entries received via interrupt.
  *         head = write index (interrupt context)
  *         tail = read index (application context)
  */
typedef struct {
    CAN_Msg_t buf[16U];              /*!< Message buffer                   */
    uint8_t   head;                  /*!< Write index (interrupt)          */
    uint8_t   tail;                  /*!< Read index (application)         */
} CAN_RingBuf_t;

/* Private define ------------------------------------------------------------*/

/**
  * @brief  Ring buffer mask (size must be power of 2)
  */
#define CAN_RB_MASK         (sizeof(((CAN_RingBuf_t *)0)->buf) / sizeof(CAN_Msg_t) - 1U)

/**
  * @brief  Ring buffer element count
  */
#define CAN_RB_SIZE         (CAN_RB_MASK + 1U)

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/**
  * @brief  CAN driver state structure
  */
static struct {
    FDCAN_HandleTypeDef *hfdcan;        /*!< FDCAN handle (&hfdcan1)          */
    CAN_RingBuf_t        rx_ring;       /*!< RX message ring buffer           */
    CAN_RxCallback_t     rx_callback;   /*!< RX message callback              */
    volatile uint8_t     tx_busy;       /*!< TX in progress flag (IT mode)    */
} g_can = {
    .hfdcan      = NULL,
    .rx_ring     = { .head = 0U, .tail = 0U },
    .rx_callback = NULL,
    .tx_busy     = 0U,
};

/* Private function prototypes -----------------------------------------------*/

/* ── DLC conversion utilities ─────────────────────────────────────────────── */

/**
  * @brief  Convert CAN FD DLC code to actual byte count
  * @param  dlc_code  DLC code from FDCAN header (0-15)
  * @retval Actual byte count
  */
uint8_t CAN_DLCToBytes(uint8_t dlc_code)
{
    static const uint8_t table[] = {
        0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U,
        8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U
    };

    if (dlc_code > 15U)
    {
        return 0U;
    }
    return table[dlc_code];
}

/**
  * @brief  Convert byte count to CAN FD DLC code for TX header
  * @param  bytes  Actual byte count (0-64)
  * @retval DLC code (0-15)
  */
uint8_t CAN_BytesToDLC(uint8_t bytes)
{
    if (bytes <= 8U)     return bytes;
    if (bytes <= 12U)    return 9U;
    if (bytes <= 16U)    return 10U;
    if (bytes <= 20U)    return 11U;
    if (bytes <= 24U)    return 12U;
    if (bytes <= 32U)    return 13U;
    if (bytes <= 48U)    return 14U;
    return 15U;  /* up to 64 bytes */
}

/* ── Ring buffer helpers ──────────────────────────────────────────────────── */

/**
  * @brief  Get number of messages available in the ring buffer
  */
static inline uint8_t _rb_available(void)
{
    return (uint8_t)((uint8_t)(g_can.rx_ring.head - g_can.rx_ring.tail)
                     & CAN_RB_MASK);
}

/**
  * @brief  Push a CAN message into the ring buffer (interrupt context)
  * @param  msg  Pointer to the message to enqueue
  */
static inline void _rb_push(const CAN_Msg_t *msg)
{
    uint8_t next = (uint8_t)((g_can.rx_ring.head + 1U) & CAN_RB_MASK);
    if (next != g_can.rx_ring.tail)
    {
        memcpy(&g_can.rx_ring.buf[g_can.rx_ring.head], msg, sizeof(CAN_Msg_t));
        g_can.rx_ring.head = next;
    }
    /* If buffer full, oldest message is overwritten (drop oldest) */
}

/* ── HAL FDCAN Callbacks (override weak) ──────────────────────────────────── */

/**
  * @brief  FDCAN Rx FIFO 0 new message callback
  * @note   Overrides the weak HAL_FDCAN_RxFifo0Callback().
  *         Called from HAL_FDCAN_IRQHandler() when a new message arrives
  *         in Rx FIFO 0. Retrieves the message and either stores it in
  *         the ring buffer or forwards to the user callback.
  * @param  hfdcan  FDCAN handle
  */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    (void)RxFifo0ITs;

    if (hfdcan != g_can.hfdcan)
    {
        return;
    }

    /* Retrieve the received message from Rx FIFO 0 */
    CAN_Msg_t msg;
    FDCAN_RxHeaderTypeDef rx_header;

    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rx_header, msg.data) != HAL_OK)
    {
        return;
    }

    /* Decode header: DataLength is the DLC code (0-15), convert to byte count */
    msg.id  = rx_header.Identifier & CAN_STD_ID_MASK;
    msg.dlc = CAN_DLCToBytes((uint8_t)(rx_header.DataLength & 0x0FU));

    /* Dispatch: callback → ring buffer fallback */
    if (g_can.rx_callback != NULL)
    {
        g_can.rx_callback(&msg);
    }
    else
    {
        _rb_push(&msg);
    }
}

/**
  * @brief  FDCAN Tx FIFO/Queue complete callback
  * @note   Overrides the weak HAL_FDCAN_TxFifoQueueCallback().
  *         Clears the TX busy flag for non-blocking transmit.
  * @param  hfdcan  FDCAN handle
  */
void HAL_FDCAN_TxFifoQueueCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t TxFifoQueueITs)
{
    (void)TxFifoQueueITs;

    if (hfdcan == g_can.hfdcan)
    {
        g_can.tx_busy = 0U;
    }
}

/**
  * @brief  FDCAN error/status callback
  * @note   Overrides the weak HAL_FDCAN_ErrorCallback().
  *         Reports error status — can be extended for custom error handling.
  * @param  hfdcan  FDCAN handle
  */
void HAL_FDCAN_ErrorCallback(FDCAN_HandleTypeDef *hfdcan)
{
    (void)hfdcan;
    /* Extend here for application-specific error handling / logging */
}

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize the CAN driver
  * @note   Must be called after MX_FDCAN1_Init().
  *
  *         Steps:
  *           1. Store FDCAN1 handle
  *           2. Configure global filter — accept all (standard & extended)
  *              non-matching frames into Rx FIFO 0
  *           3. Start FDCAN1 communication
  *           4. Activate Rx FIFO 0 new message interrupt
  *
  * @retval HAL_OK on success
  * @retval HAL_ERROR if any step fails
  */
HAL_StatusTypeDef CAN_Init(void)
{
    HAL_StatusTypeDef status;

    /* Store FDCAN1 handle */
    g_can.hfdcan = &hfdcan1;

    /* Reset state */
    g_can.rx_ring.head    = 0U;
    g_can.rx_ring.tail    = 0U;
    g_can.rx_callback     = NULL;
    g_can.tx_busy         = 0U;

    /* ── Configure global filter ─────────────────────────────────────────── */
    /* Accept all non-matching standard and extended IDs into Rx FIFO 0.
     * Reject all remote frames (no remote frame handling needed). */
    status = HAL_FDCAN_ConfigGlobalFilter(g_can.hfdcan,
                                          FDCAN_ACCEPT_IN_RX_FIFO0,   /* Non-matching Std       */
                                          FDCAN_ACCEPT_IN_RX_FIFO0,   /* Non-matching Ext       */
                                          FDCAN_REJECT_REMOTE,         /* Reject remote Std      */
                                          FDCAN_REJECT_REMOTE);        /* Reject remote Ext      */
    if (status != HAL_OK)
    {
        return status;
    }

    /* ── Start FDCAN communication ───────────────────────────────────────── */
    status = HAL_FDCAN_Start(g_can.hfdcan);
    if (status != HAL_OK)
    {
        return status;
    }

    /* ── Activate Rx FIFO 0 new message interrupt ────────────────────────── */
    status = HAL_FDCAN_ActivateNotification(g_can.hfdcan,
                                            FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
                                            0U);
    if (status != HAL_OK)
    {
        /* Stop FDCAN on notification activation failure */
        HAL_FDCAN_Stop(g_can.hfdcan);
        return status;
    }

    return HAL_OK;
}

/**
  * @brief  Send a CAN message (blocking)
  * @note   Constructs a Tx header and adds the message to the Tx FIFO.
  *         Polls for Tx FIFO completion (busy-wait with timeout).
  * @param  msg  Pointer to CAN message to send
  * @retval HAL_OK on success
  * @retval HAL_ERROR if msg is NULL or id is invalid
  * @retval HAL_TIMEOUT if TX does not complete within timeout
  */
HAL_StatusTypeDef CAN_Send(const CAN_Msg_t *msg)
{
    if (g_can.hfdcan == NULL || msg == NULL)
    {
        return HAL_ERROR;
    }

    if (msg->id > CAN_STD_ID_MASK)
    {
        return HAL_ERROR;
    }

    /* Decide frame format: use CAN FD for payloads > 8 bytes */
    uint8_t  is_fd     = (msg->dlc > 8U) ? 1U : 0U;
    uint8_t  dlc_code  = CAN_BytesToDLC(msg->dlc);

    /* Prepare Tx header */
    FDCAN_TxHeaderTypeDef tx_header;
    tx_header.Identifier          = msg->id & CAN_STD_ID_MASK;
    tx_header.IdType              = FDCAN_STANDARD_ID;
    tx_header.TxFrameType         = FDCAN_DATA_FRAME;
    tx_header.DataLength          = dlc_code;
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch       = is_fd ? FDCAN_BRS_ON : FDCAN_BRS_OFF;
    tx_header.FDFormat            = is_fd ? FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
    tx_header.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    tx_header.MessageMarker       = 0U;

    /* Add to Tx FIFO */
    HAL_StatusTypeDef status = HAL_FDCAN_AddMessageToTxFifoQ(g_can.hfdcan,
                                                             &tx_header,
                                                             msg->data);
    if (status != HAL_OK)
    {
        return status;
    }

    /* Wait for transmission to leave Tx FIFO (poll Tx FIFO empty) */
    uint32_t tick_start = HAL_GetTick();
    while ((HAL_FDCAN_GetTxFifoFreeLevel(g_can.hfdcan) < 1U) &&
           ((HAL_GetTick() - tick_start) < CAN_TX_TIMEOUT_MS))
    {
        /* Busy-wait */
    }

    if (HAL_FDCAN_GetTxFifoFreeLevel(g_can.hfdcan) < 1U)
    {
        return HAL_TIMEOUT;
    }

    return HAL_OK;
}

/**
  * @brief  Send a CAN message (non-blocking, interrupt-based)
  * @note   Adds message to Tx FIFO and returns immediately.
  *         Completion is signaled via HAL_FDCAN_TxFifoQueueCallback().
  *         Do NOT modify or free the msg buffer until Tx completes.
  * @param  msg  Pointer to CAN message to send (must remain valid)
  * @retval HAL_OK on success
  * @retval HAL_BUSY if a TX is already in progress
  * @retval HAL_ERROR if msg is NULL or id is invalid
  */
HAL_StatusTypeDef CAN_Send_IT(const CAN_Msg_t *msg)
{
    if (g_can.hfdcan == NULL || msg == NULL)
    {
        return HAL_ERROR;
    }

    if (msg->id > CAN_STD_ID_MASK)
    {
        return HAL_ERROR;
    }

    if (g_can.tx_busy != 0U)
    {
        return HAL_BUSY;
    }

    /* Decide frame format: use CAN FD for payloads > 8 bytes */
    uint8_t  is_fd     = (msg->dlc > 8U) ? 1U : 0U;
    uint8_t  dlc_code  = CAN_BytesToDLC(msg->dlc);

    /* Prepare Tx header */
    FDCAN_TxHeaderTypeDef tx_header;
    tx_header.Identifier          = msg->id & CAN_STD_ID_MASK;
    tx_header.IdType              = FDCAN_STANDARD_ID;
    tx_header.TxFrameType         = FDCAN_DATA_FRAME;
    tx_header.DataLength          = dlc_code;
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch       = is_fd ? FDCAN_BRS_ON : FDCAN_BRS_OFF;
    tx_header.FDFormat            = is_fd ? FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
    tx_header.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    tx_header.MessageMarker       = 0U;

    /* Mark TX busy */
    g_can.tx_busy = 1U;

    /* Add to Tx FIFO */
    HAL_StatusTypeDef status = HAL_FDCAN_AddMessageToTxFifoQ(g_can.hfdcan,
                                                             &tx_header,
                                                             msg->data);
    if (status != HAL_OK)
    {
        g_can.tx_busy = 0U;
    }

    /* tx_busy cleared in HAL_FDCAN_TxFifoQueueCallback() on completion */
    return status;
}

/**
  * @brief  Register a callback for received CAN messages
  * @param  callback  Function pointer, or NULL to unregister
  * @note   The callback is invoked from HAL_FDCAN_RxFifo0Callback()
  *         in interrupt context. Keep it short (e.g. set a flag, push
  *         to a queue). Do NOT call CAN_Send() from within the callback.
  */
void CAN_SetRxCallback(CAN_RxCallback_t callback)
{
    __disable_irq();
    g_can.rx_callback = callback;
    __enable_irq();
}

/**
  * @brief  Get number of messages pending in Rx FIFO 0
  * @retval Number of pending messages (0 = empty)
  */
uint16_t CAN_GetRxCount(void)
{
    uint16_t count;

    __disable_irq();
    count = (uint16_t)_rb_available();
    __enable_irq();

    return count;
}

/**
  * @brief  Get a received message from Rx FIFO 0 (polling)
  * @note   Retrieves the oldest message from the internal ring buffer.
  *         This is the polling alternative to using a callback.
  * @param  msg  Output pointer to receive the CAN message
  * @retval HAL_OK on success
  * @retval HAL_ERROR if FIFO is empty or msg is NULL
  */
HAL_StatusTypeDef CAN_GetRxMessage(CAN_Msg_t *msg)
{
    if (msg == NULL)
    {
        return HAL_ERROR;
    }

    __disable_irq();

    if (_rb_available() == 0U)
    {
        __enable_irq();
        return HAL_ERROR;
    }

    memcpy(msg, &g_can.rx_ring.buf[g_can.rx_ring.tail], sizeof(CAN_Msg_t));
    g_can.rx_ring.tail = (uint8_t)((g_can.rx_ring.tail + 1U) & CAN_RB_MASK);

    __enable_irq();

    return HAL_OK;
}

/**
  * @brief  Get FDCAN error counters
  * @param  tx_err  Output: transmit error counter
  * @param  rx_err  Output: receive error counter
  * @retval HAL_OK on success
  */
HAL_StatusTypeDef CAN_GetErrorCounters(uint32_t *tx_err, uint32_t *rx_err)
{
    if (g_can.hfdcan == NULL || tx_err == NULL || rx_err == NULL)
    {
        return HAL_ERROR;
    }

    FDCAN_ErrorCountersTypeDef counters;
    HAL_StatusTypeDef status = HAL_FDCAN_GetErrorCounters(g_can.hfdcan, &counters);

    if (status == HAL_OK)
    {
        *tx_err = counters.TxErrorCnt;
        *rx_err = counters.RxErrorCnt;
    }

    return status;
}

/**
  * @brief  Check if CAN controller is in Bus-Off state
  * @retval 1 if bus-off, 0 otherwise
  */
uint8_t CAN_IsBusOff(void)
{
    if (g_can.hfdcan == NULL)
    {
        return 0U;
    }

    FDCAN_ProtocolStatusTypeDef status;
    if (HAL_FDCAN_GetProtocolStatus(g_can.hfdcan, &status) != HAL_OK)
    {
        return 0U;
    }

    return (status.BusOff != 0U) ? 1U : 0U;
}

/**
  * @brief  Recover from Bus-Off state
  * @note   Clears the bus-off condition by performing a reset sequence:
  *         1. Stop FDCAN
  *         2. Clear protocol status
  *         3. Restart FDCAN
  *         Application should wait before retrying transmission.
  * @retval HAL_OK on success
  */
HAL_StatusTypeDef CAN_RecoverBusOff(void)
{
    if (g_can.hfdcan == NULL)
    {
        return HAL_ERROR;
    }

    HAL_StatusTypeDef status;

    /* Stop FDCAN */
    status = HAL_FDCAN_Stop(g_can.hfdcan);
    if (status != HAL_OK)
    {
        return status;
    }

    /* Small delay for bus recovery */
    HAL_Delay(10U);

    /* Restart FDCAN */
    status = HAL_FDCAN_Start(g_can.hfdcan);
    if (status != HAL_OK)
    {
        return status;
    }

    /* Re-activate notifications */
    status = HAL_FDCAN_ActivateNotification(g_can.hfdcan,
                                            FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
                                            0U);
    return status;
}

/**
  * @brief  Get the FDCAN handle used by the CAN driver
  * @retval Pointer to hfdcan1, or NULL if not initialized
  */
FDCAN_HandleTypeDef *CAN_GetHandle(void)
{
    return g_can.hfdcan;
}
