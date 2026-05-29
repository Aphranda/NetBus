/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    modbus.c
  * @brief   Modbus RTU Master implementation
  *
  *          Architecture (transaction flow):
  *
  *            Modbus_ReadHoldingRegisters(slave, reg, qty, dest)
  *              │
  *              ├─ 1. Validate parameters
  *              ├─ 2. Build request frame in g_tx_buf[]
  *              │      [slave | FC=03 | reg_H | reg_L | qty_H | qty_L | CRC_LO | CRC_HI]
  *              ├─ 3. Call _modbus_transaction(tx_buf, 8, resp_buf, &resp_len)
  *              │      │
  *              │      ├─ Flush RS485 RX ring buffer (discard stale data)
  *              │      ├─ RS485_Send(tx_buf, tx_len)  ← blocking TX
  *              │      ├─ Poll RS485_Available() with timeout
  *              │      │    └─ On data: read into resp_buf
  *              │      ├─ Validate slave address, function code, CRC
  *              │      └─ Return result
  *              │
  *              ├─ 4. Parse response data into dest[] (big-endian → uint16_t)
  *              └─ Return Modbus_Result_t
  *
  *          Error Handling:
  *            - Timeout:     Slave didn't respond within g_timeout_ms
  *            - CRC error:   Response received but CRC doesn't match
  *            - Exception:   Slave returned FC | 0x80 with exception code
  *            - Frame error: Response too short, bad length, wrong format
  *
  *          Note on poll interval:
  *            The main poll loop calls RS485_Available() repeatedly.
  *            To avoid a tight spin-lock on slow busses, the loop
  *            includes a short delay (~100µs) between polls.
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
#include "modbus.h"
#include "rs485.h"
#include <string.h>

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/**
  * @brief Size of the internal TX frame buffer
  *        Must be large enough for the largest request frame:
  *          Write Multiple Registers: 1 + 1 + 2 + 2 + 1 + (125*2) + 2 = 259
  *        We use the same maximum as the config.
  */
#define MODBUS_TX_BUF_SIZE      MODBUS_MAX_FRAME_LEN

/**
  * @brief Size of the internal RX response buffer
  */
#define MODBUS_RX_BUF_SIZE      MODBUS_MAX_FRAME_LEN

/**
  * @brief Microsecond delay between polling iterations
  *        ~100µs @ 400 MHz (STM32H7) = ~40000 CPU cycles via simple loop.
  *        This prevents a tight spin-lock while still being responsive
  *        enough for high baud rates.
  */
#define MODBUS_POLL_DELAY_US    100U

/* Private macro -------------------------------------------------------------*/

/**
  * @brief  Get the current system tick (milliseconds)
  */
#define TICK_NOW()              HAL_GetTick()

/* Private variables ---------------------------------------------------------*/

/**
  * @brief Modbus master state structure
  */
static struct {
    uint32_t    timeout_ms;         /*!< Response timeout in ms              */
    volatile uint8_t  busy;         /*!< Transaction in progress flag        */
} g_modbus = {
    .timeout_ms = MODBUS_DEFAULT_TIMEOUT_MS,
    .busy       = 0U,
};

/**
  * @brief Internal TX frame buffer (static to avoid large stack usage)
  */
static uint8_t g_tx_buf[MODBUS_TX_BUF_SIZE];

/**
  * @brief Internal RX response buffer (static to avoid large stack usage)
  */
static uint8_t g_rx_buf[MODBUS_RX_BUF_SIZE];

/* Private function prototypes -----------------------------------------------*/

static Modbus_Result_t _modbus_transaction(const uint8_t *req, uint16_t req_len,
                                           uint8_t *resp, uint16_t *resp_len);

static Modbus_Result_t _modbus_validate_response(uint8_t slave, uint8_t fc,
                                                  const uint8_t *resp,
                                                  uint16_t resp_len);

static void _modbus_delay_us(volatile uint32_t us);

/* ── Public API ──────────────────────────────────────────────────────────── */

/**
  * @brief  Initialize the Modbus RTU Master module
  * @note   Must be called after RS485_Init().
  *         Sets default timeout and clears transaction state.
  */
void Modbus_Init(void)
{
    g_modbus.timeout_ms = MODBUS_DEFAULT_TIMEOUT_MS;
    g_modbus.busy       = 0U;
}

/**
  * @brief  Set the response timeout for Modbus transactions
  * @param  timeout_ms  Timeout in milliseconds
  */
