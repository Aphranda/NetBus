/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can_task.h
  * @brief   CAN task — owns FDCAN1 driver initialization and debug CLI
  *
  *          Responsibilities:
  *            - Initialize CAN driver (FDCAN1)
  *            - Register debug CLI command "can" for bus scan/send
  *
  *          This module runs in the dedicated Can_Task FreeRTOS thread
  *          so that CAN operations do not stall other application modules.
  *
  *          Usage:
  *            1. Include this header in app_task.c
  *            2. Call App_RegisterModule(&g_can_task_module)
  *            3. StartCanTask runs the main CAN processing loop
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __CAN_TASK_H__
#define __CAN_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "app_task.h"

/* Exported variables --------------------------------------------------------*/

extern const App_Module_t g_can_task_module;

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Main CAN task processing loop
  * @note   Runs in StartCanTask FreeRTOS thread. Never returns.
  */
void Can_Task_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* __CAN_TASK_H__ */
