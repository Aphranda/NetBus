/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rs485.c
  * @brief   RS485 driver implementation -- UART8, idle-line DMA, ring buffer
  *
  *          Architecture (mirrors UART7 Log module pattern):
  *
  *            ┌─ RS485_Init()
  *            │    └─ HAL_UARTEx_ReceiveToIdle_DMA()   ← starts CIRCULAR DMA RX
  *            │         DMA buffer = g_rs485.rx_ring.buf (direct ring buffer)
  *            │    └─ __HAL_DMA_DISABLE_IT(HT)         ← disable half-transfer
  *            │
  *            ├─ UART idle → UART8_IRQHandler
  *            │    └─ HAL_UART_IRQHandler(&huart8)
  *            │         └─ HAL_UARTEx_RxEventCallback()   ← in this file
  *            │              ├─ updates ring buffer head from NDTR
  *            │              └─ invokes user callback (if registered)
  *            │
  *            ├─ RS485_Send() / RS485_Send_IT()
  *            │    ├─ Assert DE (PE3 HIGH)
  *            │    ├─ Settling delay
  *            │    ├─ HAL_UART_Transmit() / HAL_UART_Transmit_DMA()
  *            │    └─ De-assert DE (blocking: after TX; DMA: in TxCpltCallback)
  *            │
  *            └─ Application reads via RS485_Available() / RS485_Receive()
  *
  *          DMA runs continuously in CIRCULAR mode -- writes directly into
  *          g_rs485.rx_ring.buf. The ring buffer head is derived from NDTR
  *          on each IDLE event. The application reads from tail.
  *          No DMA restart is needed.
  *
  *          Reference:
  *            UART7 Log module (ThirdParty/Log/Src/log.c) uses the same
  *            idle-line + CIRCULAR DMA pattern for RX processing.
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
#include "rs485.h"
#include "main.h"       /* for UART8_DE_Pin, UART8_DE_GPIO_Port */
#include <string.h>

/* Private typedef -----------------------------------------------------------*/

/**
  * @brief  Ring buffer structure for RS485 RX
  * @note   The buffer is used directly by DMA in CIRCULAR mode.
  *         head = position where DMA has written (updated in IDLE callback
  *                via NDTR calculation)
  *         tail = position where application has read up to
  */
typedef struct {
    uint8_t  buf[RS485_RX_BUF_SIZE] __attribute__((aligned(32)));
    uint16_t head;                     /*!< Write index (DMA/IDLE cb)    */
    uint16_t tail;                     /*!< Read index (application)     */
} RS485_RingBuf_t;

/* Private define ------------------------------------------------------------*/

/**
  * @brief  Ring buffer mask (buffer size must be power of 2)
  */
#define RS485_RB_MASK   (RS485_RX_BUF_SIZE - 1U)

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/**
  * @brief  RS485 state structure
  */
static struct {
    RS485_RingBuf_t      rx_ring;           /*!< RX ring buffer (DMA target) — must be first for cache alignment */
    UART_HandleTypeDef  *huart;             /*!< UART handle (&huart8)        */
    RS485_RxCallback_t   rx_callback;       /*!< Frame received callback      */
    volatile uint8_t     tx_busy;           /*!< TX in progress flag (DMA)    */
} g_rs485 = {
    .rx_ring      = { .head = 0U, .tail = 0U },
    .huart        = NULL,
    .rx_callback  = NULL,
    .tx_busy      = 0U,
};

/* Private function prototypes -----------------------------------------------*/

/**
  * @brief  Assert DE pin (enable RS485 driver, enter TX mode)
  */
static inline void _de_assert(void)
{
    HAL_GPIO_WritePin(UART8_DE_GPIO_Port, UART8_DE_Pin, GPIO_PIN_SET);
}

/**
  * @brief  De-assert DE pin (disable RS485 driver, enter RX mode)
  */
