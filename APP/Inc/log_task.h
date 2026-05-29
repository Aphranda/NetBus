/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    log_task.h
  * @brief   Log task -- wraps ThirdParty/Log into an App_Module_t interface
  *
  *          Responsibilities:
  *            - Initialize the Log subsystem (binds UART7)
  *            - Provide log control API (level, re-init)
  *            - Expose module descriptor for app_task registration
  *
  *          Usage:
  *            1. Include this header in main.c
  *            2. Call App_RegisterModule(&g_log_task_module) BEFORE App_Task_Init()
  *            3. App_Task_Init() will invoke log_task's init automatically
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __LOG_TASK_H__
#define __LOG_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "app_task.h"
#include "log.h"

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/**
  * @brief  Log task module descriptor -- register with App_RegisterModule()
  * @note   Must be registered BEFORE App_Task_Init() so that Log is available
  *         for diagnostic output during other modules' initialization.
  */
extern const App_Module_t g_log_task_module;

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Set the runtime log level dynamically
  * @param  level  One of LOG_LEVEL_* constants (0 = NONE .. 5 = VERBOSE)
  */
void Log_Task_SetLevel(uint8_t level);

/**
  * @brief  Get the current runtime log level
  * @retval Current log level
  */
uint8_t Log_Task_GetLevel(void);

/**
  * @brief  Re-initialize the log subsystem with new configuration
  * @param  config  Optional new config; NULL keeps current settings
  */
void Log_Task_ReInit(const Log_Config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* __LOG_TASK_H__ */
