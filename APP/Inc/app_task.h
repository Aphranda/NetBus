/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_task.h
  * @brief   Application task manager -- pure orchestration framework
  *
  *          Responsibilities:
  *            - Provide module registration framework (App_Module_t)
  *            - Initialize all registered modules in order
  *            - Run the main processing loop (round-robin)
  *            - Track system state and aggregate error status
  *
  *          Usage:
  *            1. Define App_Module_t descriptors for each subsystem
  *            2. In main(), register all modules via App_RegisterModule()
  *            3. Call App_Task_Init() to initialize all modules
  *            4. Call App_Task_Loop() (never returns)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __APP_TASK_H__
#define __APP_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/

/**
  * @brief  Application status code
  */
typedef enum {
    APP_OK       = 0U,   /*!< Operation succeeded                  */
    APP_ERROR    = 1U,   /*!< Generic error                        */
    APP_BUSY     = 2U,   /*!< Resource busy, retry later           */
    APP_TIMEOUT  = 3U,   /*!< Operation timed out                  */
    APP_INVALID  = 4U,   /*!< Invalid parameter or state           */
} App_Status_t;

/**
  * @brief  Application system state
  */
typedef enum {
    APP_STATE_RESET      = 0U,  /*!< After reset, before init       */
    APP_STATE_INIT       = 1U,  /*!< Initializing                   */
    APP_STATE_RUNNING    = 2U,  /*!< Normal operation               */
    APP_STATE_ERROR      = 3U,  /*!< Recoverable error state        */
    APP_STATE_STOPPED    = 4U,  /*!< Stopped (e.g. safety trigger)  */
} App_State_t;

/**
  * @brief  Application module registration structure
  *         Each application module provides init, process, and error hooks.
  */
typedef struct {
    const char    *name;                          /*!< Module name (for diagnostics) */
    App_Status_t (*init)(void);                   /*!< Module initialization         */
    App_Status_t (*process)(void);                /*!< Periodic processing call      */
    void         (*on_error)(App_Status_t err);   /*!< Error callback                */
} App_Module_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Register an application module
  * @note   Must be called BEFORE App_Task_Init().
  *         Registration order determines initialization and processing order.
  * @param  module  Pointer to a static App_Module_t descriptor
  * @retval APP_OK      on success
  * @retval APP_INVALID if module is NULL
  * @retval APP_ERROR   if module table is full
  */
App_Status_t App_RegisterModule(const App_Module_t *module);

/**
  * @brief  Initialize the application task manager and all registered modules
  * @note   Call once after all HAL peripheral initialization (MX_*_Init()).
  *         Iterates registered modules and calls their init() in order.
  * @retval APP_OK on success, APP_ERROR if any module fails
  */
App_Status_t App_Task_Init(void);

/**
  * @brief  Initialize all application modules in one call
  * @note   Single entry point for main(). Registers ALL built-in modules,
  *         then calls App_Task_Init() to initialize them in order.
  *         Replace manual App_RegisterModule() + App_Task_Init() sequence.
  * @retval APP_OK on success, APP_ERROR if any module fails
  */
App_Status_t App_Init(void);

/**
  * @brief  Main application loop -- calls process() on all registered modules
  * @note   Call inside while(1) in main(). Never returns.
  *         Modules are processed round-robin in registration order.
  */
void App_Task_Loop(void);

/**
  * @brief  Get current application system state
  * @retval Current App_State_t
  */
App_State_t App_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_TASK_H__ */
