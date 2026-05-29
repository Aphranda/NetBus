/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rfsw_task.h
  * @brief   RF Switch control task — RS485 Modbus RTU master for SP10T switches
  *
  *          Responsibilities:
 *            - Register debug CLI command "rfsw" for RF switch control
 *            - RS485 and Modbus are initialized by the Modbus module
 *              (registered before RFSW).
  *
  *          Usage:
  *            1. Include this header in main.c or app_task.c
  *            2. Call App_RegisterModule(&g_rfsw_task_module) BEFORE App_Task_Init()
  *            3. App_Task_Init() will invoke rfsw_task's init automatically
  *
  *          Debug CLI (rfsw):
  *            rfsw get <addr>              — Read current channel
  *            rfsw set <addr> <ch>         — Set channel (1-10)
  *            rfsw mode <addr> [io|cmd]    — Get/set work mode
  *            rfsw info <addr>             — Read device identity & status
  *            rfsw output <addr> <id> <0|1>— Set single output coil
  *            rfsw outputs <addr>          — Read all 6 output coils
  *            rfsw inputs <addr>           — Read 4 discrete inputs
  *            rfsw status <addr>           — Read device status register
  *            rfsw id <addr> <new_id>      — Change device Modbus address
  *
  *          Reference:
  *            Doc/RF_Switch_Commands.md — SP10T Modbus RTU register map
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __RFSW_TASK_H__
#define __RFSW_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "app_task.h"

/* Exported constants --------------------------------------------------------*/

/* SP10T RF Switch register addresses (Modbus holding registers) */
#define RFSW_REG_CHANNEL        0x0000U  /*!< Current channel (1-10), R/W        */
#define RFSW_REG_WORK_MODE      0x0001U  /*!< Work mode (0=IO, 1=CMD), R/W       */
#define RFSW_REG_DEVICE_ID      0x0002U  /*!< Device Modbus address (1-247), R/W */
#define RFSW_REG_SERIAL_H       0x0003U  /*!< Serial number high 16 bits, R      */
#define RFSW_REG_SERIAL_L       0x0004U  /*!< Serial number low 16 bits, R       */
#define RFSW_REG_DEVICE_NAME_0  0x0005U  /*!< Device name chars 1-2, R/W         */
#define RFSW_REG_DEVICE_NAME_1  0x0006U  /*!< Device name chars 3-4, R/W         */
#define RFSW_REG_DEVICE_NAME_2  0x0007U  /*!< Device name chars 5-6, R/W         */
#define RFSW_REG_DEVICE_NAME_3  0x0008U  /*!< Device name chars 7-8, R/W         */
#define RFSW_REG_DEVICE_STATUS  0x0009U  /*!< Device status bits, R              */
#define RFSW_REG_OUTPUT_CONTROL 0x000AU  /*!< Output control bits, R/W           */
#define RFSW_REG_FW_VERSION     0x000BU  /*!< Firmware version (major<<8|minor)  */
#define RFSW_REG_HW_VERSION     0x001BU  /*!< Hardware version, R                */

/* RF Switch channel limits */
#define RFSW_CHANNEL_MIN        1U       /*!< Minimum channel number (1-based)   */
#define RFSW_CHANNEL_MAX        10U      /*!< Maximum channel number              */

/* RF Switch work modes */
#define RFSW_MODE_IO            0U       /*!< IO DIRECT mode (hardware inputs)   */
#define RFSW_MODE_COMMAND       1U       /*!< COMMAND mode (software control)    */

/* Output coil addresses (0-based) */
#define RFSW_COIL_COUNT         6U       /*!< Number of output coils (S0_CA..S2_CB) */
#define RFSW_INPUT_COUNT        4U       /*!< Number of discrete inputs (CTRL1-4) */

/* Device status bit masks */
#define RFSW_STATUS_INIT_DONE   (1U << 0) /*!< Initialization complete            */
#define RFSW_STATUS_COMM_ACTIVE (1U << 1) /*!< Communication active               */

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/**
  * @brief  RF Switch task module descriptor — register with App_RegisterModule()
  * @note   Must be registered after Log task so that LOG_* macros are available.
  */
extern const App_Module_t g_rfsw_task_module;

/* Exported functions --------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* __RFSW_TASK_H__ */
