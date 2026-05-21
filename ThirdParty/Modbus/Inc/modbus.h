/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    modbus.h
  * @brief   Modbus RTU Master driver — request/response transaction layer
  *
  *          Architecture:
  *            This module implements a Modbus RTU Master (client) that sends
  *            requests via the RS485 driver (ThirdParty/RS485) and waits for
  *            slave responses using a polling-based timeout mechanism.
  *
  *            ┌─────────────────────────────────────────────────────┐
  *            │  Application Layer                                  │
  *            │    Modbus_ReadHoldingRegisters(slave, reg, qty, d)  │
  *            └──────────────┬──────────────────────────────────────┘
  *                           │
  *            ┌──────────────▼──────────────────────────────────────┐
  *            │  Modbus RTU Master (modbus.c)                       │
  *            │    - Build request frame (addr + FC + data + CRC)   │
  *            │    - Send via RS485_Transmit()                      │
  *            │    - Poll RS485_Available() with timeout            │
  *            │    - Validate response (addr, FC, CRC)              │
  *            │    - Parse data into caller's buffer                │
  *            └──────────────┬──────────────────────────────────────┘
  *                           │
  *            ┌──────────────▼──────────────────────────────────────┐
  *            │  RS485 Driver (ThirdParty/RS485)                    │
  *            │    - UART8 + DMA CIRCULAR + IDLE line detection     │
  *            │    - RS485_Transmit() / RS485_Available() / ...     │
  *            └─────────────────────────────────────────────────────┘
  *
  *          Important Design Decisions:
  *            1. Synchronous (blocking) — each request waits for the
  *               response or timeout. Simplifies application logic.
  *            2. Flushes RX buffer before each request — prevents
  *               stale data from being misinterpreted as a response.
  *            3. Uses RS485_Transmit() (non-blocking DMA TX) + polling
  *               RX — does not rely on the IDLE callback for frame
  *               assembly.
  *            4. Thread-safe for single-caller use (no reentrancy).
  *
  *          Modbus RTU Frame Format:
  *            [Slave Addr (1)] [Function Code (1)] [Data (N)] [CRC Lo] [CRC Hi]
  *
  *          Exception Response:
  *            [Slave Addr (1)] [FC | 0x80 (1)] [Exception Code (1)] [CRC Lo] [CRC Hi]
  *
  *          Supported Function Codes:
  *            FC 0x01 — Read Coils
  *            FC 0x02 — Read Discrete Inputs
  *            FC 0x03 — Read Holding Registers
  *            FC 0x04 — Read Input Registers
  *            FC 0x05 — Write Single Coil
  *            FC 0x06 — Write Single Register
  *            FC 0x0F — Write Multiple Coils
  *            FC 0x10 — Write Multiple Registers
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MODBUS_H__
#define __MODBUS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "modbus_config.h"

/* Exported constants --------------------------------------------------------*/

/** @defgroup MODBUS_Function_Codes Modbus Function Codes
  * @brief Standard Modbus RTU function code definitions
  * @{
  */
#define MODBUS_FC_READ_COILS              0x01U  /*!< Read Coils (digital outputs)          */
#define MODBUS_FC_READ_DISCRETE_INPUTS    0x02U  /*!< Read Discrete Inputs (digital inputs) */
#define MODBUS_FC_READ_HOLDING_REGISTERS  0x03U  /*!< Read Holding Registers                */
#define MODBUS_FC_READ_INPUT_REGISTERS    0x04U  /*!< Read Input Registers                  */
#define MODBUS_FC_WRITE_SINGLE_COIL       0x05U  /*!< Write Single Coil                     */
#define MODBUS_FC_WRITE_SINGLE_REGISTER   0x06U  /*!< Write Single Register                 */
#define MODBUS_FC_WRITE_MULTIPLE_COILS    0x0FU  /*!< Write Multiple Coils                  */
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS 0x10U /*!< Write Multiple Registers              */
/** @} */

