/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    attenuator_task.h
  * @brief   Attenuator task -- owns the "att" debug CLI command
  *
  *          Responsibilities:
  *            - Wrap the IO attenuator API (IO_GetAttenuatorA/B,
  *              IO_SetAttenuatorA/B) behind the "att" debug CLI command
  *            - Expose module descriptor for app_task registration
  *
  *          Usage:
  *            Register via App_RegisterModule(&g_attenuator_task_module)
  *            before App_Task_Init().
  *
  *          Debug CLI:
  *            att a <0-15>   -- Set Attenuator A
  *            att b <0-15>   -- Set Attenuator B
  *            att a get/?    -- Get Attenuator A
  *            att b get/?    -- Get Attenuator B
  *            att get/?      -- Get both attenuators
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __ATTENUATOR_TASK_H__
#define __ATTENUATOR_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "app_task.h"

/* Exported variables --------------------------------------------------------*/

extern const App_Module_t g_attenuator_task_module;

#ifdef __cplusplus
}
#endif

#endif /* __ATTENUATOR_TASK_H__ */