static inline void _de_deassert(void)
{
    HAL_GPIO_WritePin(UART8_DE_GPIO_Port, UART8_DE_Pin, GPIO_PIN_RESET);
}

/* ── Ring buffer helpers ──────────────────────────────────────────────────── */

/**
  * @brief  Get number of bytes available in the ring buffer
  */
static inline uint16_t _rb_available(void)
{
    return (uint16_t)((uint16_t)(g_rs485.rx_ring.head - g_rs485.rx_ring.tail)
                      & RS485_RB_MASK);
}

/* ── TX Complete callback (HAL weak override) ─────────────────────────────── */

/**
  * @brief  UART TX complete callback -- de-asserts DE after DMA TX finishes
  * @note   Overrides the weak HAL_UART_TxCpltCallback().
  *         Only acts on UART8 (RS485). For UART7, this is unused (blocking TX).
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == g_rs485.huart)
    {
        /* De-assert DE -- back to RX mode */
        _de_deassert();
        g_rs485.tx_busy = 0U;
    }
}

/* ── RX Event callback (dispatch target) ──────────────────────────────────── */

/**
  * @brief  RS485 UART (UART8) RX Event callback
  * @note   Called by irq_router.c HAL_UARTEx_RxEventCallback() dispatcher
  *         on IDLE line detection / DMA TC events.
  *
  *         IDLE event: computes current DMA write position from NDTR,
  *         updates ring buffer head, calls user callback with available data.
  *
  *         TC event (DMA wrap): head naturally wraps -- no special handling
  *         needed since NDTR resets to BUF_SIZE.
  *
  * @param  huart  UART handle
  * @param  Size   Number of bytes transferred (unused)
  */
void RS485_UART_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    (void)Size;

    /* ── Invalidate D-Cache for DMA buffer ──────────────────────────────── */
    SCB_InvalidateDCache_by_Addr((uint32_t *)g_rs485.rx_ring.buf, RS485_RX_BUF_SIZE);

    /* ── Compute current DMA write position from NDTR ──────────────────── */
    /* NDTR decrements from RS485_RX_BUF_SIZE → 0 as DMA fills the buffer.
     * Write index = (BUF_SIZE - NDTR) gives the current write position.
     * When NDTR was 0 (TC/wrap), it reloads to BUF_SIZE → wr_idx = 0. */
    uint16_t ndtr   = (uint16_t)__HAL_DMA_GET_COUNTER(huart->hdmarx);
    uint16_t wr_idx = RS485_RX_BUF_SIZE - ndtr;
    if (wr_idx >= RS485_RX_BUF_SIZE) wr_idx = 0U;

    /* ── Handle TC (DMA wrap) events ───────────────────────────────────── */
    /* On TC, the entire buffer just wrapped. Update head to wr_idx (0).
     * The application's tail may be behind -- it will catch up naturally. */
    if (huart->RxEventType == HAL_UART_RXEVENT_TC)
    {
        g_rs485.rx_ring.head = wr_idx;
        return;
    }

    /* ── IDLE event -- update head and notify application ───────────────── */
    g_rs485.rx_ring.head = wr_idx;

    /* Invoke user callback with pointer to available data */
    if (g_rs485.rx_callback != NULL)
    {
        uint16_t avail = _rb_available();

        if (avail > 0U)
        {
            /* Provide contiguous segment from tail position */
            uint16_t cont_len = avail;
            uint16_t end = g_rs485.rx_ring.tail + avail;
            if (end > RS485_RX_BUF_SIZE)
            {
                cont_len = RS485_RX_BUF_SIZE - g_rs485.rx_ring.tail;
            }

            g_rs485.rx_callback(&g_rs485.rx_ring.buf[g_rs485.rx_ring.tail],
                                cont_len);
        }
    }

    /* DMA runs continuously in CIRCULAR mode -- no restart needed here.
     * __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT) was done in Init. */
}

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize the RS485 driver
  * @note   Must be called after MX_UART8_Init() and MX_DMA_Init().
  *         Starts DMA RX in CIRCULAR mode with IDLE line detection.
  *         DMA writes directly into g_rs485.rx_ring.buf.
  * @retval HAL_OK on success
  * @retval HAL_ERROR if init fails
  */