/** @defgroup MODBUS_Exception_Codes Modbus Exception Codes
  * @brief Standard Modbus exception code definitions
  * @{
  */
#define MODBUS_EXC_ILLEGAL_FUNCTION       0x01U  /*!< Function code not supported by slave   */
#define MODBUS_EXC_ILLEGAL_DATA_ADDRESS   0x02U  /*!< Data address out of range              */
#define MODBUS_EXC_ILLEGAL_DATA_VALUE     0x03U  /*!< Data value out of range                */
#define MODBUS_EXC_SLAVE_DEVICE_FAILURE   0x04U  /*!< Slave device failure                   */
#define MODBUS_EXC_ACKNOWLEDGE            0x05U  /*!< Slave accepted, processing (long task) */
#define MODBUS_EXC_SLAVE_DEVICE_BUSY      0x06U  /*!< Slave busy, retry later                */
#define MODBUS_EXC_NEGATIVE_ACKNOWLEDGE   0x07U  /*!< Negative acknowledgement               */
#define MODBUS_EXC_MEMORY_PARITY_ERROR    0x08U  /*!< Memory parity error                    */
#define MODBUS_EXC_GATEWAY_PATH_UNAVAIL   0x0AU  /*!< Gateway path unavailable               */
#define MODBUS_EXC_GATEWAY_TARGET_FAILED  0x0BU  /*!< Gateway target device failed to resp.  */
/** @} */

/** @defgroup MODBUS_Status Modbus Status Codes
  * @brief Return status for all Modbus master API functions
  * @{
  */
typedef enum {
    MODBUS_OK          = 0,  /*!< Transaction completed successfully                      */
    MODBUS_ERR_TIMEOUT = 1,  /*!< Slave did not respond within timeout period             */
    MODBUS_ERR_CRC     = 2,  /*!< Response CRC mismatch                                   */
    MODBUS_ERR_SLAVE   = 3,  /*!< Response slave address does not match request            */
    MODBUS_ERR_FRAME   = 4,  /*!< Response frame is malformed (too short, invalid length)  */
    MODBUS_ERR_EXCEPTION = 5,/*!< Slave returned an exception (check exc_code)            */
    MODBUS_ERR_BUSY    = 6,  /*!< RS485 TX busy (another transaction in progress)          */
    MODBUS_ERR_PARAM   = 7,  /*!< Invalid parameter (null pointer, zero qty, etc.)         */
    MODBUS_ERR_NODATA  = 8,  /*!< No data received (zero bytes from RS485)                 */
} Modbus_Status_t;
/** @} */

/* Exported types ------------------------------------------------------------*/

/**
  * @brief  Modbus transaction result structure
  * @note   Returned by all Modbus_* functions (except Init/SetTimeout).
  *         Contains overall status and the exception code if applicable.
  */
typedef struct {
    Modbus_Status_t status;    /*!< Transaction status                     */
    uint8_t         exc_code;  /*!< Exception code (valid if status=EXCEPT) */
} Modbus_Result_t;

/* Exported macro ------------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize the Modbus RTU Master module
  * @note   Must be called once after RS485_Init() and before any other
  *         Modbus API functions. Resets internal state and sets default
  *         response timeout (MODBUS_DEFAULT_TIMEOUT_MS).
  */
void Modbus_Init(void);

/**
  * @brief  Set the response timeout for Modbus transactions
  * @param  timeout_ms  Timeout in milliseconds
  * @note   Default is MODBUS_DEFAULT_TIMEOUT_MS (1000ms).
  *         Increase for slow slave devices or low baud rates.
  *         Decrease for faster error detection on high-speed links.
  */
void Modbus_SetTimeout(uint32_t timeout_ms);

/**
  * @brief  Get the current response timeout value
  * @return Timeout in milliseconds
  */
uint32_t Modbus_GetTimeout(void);

/**
  * @brief  Check if a Modbus transaction is currently pending
  * @retval 1 if a transaction is in progress, 0 otherwise
  * @note   Useful for cooperative multitasking — other tasks (e.g.
  *         rfsw_task process function) should not consume RS485 RX
  *         data while a transaction is pending.
  */