void Modbus_SetTimeout(uint32_t timeout_ms)
{
    if (timeout_ms == 0U)
    {
        timeout_ms = 1U;  /* Minimum 1ms timeout */
    }
    g_modbus.timeout_ms = timeout_ms;
}

/**
  * @brief  Get the current response timeout value
  * @return Timeout in milliseconds
  */
uint32_t Modbus_GetTimeout(void)
{
    return g_modbus.timeout_ms;
}

/**
  * @brief  Check if a Modbus transaction is currently pending
  * @retval 1 if busy, 0 otherwise
  */
uint8_t Modbus_IsTransactionPending(void)
{
    return g_modbus.busy;
}

/* ── Read Functions ──────────────────────────────────────────────────────── */

/**
  * @brief  Read Holding Registers (FC = 0x03)
  */
Modbus_Result_t Modbus_ReadHoldingRegisters(uint8_t slave, uint16_t reg,
                                            uint16_t qty, uint16_t *dest)
{
    Modbus_Result_t result;
    uint16_t resp_len;
    uint16_t i;

    /* ── Parameter validation ──────────────────────────────────────── */
    if (dest == NULL || qty == 0U || qty > MODBUS_MAX_REGISTERS)
    {
        result.status   = MODBUS_ERR_PARAM;
        result.exc_code = 0U;
        return result;
    }
    if (slave < 1U || slave > 247U)
    {
        result.status   = MODBUS_ERR_PARAM;
        result.exc_code = 0U;
        return result;
    }

    /* ── Build request frame ────────────────────────────────────────── */
    g_tx_buf[0] = slave;
    g_tx_buf[1] = MODBUS_FC_READ_HOLDING_REGISTERS;
    g_tx_buf[2] = (uint8_t)((reg >> 8) & 0xFFU);   /* Register address high byte */
    g_tx_buf[3] = (uint8_t)(reg & 0xFFU);           /* Register address low byte  */
    g_tx_buf[4] = (uint8_t)((qty >> 8) & 0xFFU);    /* Quantity high byte         */
    g_tx_buf[5] = (uint8_t)(qty & 0xFFU);            /* Quantity low byte          */

    /* Append CRC16 (little-endian) */
    uint16_t crc = Modbus_CRC16(g_tx_buf, 6U);
    g_tx_buf[6] = (uint8_t)(crc & 0xFFU);       /* CRC low byte  */
    g_tx_buf[7] = (uint8_t)((crc >> 8) & 0xFFU); /* CRC high byte */

    /* ── Execute transaction ────────────────────────────────────────── */
    resp_len = MODBUS_RX_BUF_SIZE;
    result = _modbus_transaction(g_tx_buf, 8U, g_rx_buf, &resp_len);

    /* ── Parse response ─────────────────────────────────────────────── */
    if (result.status == MODBUS_OK)
    {
        /* Parse byte count: g_rx_buf[0]=slave, [1]=FC, [2]=byte_count */
        uint8_t byte_count = g_rx_buf[2];

        if (byte_count != (uint8_t)(qty * 2U))
        {
            result.status   = MODBUS_ERR_FRAME;
            result.exc_code = 0U;
            return result;
        }

        /* Convert big-endian register data to host uint16_t */
        for (i = 0U; i < qty; i++)
        {
            uint16_t hi = (uint16_t)g_rx_buf[3U + i * 2U];
            uint16_t lo = (uint16_t)g_rx_buf[3U + i * 2U + 1U];
            dest[i] = (uint16_t)((hi << 8U) | lo);
        }
    }

    return result;
}

/**
  * @brief  Read Input Registers (FC = 0x04)
  */
