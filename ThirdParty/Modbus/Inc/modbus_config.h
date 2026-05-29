/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    modbus_config.h
  * @brief   Modbus RTU Master configuration -- user-tunable parameters
  *
  *          Default values are suitable for most RS485 Modbus RTU networks.
  *          Override any define before including this header if needed.
  *
  *          Typical UART baud rates: 9600, 19200, 38400, 115200
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MODBUS_CONFIG_H__
#define __MODBUS_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported defines ----------------------------------------------------------*/

/**
  * @brief Maximum Modbus RTU frame length (bytes)
  *        Largest typical frame:
  *          - Request:  1 (addr) + 1 (fc) + 4 (data) + 2 (CRC) = 8 bytes
  *          - Response: 1 (addr) + 1 (fc) + 1 (count) + 250 (data) + 2 (CRC) = 255 bytes
  *        Set to 256 to safely accommodate the maximum response.
  */
#ifndef MODBUS_MAX_FRAME_LEN
#define MODBUS_MAX_FRAME_LEN    256U
#endif

/**
  * @brief Maximum number of registers that can be read/written in one transaction
  *        Modbus specification: maximum 125 registers for FC=03/04.
  */
#ifndef MODBUS_MAX_REGISTERS
#define MODBUS_MAX_REGISTERS    125U
#endif

/**
  * @brief Maximum number of coils that can be read/written in one transaction
  *        Modbus specification: maximum 2000 coils for FC=01/02,
  *        but we limit to 256 for buffer size reasons.
  */
#ifndef MODBUS_MAX_COILS
#define MODBUS_MAX_COILS        256U
#endif

/**
  * @brief Default response timeout in milliseconds
  *        The master waits this long for a slave response before
  *        returning MODBUS_ERR_TIMEOUT.
  *        Adjust based on slave device response time and baud rate.
  */
#ifndef MODBUS_DEFAULT_TIMEOUT_MS
#define MODBUS_DEFAULT_TIMEOUT_MS   1000U
#endif

/**
  * @brief Inter-character timeout in milliseconds
  *        After receiving the first byte of a response, the master
  *        waits this long for additional bytes. If no more bytes
  *        arrive within this window, the frame is considered complete.
  *        This handles cases where the IDLE line detection fires
  *        between bytes of a single frame.
  *
  *        Modbus RTU spec: 3.5 character times silence = frame end.
  *        At 115200 baud (11.5K char/s): ~0.3ms → use 5ms as safe minimum.
  *        At 9600 baud: ~3.6ms → use 10ms.
  */
#ifndef MODBUS_CHAR_TIMEOUT_MS
#define MODBUS_CHAR_TIMEOUT_MS     10U
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MODBUS_CONFIG_H__ */