uint8_t Modbus_IsTransactionPending(void);

/* ── Read Functions ──────────────────────────────────────────────────────── */

/**
  * @brief  Read Holding Registers (Modbus FC = 0x03)
  * @note   Reads the contents of holding registers from a remote device.
  *         Holding registers are 16-bit read/write data registers.
  * @param  slave  Slave device address (1 – 247)
  * @param  reg    Starting register address (0 – 0xFFFF)
  * @param  qty    Number of registers to read (1 – 125)
  * @param  dest   Output buffer for register values (must hold at least qty uint16_t)
  * @retval Modbus_Result_t with status
  *         - MODBUS_OK:      Success, dest filled with register values (big-endian decoded)
  *         - MODBUS_ERR_TIMEOUT:  No response from slave
  *         - MODBUS_ERR_EXCEPTION: Slave returned exception, check exc_code
  */
Modbus_Result_t Modbus_ReadHoldingRegisters(uint8_t slave, uint16_t reg,
                                            uint16_t qty, uint16_t *dest);

/**
  * @brief  Read Input Registers (Modbus FC = 0x04)
  * @note   Reads the contents of input registers from a remote device.
  *         Input registers are 16-bit read-only data registers.
  * @param  slave  Slave device address (1 – 247)
  * @param  reg    Starting register address (0 – 0xFFFF)
  * @param  qty    Number of registers to read (1 – 125)
  * @param  dest   Output buffer for register values (must hold at least qty uint16_t)
  * @retval Modbus_Result_t
  */
Modbus_Result_t Modbus_ReadInputRegisters(uint8_t slave, uint16_t reg,
                                          uint16_t qty, uint16_t *dest);

/**
  * @brief  Read Coils (Modbus FC = 0x01)
  * @note   Reads the ON/OFF status of discrete outputs (coils) from a
  *         remote device. Coil values are packed as bits in the destination
  *         buffer (LSB of first byte = first coil).
  * @param  slave  Slave device address (1 – 247)
  * @param  addr   Starting coil address (0 – 0xFFFF)
  * @param  qty    Number of coils to read (1 – 2000, but limited by
  *                MODBUS_MAX_COILS in config)
  * @param  dest   Output buffer for coil states (must hold at least
  *                (qty + 7) / 8 bytes)
  * @retval Modbus_Result_t
  */
Modbus_Result_t Modbus_ReadCoils(uint8_t slave, uint16_t addr,
                                 uint16_t qty, uint8_t *dest);

/**
  * @brief  Read Discrete Inputs (Modbus FC = 0x02)
  * @note   Reads the ON/OFF status of discrete inputs from a remote device.
  *         Discrete inputs are read-only digital input points.
  * @param  slave  Slave device address (1 – 247)
  * @param  addr   Starting input address (0 – 0xFFFF)
  * @param  qty    Number of inputs to read (1 – 2000, but limited by
  *                MODBUS_MAX_COILS in config)
  * @param  dest   Output buffer for input states (must hold at least
  *                (qty + 7) / 8 bytes)
  * @retval Modbus_Result_t
  */
Modbus_Result_t Modbus_ReadDiscreteInputs(uint8_t slave, uint16_t addr,
                                          uint16_t qty, uint8_t *dest);

/* ── Write Single Functions ─────────────────────────────────────────────── */

/**
  * @brief  Write Single Coil (Modbus FC = 0x05)
  * @note   Forces a coil to either ON or OFF. Value 0x0000 = OFF,
  *         0xFF00 = ON (standard Modbus convention).
  * @param  slave  Slave device address (1 – 247)
  * @param  addr   Coil address (0 – 0xFFFF)
  * @param  value  0 = OFF, any non-zero = ON (internally mapped to 0xFF00)
  * @retval Modbus_Result_t
  */
Modbus_Result_t Modbus_WriteSingleCoil(uint8_t slave, uint16_t addr, uint8_t value);