Modbus_Result_t Modbus_ReadInputRegisters(uint8_t slave, uint16_t reg,
                                          uint16_t qty, uint16_t *dest)
{
    Modbus_Result_t result;
    uint16_t resp_len;
    uint16_t i;

    /* ── Parameter validation ──────────────────────────────────────── */
    if (dest == NULL || qty == 0U || qty > MODBUS_MAX_REGISTERS)
    {
        result.status   = MODBUS_ERR_PARAM;
        result.exc_code = 0U;
        return result;
    }
    if (slave < 1U || slave > 247U)
    {
        result.status   = MODBUS_ERR_PARAM;
        result.exc_code = 0U;
        return result;
    }

    /* ── Build request frame ────────────────────────────────────────── */
    g_tx_buf[0] = slave;
    g_tx_buf[1] = MODBUS_FC_READ_INPUT_REGISTERS;
    g_tx_buf[2] = (uint8_t)((reg >> 8) & 0xFFU);
    g_tx_buf[3] = (uint8_t)(reg & 0xFFU);
    g_tx_buf[4] = (uint8_t)((qty >> 8) & 0xFFU);
    g_tx_buf[5] = (uint8_t)(qty & 0xFFU);

    uint16_t crc = Modbus_CRC16(g_tx_buf, 6U);
    g_tx_buf[6] = (uint8_t)(crc & 0xFFU);
    g_tx_buf[7] = (uint8_t)((crc >> 8) & 0xFFU);

    /* ── Execute transaction ────────────────────────────────────────── */
    resp_len = MODBUS_RX_BUF_SIZE;
    result = _modbus_transaction(g_tx_buf, 8U, g_rx_buf, &resp_len);

    /* ── Parse response ─────────────────────────────────────────────── */
    if (result.status == MODBUS_OK)
    {
        uint8_t byte_count = g_rx_buf[2];

        if (byte_count != (uint8_t)(qty * 2U))
        {
            result.status   = MODBUS_ERR_FRAME;
            result.exc_code = 0U;
            return result;
        }

        for (i = 0U; i < qty; i++)
        {
            uint16_t hi = (uint16_t)g_rx_buf[3U + i * 2U];
            uint16_t lo = (uint16_t)g_rx_buf[3U + i * 2U + 1U];
            dest[i] = (uint16_t)((hi << 8U) | lo);
        }
    }

    return result;
}

/**
  * @brief  Read Coils (FC = 0x01)
  */
Modbus_Result_t Modbus_ReadCoils(uint8_t slave, uint16_t addr,
                                 uint16_t qty, uint8_t *dest)
{
    Modbus_Result_t result;
    uint16_t resp_len;

    /* ── Parameter validation ──────────────────────────────────────── */
    if (dest == NULL || qty == 0U || qty > MODBUS_MAX_COILS)
    {
        result.status   = MODBUS_ERR_PARAM;
        result.exc_code = 0U;
        return result;
    }
    if (slave < 1U || slave > 247U)
    {
        result.status   = MODBUS_ERR_PARAM;
        result.exc_code = 0U;
        return result;
    }

    /* ── Build request frame ────────────────────────────────────────── */
    g_tx_buf[0] = slave;
    g_tx_buf[1] = MODBUS_FC_READ_COILS;
    g_tx_buf[2] = (uint8_t)((addr >> 8) & 0xFFU);
    g_tx_buf[3] = (uint8_t)(addr & 0xFFU);
    g_tx_buf[4] = (uint8_t)((qty >> 8) & 0xFFU);
    g_tx_buf[5] = (uint8_t)(qty & 0xFFU);

    uint16_t crc = Modbus_CRC16(g_tx_buf, 6U);
    g_tx_buf[6] = (uint8_t)(crc & 0xFFU);
    g_tx_buf[7] = (uint8_t)((crc >> 8) & 0xFFU);

    /* ── Execute transaction ────────────────────────────────────────── */
    resp_len = MODBUS_RX_BUF_SIZE;
    result = _modbus_transaction(g_tx_buf, 8U, g_rx_buf, &resp_len);

    /* ── Parse response ─────────────────────────────────────────────── */
    if (result.status == MODBUS_OK)
    {
        uint8_t byte_count = g_rx_buf[2];
        uint16_t expected_bytes = (uint16_t)((qty + 7U) / 8U);

        if (byte_count < 1U || (uint16_t)byte_count > expected_bytes)
        {
            result.status   = MODBUS_ERR_FRAME;
            result.exc_code = 0U;
            return result;
        }

        /* Copy coil data */
        (void)memcpy(dest, &g_rx_buf[3], (size_t)byte_count);
    }

    return result;
}

/**
  * @brief  Read Discrete Inputs (FC = 0x02)
  */