HAL_StatusTypeDef RS485_Init(void)
{
    /* Store UART8 handle */
    g_rs485.huart = &huart8;

    /* Reset state */
    g_rs485.rx_ring.head = 0U;
    g_rs485.rx_ring.tail = 0U;
    g_rs485.rx_callback  = NULL;
    g_rs485.tx_busy      = 0U;

    /* Ensure DE is de-asserted (RX mode by default) */
    _de_deassert();

    /* Start UART RX via DMA with IDLE line detection (CIRCULAR mode).
     * DMA continuously receives bytes into the ring buffer.
     * When the UART line goes idle (after a complete frame), the IDLE
     * interrupt fires and HAL_UARTEx_RxEventCallback() processes the
     * received bytes. DMA keeps running -- no restart needed. */
    if (HAL_UARTEx_ReceiveToIdle_DMA(g_rs485.huart,
                                     g_rs485.rx_ring.buf,
                                     RS485_RX_BUF_SIZE) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Disable DMA half-transfer interrupt -- only care about IDLE events */
    __HAL_DMA_DISABLE_IT(g_rs485.huart->hdmarx, DMA_IT_HT);

    return HAL_OK;
}

/**
  * @brief  Send data over RS485 bus (blocking)
  * @param  data  Pointer to data buffer
  * @param  len   Number of bytes to transmit
  * @retval HAL_OK on success
  */
HAL_StatusTypeDef RS485_Send(const uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef status;

    if (g_rs485.huart == NULL || data == NULL || len == 0U)
    {
        return HAL_ERROR;
    }

    /* Assert DE -- enable RS485 driver */
    _de_assert();

    /* Brief settling delay for transceiver */
    for (volatile uint32_t i = 0U; i < (uint32_t)RS485_DE_SETTLE_US * 10U; i++)
    {
        __NOP();
    }

    /* Transmit data (blocking) */
    status = HAL_UART_Transmit(g_rs485.huart, (uint8_t *)data, len, 1000U);

    /* De-assert DE -- back to RX mode */
    _de_deassert();

    return status;
}

/**
  * @brief  Send data over RS485 bus (non-blocking, DMA-based)
  * @param  data  Pointer to data buffer (must remain valid until TX completes)
  * @param  len   Number of bytes to transmit
  * @retval HAL_OK    on success
  * @retval HAL_BUSY  if a TX is already in progress
  * @retval HAL_ERROR on failure
  */
HAL_StatusTypeDef RS485_Send_IT(const uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef status;

    if (g_rs485.huart == NULL || data == NULL || len == 0U)
    {
        return HAL_ERROR;
    }

    if (g_rs485.tx_busy != 0U)
    {
        return HAL_BUSY;
    }

    /* Assert DE -- enable RS485 driver */
    _de_assert();

    /* Brief settling delay for transceiver */
    for (volatile uint32_t i = 0U; i < (uint32_t)RS485_DE_SETTLE_US * 10U; i++)
    {
        __NOP();
    }

    /* Mark TX as busy (will be cleared in TxCpltCallback) */
    g_rs485.tx_busy = 1U;

    /* Transmit via DMA (non-blocking) */
    status = HAL_UART_Transmit_DMA(g_rs485.huart, (uint8_t *)data, len);

    if (status != HAL_OK)
    {
        /* DMA start failed -- revert to RX mode */
        g_rs485.tx_busy = 0U;
        _de_deassert();
    }

    /* DE is de-asserted in HAL_UART_TxCpltCallback() when DMA completes */
    return status;
}

/**
  * @brief  Transmit data over RS485 bus (non-blocking, DMA-based)
  * @note   Alias for RS485_Send_IT(). Provided for compatibility with
  *         Modbus master/slave modules.
  * @param  data  Pointer to data buffer (must remain valid until TX completes)
  * @param  len   Number of bytes to transmit
  * @retval HAL_OK on success, HAL_BUSY if TX in progress, HAL_ERROR on failure
  */
HAL_StatusTypeDef RS485_Transmit(const uint8_t *data, uint16_t len)
{
    return RS485_Send_IT(data, len);
}

/**
  * @brief  Get number of bytes available in the RX ring buffer
  * @retval Number of bytes available to read
  */
uint16_t RS485_Available(void)
{
    uint16_t avail;

    __disable_irq();
    avail = _rb_available();
    __enable_irq();

    return avail;
}

/**
  * @brief  Read received data from the RX ring buffer
  * @param  buf      Output buffer
  * @param  max_len  Maximum bytes to read
  * @return Number of bytes actually copied
  */
uint16_t RS485_Receive(uint8_t *buf, uint16_t max_len)
{
    uint16_t cnt = 0U;

    if (buf == NULL || max_len == 0U)
    {
        return 0U;
    }

    __disable_irq();
    while (cnt < max_len && _rb_available() > 0U)
    {
        buf[cnt] = g_rs485.rx_ring.buf[g_rs485.rx_ring.tail];
        g_rs485.rx_ring.tail = (uint16_t)((g_rs485.rx_ring.tail + 1U)
                                          & RS485_RB_MASK);
        cnt++;
    }
    __enable_irq();

    return cnt;
}

/**
  * @brief  Get pointer to contiguous received data (zero-copy)
  * @param  out_len  Output: number of contiguous bytes available
  * @return Pointer to data in ring buffer, or NULL if empty
  * @note   The returned data is only valid until the next ring buffer
  *         operation. Use RS485_ReceiveAdvance() to consume after processing.
  */
uint8_t *RS485_ReceivePtr(uint16_t *out_len)
{
    uint8_t *ptr = NULL;
    uint16_t len = 0U;

    if (out_len == NULL)
    {
        return NULL;
    }

    __disable_irq();
    uint16_t avail = _rb_available();
    if (avail > 0U)
    {
        uint16_t end = g_rs485.rx_ring.tail + avail;
        if (end > RS485_RX_BUF_SIZE)
        {
            /* Data wraps -- only provide first contiguous segment */
            len = RS485_RX_BUF_SIZE - g_rs485.rx_ring.tail;
        }
        else
        {
            len = avail;
        }
        ptr = &g_rs485.rx_ring.buf[g_rs485.rx_ring.tail];
    }
    __enable_irq();

    *out_len = len;
    return ptr;
}

/**
  * @brief  Advance the read pointer after RS485_ReceivePtr()
  * @param  len  Number of bytes to consume
  */
void RS485_ReceiveAdvance(uint16_t len)
{
    __disable_irq();
    g_rs485.rx_ring.tail = (uint16_t)((g_rs485.rx_ring.tail + len)
                                      & RS485_RB_MASK);
    __enable_irq();
}

/**
  * @brief  Register a callback for received RS485 frames
  * @param  callback  Function pointer, or NULL to unregister
  */
void RS485_SetRxCallback(RS485_RxCallback_t callback)
{
    __disable_irq();
    g_rs485.rx_callback = callback;
    __enable_irq();
}

/**
  * @brief  Flush the RX ring buffer
  */
void RS485_Flush(void)
{
    __disable_irq();
    g_rs485.rx_ring.tail = g_rs485.rx_ring.head;
    __enable_irq();
}

/**
  * @brief  Get the UART handle used by RS485
  * @retval Pointer to huart8
  */
UART_HandleTypeDef *RS485_GetUartHandle(void)
{
    return g_rs485.huart;
}