/**
  * @brief  Write Single Register (Modbus FC = 0x06)
  * @note   Writes a single 16-bit value to a holding register.
  * @param  slave  Slave device address (1 – 247)
  * @param  reg    Register address (0 – 0xFFFF)
  * @param  value  16-bit value to write
  * @retval Modbus_Result_t
  */
Modbus_Result_t Modbus_WriteSingleRegister(uint8_t slave, uint16_t reg, uint16_t value);

/* ── Write Multiple Functions ───────────────────────────────────────────── */

/**
  * @brief  Write Multiple Registers (Modbus FC = 0x10)
  * @note   Writes a block of 16-bit values to consecutive holding registers.
  * @param  slave  Slave device address (1 – 247)
  * @param  reg    Starting register address (0 – 0xFFFF)
  * @param  qty    Number of registers to write (1 – 125)
  * @param  data   Pointer to data buffer containing values to write
  *                (must contain at least qty uint16_t, big-endian encoded)
  * @retval Modbus_Result_t
  */
Modbus_Result_t Modbus_WriteMultipleRegisters(uint8_t slave, uint16_t reg,
                                              uint16_t qty, const uint16_t *data);

/**
  * @brief  Write Multiple Coils (Modbus FC = 0x0F)
  * @note   Sets a sequence of coils to either ON or OFF.
  *         Coil values are packed as bits in the source buffer
  *         (LSB of first byte = first coil).
  * @param  slave  Slave device address (1 – 247)
  * @param  addr   Starting coil address (0 – 0xFFFF)
  * @param  qty    Number of coils to write (1 – 2000, but limited by
  *                MODBUS_MAX_COILS in config)
  * @param  data   Pointer to coil state buffer (bit-packed, (qty+7)/8 bytes)
  * @retval Modbus_Result_t
  */
Modbus_Result_t Modbus_WriteMultipleCoils(uint8_t slave, uint16_t addr,
                                          uint16_t qty, const uint8_t *data);

/* ── Utility Functions ──────────────────────────────────────────────────── */

/**
  * @brief  Compute Modbus RTU CRC-16 (polynomial 0xA001)
  * @param  data  Pointer to data buffer
  * @param  len   Length of data in bytes
  * @return CRC-16 value
  * @note   Public for external use (e.g., debugging, custom frame building).
  *         The CRC is appended little-endian: CRC low byte first, high byte second.
  */
uint16_t Modbus_CRC16(const uint8_t *data, uint16_t len);

/**
  * @brief  Get a human-readable string for a Modbus status code
  * @param  status  Modbus_Status_t value
  * @return Pointer to a static string describing the status
  * @note   Useful for logging and debugging.
  */
const char *Modbus_StatusString(Modbus_Status_t status);

/**
  * @brief  Get a human-readable string for a Modbus exception code
  * @param  exc_code  Modbus exception code (MODBUS_EXC_*)
  * @return Pointer to a static string describing the exception
  */
const char *Modbus_ExceptionString(uint8_t exc_code);

/**
  * @brief  Send a raw Modbus frame and wait for response (low-level API)
  * @note   Advanced use only — most applications should use the higher-level
  *         Modbus_ReadHoldingRegisters(), etc. This function allows sending
  *         arbitrary Modbus frames and receiving the raw response.
  *
  *         Internal flow:
  *           1. Flush RS485 RX buffer
  *           2. Send request frame via RS485_Transmit()
  *           3. Wait for RS485_Available() > 0 with timeout
  *           4. Read response into resp buffer
  *
  * @param  req       Request frame (including CRC16)
  * @param  req_len   Length of request frame in bytes
  * @param  resp      Output buffer for response frame
  * @param  resp_len  In: maximum response buffer size
  *                   Out: actual response length in bytes
  * @retval Modbus_Result_t
  */
Modbus_Result_t Modbus_SendRaw(const uint8_t *req, uint16_t req_len,
                               uint8_t *resp, uint16_t *resp_len);

#ifdef __cplusplus
}
#endif

#endif /* __MODBUS_H__ */