Modbus_Result_t Modbus_ReadDiscreteInputs(uint8_t slave, uint16_t addr,
                                          uint16_t qty, uint8_t *dest)
{
    Modbus_Result_t result;
    uint16_t resp_len;

    /* ── Parameter validation ──────────────────────────────────────── */
    if (dest == NULL || qty == 0U || qty > MODBUS_MAX_COILS)
    {
        result.status   = MODBUS_ERR_PARAM;
        result.exc_code = 0U;
        return result;
    }
    if (slave < 1U || slave > 247U)
    {
        result.status   = MODBUS_ERR_PARAM;
        result.exc_code = 0U;
        return result;
    }

    /* ── Build request frame ────────────────────────────────────────── */
    g_tx_buf[0] = slave;
    g_tx_buf[1] = MODBUS_FC_READ_DISCRETE_INPUTS;
    g_tx_buf[2] = (uint8_t)((addr >> 8) & 0xFFU);
    g_tx_buf[3] = (uint8_t)(addr & 0xFFU);
    g_tx_buf[4] = (uint8_t)((qty >> 8) & 0xFFU);
    g_tx_buf[5] = (uint8_t)(qty & 0xFFU);

    uint16_t crc = Modbus_CRC16(g_tx_buf, 6U);
    g_tx_buf[6] = (uint8_t)(crc & 0xFFU);
    g_tx_buf[7] = (uint8_t)((crc >> 8) & 0xFFU);

    /* ── Execute transaction ────────────────────────────────────────── */
    resp_len = MODBUS_RX_BUF_SIZE;
    result = _modbus_transaction(g_tx_buf, 8U, g_rx_buf, &resp_len);

    /* ── Parse response ─────────────────────────────────────────────── */
    if (result.status == MODBUS_OK)
    {
        uint8_t byte_count = g_rx_buf[2];
        uint16_t expected_bytes = (uint16_t)((qty + 7U) / 8U);

        if (byte_count < 1U || (uint16_t)byte_count > expected_bytes)
        {
            result.status   = MODBUS_ERR_FRAME;
            result.exc_code = 0U;
            return result;
        }

        (void)memcpy(dest, &g_rx_buf[3], (size_t)byte_count);
    }

    return result;
}

/* ── Write Single Functions ─────────────────────────────────────────────── */

/**
  * @brief  Write Single Coil (FC = 0x05)
  */
Modbus_Result_t Modbus_WriteSingleCoil(uint8_t slave, uint16_t addr, uint8_t value)
{
    Modbus_Result_t result;
    uint16_t resp_len;

    /* ── Parameter validation ──────────────────────────────────────── */
    if (slave < 1U || slave > 247U)
    {
        result.status   = MODBUS_ERR_PARAM;
        result.exc_code = 0U;
        return result;
    }

    /* ── Build request frame ────────────────────────────────────────── */
    g_tx_buf[0] = slave;
    g_tx_buf[1] = MODBUS_FC_WRITE_SINGLE_COIL;
    g_tx_buf[2] = (uint8_t)((addr >> 8) & 0xFFU);
    g_tx_buf[3] = (uint8_t)(addr & 0xFFU);
    /* Coil value: 0x0000 = OFF, 0xFF00 = ON */
    g_tx_buf[4] = (value != 0U) ? 0xFFU : 0x00U;
    g_tx_buf[5] = 0x00U;

    uint16_t crc = Modbus_CRC16(g_tx_buf, 6U);
    g_tx_buf[6] = (uint8_t)(crc & 0xFFU);
    g_tx_buf[7] = (uint8_t)((crc >> 8) & 0xFFU);

    /* ── Execute transaction ────────────────────────────────────────── */
    resp_len = MODBUS_RX_BUF_SIZE;
    result = _modbus_transaction(g_tx_buf, 8U, g_rx_buf, &resp_len);

    /* ── Validate echo response ─────────────────────────────────────── */
    if (result.status == MODBUS_OK)
    {
        /* FC=05 response echoes the request: slave, FC, addr_H, addr_L, val_H, val_L, CRC */
        if (resp_len < 8U)
        {
            result.status   = MODBUS_ERR_FRAME;
            result.exc_code = 0U;
        }
    }

    return result;
}

/**
  * @brief  Write Single Register (FC = 0x06)
  */
