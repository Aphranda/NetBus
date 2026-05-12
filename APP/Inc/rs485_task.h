/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rs485_task.h
  * @brief   RS485 task — wraps ThirdParty/RS485 into an App_Module_t interface
  *
  *          Responsibilities:
  *            - Initialize the RS485 subsystem (binds UART8, DMA, DE/RE pin)
  *            - Register debug CLI command "modbus" for sending Modbus frames
  *            - Poll RS485 RX buffer and print received data to Log console
  *
  *          Usage:
  *            1. Include this header in main.c or app_task.c
  *            2. Call App_RegisterModule(&g_rs485_task_module) BEFORE App_Task_Init()
  *            3. App_Task_Init() will invoke rs485_task's init automatically
  *
  *          Debug CLI:
  *            modbus read <slave> <reg> <qty>
  *              - Send a Modbus Read Holding Registers (FC=03) query
  *              - e.g. "modbus read 1 2 1" → 01 03 00 02 00 01 CRChi CRClo
  *            modbus send <hex bytes...>
  *              - Send raw hex bytes over RS485
  *              - e.g. "modbus send 01 03 00 02 00 01 24 0A"
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __RS485_TASK_H__
#define __RS485_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "app_task.h"

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/**
  * @brief  RS485 task module descriptor — register with App_RegisterModule()
  * @note   Must be registered after Log task so that LOG_* macros are available.
  */
extern const App_Module_t g_rs485_task_module;

/* Exported functions --------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* __RS485_TASK_H__ */
