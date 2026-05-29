/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    modbus_task.h
  * @brief   Modbus task — owns RS485 and Modbus RTU Master infrastructure
  *
  *          Responsibilities:
  *            - Initialize RS485 driver (UART8 DMA+IDLE)
  *            - Initialize Modbus RTU Master
  *            - Register debug CLI command "modbus" for raw frame injection
  *            - Monitor RS485 RX for unexpected traffic
  *
  *          This module runs in the dedicated Modbus_Task FreeRTOS thread
  *          so that Modbus transactions (which are blocking/polling) do
  *          not stall other application modules.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MODBUS_TASK_H__
#define __MODBUS_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "app_task.h"

extern const App_Module_t g_modbus_task_module;

void Modbus_Task_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* __MODBUS_TASK_H__ */