Modbus_Result_t Modbus_WriteSingleRegister(uint8_t slave, uint16_t reg, uint16_t value)
{
    Modbus_Result_t result;
    uint16_t resp_len;

    /* ── Parameter validation ──────────────────────────────────────── */
    if (slave < 1U || slave > 247U)
    {
        result.status   = MODBUS_ERR_PARAM;
        result.exc_code = 0U;
        return result;
    }

    /* ── Build request frame ────────────────────────────────────────── */
    g_tx_buf[0] = slave;
    g_tx_buf[1] = MODBUS_FC_WRITE_SINGLE_REGISTER;
    g_tx_buf[2] = (uint8_t)((reg >> 8) & 0xFFU);
    g_tx_buf[3] = (uint8_t)(reg & 0xFFU);
    g_tx_buf[4] = (uint8_t)((value >> 8) & 0xFFU);
    g_tx_buf[5] = (uint8_t)(value & 0xFFU);

    uint16_t crc = Modbus_CRC16(g_tx_buf, 6U);
    g_tx_buf[6] = (uint8_t)(crc & 0xFFU);
    g_tx_buf[7] = (uint8_t)((crc >> 8) & 0xFFU);

    /* ── Execute transaction ────────────────────────────────────────── */
    resp_len = MODBUS_RX_BUF_SIZE;
    result = _modbus_transaction(g_tx_buf, 8U, g_rx_buf, &resp_len);

    /* ── Validate echo response ─────────────────────────────────────── */
    if (result.status == MODBUS_OK)
    {
        /* FC=06 response echoes the request: slave, FC, reg_H, reg_L, val_H, val_L, CRC */
        if (resp_len < 8U)
        {
            result.status   = MODBUS_ERR_FRAME;
            result.exc_code = 0U;
        }
    }

    return result;
}

/* ── Write Multiple Functions ───────────────────────────────────────────── */

/**
  * @brief  Write Multiple Registers (FC = 0x10)
  */
Modbus_Result_t Modbus_WriteMultipleRegisters(uint8_t slave, uint16_t reg,
                                              uint16_t qty, const uint16_t *data)
{
    Modbus_Result_t result;
    uint16_t resp_len;
    uint16_t i;

    /* ── Parameter validation ──────────────────────────────────────── */
    if (data == NULL || qty == 0U || qty > MODBUS_MAX_REGISTERS)
    {
        result.status   = MODBUS_ERR_PARAM;
        result.exc_code = 0U;
        return result;
    }
    if (slave < 1U || slave > 247U)
    {
        result.status   = MODBUS_ERR_PARAM;
        result.exc_code = 0U;
        return result;
    }

    /* ── Build request frame ────────────────────────────────────────── */
    uint16_t byte_count = (uint16_t)(qty * 2U);
    uint16_t idx = 0U;

    g_tx_buf[idx++] = slave;
    g_tx_buf[idx++] = MODBUS_FC_WRITE_MULTIPLE_REGISTERS;
    g_tx_buf[idx++] = (uint8_t)((reg >> 8) & 0xFFU);
    g_tx_buf[idx++] = (uint8_t)(reg & 0xFFU);
    g_tx_buf[idx++] = (uint8_t)((qty >> 8) & 0xFFU);
    g_tx_buf[idx++] = (uint8_t)(qty & 0xFFU);
    g_tx_buf[idx++] = (uint8_t)byte_count;

    /* Copy register data in big-endian order */
    for (i = 0U; i < qty; i++)
    {
        g_tx_buf[idx++] = (uint8_t)((data[i] >> 8) & 0xFFU);
        g_tx_buf[idx++] = (uint8_t)(data[i] & 0xFFU);
    }

    /* Append CRC16 */
    uint16_t crc = Modbus_CRC16(g_tx_buf, idx);
    g_tx_buf[idx++] = (uint8_t)(crc & 0xFFU);
    g_tx_buf[idx++] = (uint8_t)((crc >> 8) & 0xFFU);

    /* ── Execute transaction ────────────────────────────────────────── */
    resp_len = MODBUS_RX_BUF_SIZE;
    result = _modbus_transaction(g_tx_buf, idx, g_rx_buf, &resp_len);

    /* ── Validate response ──────────────────────────────────────────── */
    if (result.status == MODBUS_OK)
    {
        /* FC=10 response: slave, FC, reg_H, reg_L, qty_H, qty_L, CRC → 8 bytes */
        if (resp_len < 8U)
        {
            result.status   = MODBUS_ERR_FRAME;
            result.exc_code = 0U;
        }
        else
        {
            /* Verify the echoed quantity matches */
            uint16_t echo_qty = ((uint16_t)g_rx_buf[4] << 8U) | (uint16_t)g_rx_buf[5];
            if (echo_qty != qty)
            {
                result.status   = MODBUS_ERR_FRAME;
                result.exc_code = 0U;
            }
        }
    }

    return result;
}

/**
  * @brief  Write Multiple Coils (FC = 0x0F)
  */
Modbus_Result_t Modbus_WriteMultipleCoils(uint8_t slave, uint16_t addr,
                                          uint16_t qty, const uint8_t *data)
{
    Modbus_Result_t result;
    uint16_t resp_len;
    uint16_t i;

    /* ── Parameter validation ──────────────────────────────────────── */
    if (data == NULL || qty == 0U || qty > MODBUS_MAX_COILS)
    {
        result.status   = MODBUS_ERR_PARAM;
        result.exc_code = 0U;
        return result;
    }
    if (slave < 1U || slave > 247U)
    {
        result.status   = MODBUS_ERR_PARAM;
        result.exc_code = 0U;
        return result;
    }

    /* ── Build request frame ────────────────────────────────────────── */
    uint16_t byte_count = (uint16_t)((qty + 7U) / 8U);
    uint16_t idx = 0U;

    g_tx_buf[idx++] = slave;
    g_tx_buf[idx++] = MODBUS_FC_WRITE_MULTIPLE_COILS;
    g_tx_buf[idx++] = (uint8_t)((addr >> 8) & 0xFFU);
    g_tx_buf[idx++] = (uint8_t)(addr & 0xFFU);
    g_tx_buf[idx++] = (uint8_t)((qty >> 8) & 0xFFU);
    g_tx_buf[idx++] = (uint8_t)(qty & 0xFFU);
    g_tx_buf[idx++] = (uint8_t)byte_count;

    /* Copy coil data */
    for (i = 0U; i < byte_count; i++)
    {
        g_tx_buf[idx++] = data[i];
    }

    /* Append CRC16 */
    uint16_t crc = Modbus_CRC16(g_tx_buf, idx);
    g_tx_buf[idx++] = (uint8_t)(crc & 0xFFU);
    g_tx_buf[idx++] = (uint8_t)((crc >> 8) & 0xFFU);

    /* ── Execute transaction ────────────────────────────────────────── */
    resp_len = MODBUS_RX_BUF_SIZE;
    result = _modbus_transaction(g_tx_buf, idx, g_rx_buf, &resp_len);

    /* ── Validate response ──────────────────────────────────────────── */
    if (result.status == MODBUS_OK)
    {
        /* FC=0F response: slave, FC, addr_H, addr_L, qty_H, qty_L, CRC → 8 bytes */
        if (resp_len < 8U)
        {
            result.status   = MODBUS_ERR_FRAME;
            result.exc_code = 0U;
        }
        else
        {
            /* Verify the echoed quantity matches */
            uint16_t echo_qty = ((uint16_t)g_rx_buf[4] << 8U) | (uint16_t)g_rx_buf[5];
            if (echo_qty != qty)
            {
                result.status   = MODBUS_ERR_FRAME;
                result.exc_code = 0U;
            }
        }
    }

    return result;
}

/* ── Low-Level API ──────────────────────────────────────────────────────── */

/**
  * @brief  Send a raw Modbus frame and wait for response
  * @note   Flushes RX buffer before sending. Polls for response with
  *         configured timeout.
  */
Modbus_Result_t Modbus_SendRaw(const uint8_t *req, uint16_t req_len,
                               uint8_t *resp, uint16_t *resp_len)
{
    if (req == NULL || req_len == 0U || resp == NULL || resp_len == NULL)
    {
        Modbus_Result_t err;
        err.status   = MODBUS_ERR_PARAM;
        err.exc_code = 0U;
        return err;
    }

    if (*resp_len == 0U)
    {
        Modbus_Result_t err;
        err.status   = MODBUS_ERR_PARAM;
        err.exc_code = 0U;
        return err;
    }

    return _modbus_transaction(req, req_len, resp, resp_len);
}

/* ── Utility Functions ──────────────────────────────────────────────────── */

/**
  * @brief  Compute Modbus RTU CRC-16
  * @note   Polynomial: x^16 + x^15 + x^2 + 1 (0xA001 reflected)
  */
uint16_t Modbus_CRC16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;

    for (uint16_t i = 0U; i < len; i++)
    {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0U; j < 8U; j++)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = (crc >> 1U) ^ 0xA001U;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

/**
  * @brief  Get status string
  */
const char *Modbus_StatusString(Modbus_Status_t status)
{
    switch (status)
    {
        case MODBUS_OK:          return "OK";
        case MODBUS_ERR_TIMEOUT: return "Timeout (no response)";
        case MODBUS_ERR_CRC:     return "CRC error";
        case MODBUS_ERR_SLAVE:   return "Slave address mismatch";
        case MODBUS_ERR_FRAME:   return "Frame format error";
        case MODBUS_ERR_EXCEPTION: return "Slave exception";
        case MODBUS_ERR_BUSY:    return "Busy (transaction in progress)";
        case MODBUS_ERR_PARAM:   return "Invalid parameter";
        case MODBUS_ERR_NODATA:  return "No data received";
        default:                 return "Unknown status";
    }
}

/**
  * @brief  Get exception string
  */
const char *Modbus_ExceptionString(uint8_t exc_code)
{
    switch (exc_code)
    {
        case MODBUS_EXC_ILLEGAL_FUNCTION:      return "Illegal function";
        case MODBUS_EXC_ILLEGAL_DATA_ADDRESS:  return "Illegal data address";
        case MODBUS_EXC_ILLEGAL_DATA_VALUE:    return "Illegal data value";
        case MODBUS_EXC_SLAVE_DEVICE_FAILURE:  return "Slave device failure";
        case MODBUS_EXC_ACKNOWLEDGE:           return "Acknowledge (long task)";
        case MODBUS_EXC_SLAVE_DEVICE_BUSY:     return "Slave device busy";
        case MODBUS_EXC_NEGATIVE_ACKNOWLEDGE:  return "Negative acknowledge";
        case MODBUS_EXC_MEMORY_PARITY_ERROR:   return "Memory parity error";
        case MODBUS_EXC_GATEWAY_PATH_UNAVAIL:  return "Gateway path unavailable";
        case MODBUS_EXC_GATEWAY_TARGET_FAILED: return "Gateway target failed";
        default:                               return "Unknown exception";
    }
}

/* ── Private Functions ──────────────────────────────────────────────────── */

/**
  * @brief  Execute a complete Modbus transaction
  *
  *         Flow:
  *           1. Check if already busy (reentrancy guard)
  *           2. Flush RS485 RX buffer (discard stale data)
  *           3. Send request via RS485_Send() (blocking TX)
  *           4. Poll RS485_Available() with timeout
  *           5. Read response into resp buffer
  *           6. Validate response (slave address, function code, CRC)
  *           7. Set busy = 0 and return
  *
  * @param  req      Request frame (including CRC)
  * @param  req_len  Request length
  * @param  resp     Response buffer (output)
  * @param  resp_len In: max size, Out: actual size
  * @return Modbus_Result_t
  */
static Modbus_Result_t _modbus_transaction(const uint8_t *req, uint16_t req_len,
                                           uint8_t *resp, uint16_t *resp_len)
{
    Modbus_Result_t result;
    uint32_t timeout;
    uint16_t rx_len = 0U;
    uint16_t first_pass_len = 0U;
    uint32_t last_byte_tick;

    /* ── Reentrancy guard ──────────────────────────────────────────── */
    if (g_modbus.busy != 0U)
    {
        result.status   = MODBUS_ERR_BUSY;
        result.exc_code = 0U;
        return result;
    }
    g_modbus.busy = 1U;

    /* ── Flush any stale RX data ───────────────────────────────────── */
    RS485_Flush();

    /* ── Send request (blocking) ───────────────────────────────────── */
    if (RS485_Send(req, req_len) != HAL_OK)
    {
        g_modbus.busy = 0U;
        result.status   = MODBUS_ERR_BUSY;
        result.exc_code = 0U;
        return result;
    }

    /* ── Wait for response with timeout ────────────────────────────── */
    timeout = TICK_NOW() + g_modbus.timeout_ms;

    while (TICK_NOW() < timeout)
    {
        uint16_t avail = RS485_Available();

        if (avail > 0U)
        {
            /* Read available data */
            uint16_t to_read = avail;
            if ((rx_len + to_read) > *resp_len)
            {
                to_read = *resp_len - rx_len;
            }

            uint16_t n = RS485_Receive(&resp[rx_len], to_read);
            rx_len += n;

            /* Record the tick when we received new data */
            last_byte_tick = TICK_NOW();

            /* After first batch, give a short window for more bytes to
             * arrive (handles the case where the IDLE callback fires
             * before all bytes of a frame have been received).
             * Wait up to MODBUS_CHAR_TIMEOUT_MS for additional data. */
            if (first_pass_len == 0U)
            {
                first_pass_len = rx_len;
                /* Extend timeout to allow inter-character window */
                timeout = last_byte_tick + MODBUS_CHAR_TIMEOUT_MS;
            }
        }
        else if (first_pass_len > 0U)
        {
            /* No new data, and we've already received some.
             * Check if we've waited long enough for more bytes. */
            if ((TICK_NOW() - last_byte_tick) >= MODBUS_CHAR_TIMEOUT_MS)
            {
                /* Frame assembly complete -- exit poll loop */
                break;
            }
        }

        /* Brief delay to avoid tight spin-lock */
        if (first_pass_len == 0U)
        {
            /* Before first byte: use full delay */
            _modbus_delay_us(MODBUS_POLL_DELAY_US);
        }
        else
        {
            /* After first byte: shorter delay for faster response */
            _modbus_delay_us(MODBUS_POLL_DELAY_US / 2U);
        }
    }

    /* ── Update actual response length ─────────────────────────────── */
    *resp_len = rx_len;

    /* ── Check for timeout (no data received) ──────────────────────── */
    if (rx_len == 0U)
    {
        g_modbus.busy = 0U;
        result.status   = MODBUS_ERR_TIMEOUT;
        result.exc_code = 0U;
        return result;
    }

    /* ── Validate response ─────────────────────────────────────────── */
    /* Extract slave address and function code from first two bytes */
    uint8_t resp_slave = resp[0];
    uint8_t resp_fc    = resp[1];

    result = _modbus_validate_response(req[0], req[1], resp, rx_len);

    g_modbus.busy = 0U;
    return result;
}

/**
  * @brief  Validate a Modbus response frame
  *
  *         Checks performed:
  *           1. Minimum length (at least 5 bytes: addr, FC, CRC_LO, CRC_HI,
  *              plus at least 1 data byte or exception code)
  *           2. Slave address matches request
  *           3. Function code matches request (or is exception: FC | 0x80)
  *           4. CRC16 matches
  *
  * @param  slave     Expected slave address (from request)
  * @param  fc        Expected function code (from request)
  * @param  resp      Response buffer
  * @param  resp_len  Response length
  * @return Modbus_Result_t
  */
static Modbus_Result_t _modbus_validate_response(uint8_t slave, uint8_t fc,
                                                  const uint8_t *resp,
                                                  uint16_t resp_len)
{
    Modbus_Result_t result;
    result.status   = MODBUS_OK;
    result.exc_code = 0U;

    /* ── Minimum frame length check ────────────────────────────────── */
    /* Absolute minimum: addr(1) + FC(1) + CRC(2) = 4 bytes */
    if (resp_len < 4U)
    {
        result.status = MODBUS_ERR_FRAME;
        return result;
    }

    /* ── Slave address check ───────────────────────────────────────── */
    if (resp[0] != slave)
    {
        result.status = MODBUS_ERR_SLAVE;
        return result;
    }

    /* ── Function code check ───────────────────────────────────────── */
    if (resp[1] == (uint8_t)(fc | 0x80U))
    {
        /* Exception response -- at least 5 bytes needed */
        if (resp_len < 5U)
        {
            result.status = MODBUS_ERR_FRAME;
            return result;
        }

        result.status   = MODBUS_ERR_EXCEPTION;
        result.exc_code = resp[2];
        return result;
    }

    if (resp[1] != fc)
    {
        /* Function code doesn't match -- unexpected response */
        result.status = MODBUS_ERR_FRAME;
        return result;
    }

    /* ── CRC check ─────────────────────────────────────────────────── */
    uint16_t calc_crc = Modbus_CRC16(resp, resp_len - 2U);
    uint16_t rcv_crc  = (uint16_t)resp[resp_len - 2U]
                      | ((uint16_t)resp[resp_len - 1U] << 8U);

    if (calc_crc != rcv_crc)
    {
        result.status = MODBUS_ERR_CRC;
        return result;
    }

    return result;
}

/**
  * @brief  Simple microsecond delay (busy-wait)
  * @note   Approximate -- calibrated for STM32H743 @ 400 MHz.
  *         Each iteration is ~10 CPU cycles ≈ 0.025µs.
  *         So for 100µs delay: 100 / 0.025 = 4000 iterations.
  *         We use a simplified approximation: us * 10 = loop iterations
  *         (roughly 10 cycles per iteration on STM32H7 at 400MHz).
  *         A more precise approach would use DWT->CYCCNT, but this
  *         is sufficient for polling delays.
  * @param  us  Microseconds to delay
  */
static void _modbus_delay_us(volatile uint32_t us)
{
    /* Each NOP + loop overhead ≈ 10 CPU cycles ≈ 0.025µs @ 400MHz
     * We target ~100ns per iteration, so us * 10 gives a rough µs delay.
     * For safety, use a factor of 15 to ensure minimum delay meets target. */
    volatile uint32_t count = us * 15U;
    while (count > 0U)
    {
        count--;
        __NOP();
    }
}
