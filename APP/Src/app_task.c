/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_task.c
  * @brief   Application task manager — pure orchestration framework
  *
  *          Architecture:
  *            main()
  *              ├─ App_RegisterModule(&module_1);  // Register modules
  *              ├─ App_RegisterModule(&module_2);
  *              ├─ App_Task_Init();                 // Init all in order
  *              └─ App_Task_Loop();                 // Never returns
  *                    ├─ module_1.process()
  *                    ├─ module_2.process()
  *                    └─ ...
  *
  *          Module Registration:
  *            Modules are stored in registration order. App_Task_Init()
  *            iterates the table and calls each module's init() hook.
  *            App_Task_Loop() calls process() on each module per iteration.
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
#include "app_task.h"
#include "log_task.h"
#include "rs485_task.h"
#include "detector_task.h"
#include <string.h>

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/**
  * @brief  Maximum number of registered application modules
  */
#define APP_MAX_MODULES   8U

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/**
  * @brief  Current application system state
  */
static App_State_t g_app_state = APP_STATE_RESET;

/**
  * @brief  Registered module table — statically allocated
  */
static const App_Module_t *g_modules[APP_MAX_MODULES];
static uint8_t             g_module_count = 0U;

/**
  * @brief  Global error count for diagnostics
  */
static uint32_t g_error_count = 0U;

/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize all application modules — single entry point for main()
  * @note   Registers ALL built-in modules (Log first, then others), then
  *         initializes them in registration order via App_Task_Init().
  *         As new modules are added, register them here instead of in main().
  * @retval APP_OK on success, APP_ERROR if any module fails
  */
App_Status_t App_Init(void)
{
    App_Status_t status;

    /* ── Register Log task first — must be first so other modules can log during init ─ */
    status = App_RegisterModule(&g_log_task_module);
    if (status != APP_OK)
    {
        return status;
    }

    /* ── Register RS485 task ───────────────────────────────────────────────── */
    status = App_RegisterModule(&g_rs485_task_module);
    if (status != APP_OK)
    {
        return status;
    }

    /* ── Register Detector task ─────────────────────────────────────────────── */
    status = App_RegisterModule(&g_detector_task_module);
    if (status != APP_OK)
    {
        return status;
    }

    /* ── Register additional modules here ──────────────────────────────────── */
    /* e.g. status = App_RegisterModule(&g_xxx_module); if (status != APP_OK) return status; */

    /* ── Initialize all registered modules in order ────────────────────────── */
    return App_Task_Init();
}

/**
  * @brief  Register an application module
  * @note   Must be called BEFORE App_Task_Init().
  *         Modules are processed in registration order.
  * @param  module  Pointer to static App_Module_t descriptor
  * @retval APP_OK         on success
  * @retval APP_INVALID    if module is NULL
  * @retval APP_ERROR      if module table is full
  */
App_Status_t App_RegisterModule(const App_Module_t *module)
{
    if (module == NULL)
    {
        return APP_INVALID;
    }

    if (g_module_count >= APP_MAX_MODULES)
    {
        return APP_ERROR;  /* Table full */
    }

    g_modules[g_module_count] = module;
    g_module_count++;

    return APP_OK;
}

/**
  * @brief  Initialize the application task manager
  * @note   Iterates all previously registered modules and calls their init().
  *         Does NOT reset the module table — modules registered before this
  *         call are preserved.
  * @retval APP_OK on success, APP_ERROR if any module fails
  */
App_Status_t App_Task_Init(void)
{
    App_Status_t status = APP_OK;

    /* Set system state */
    g_app_state = APP_STATE_INIT;
    g_error_count = 0U;

    /* ── Initialize all registered modules in order ──────────────────────── */
    for (uint8_t i = 0U; i < g_module_count; i++)
    {
        if (g_modules[i] != NULL && g_modules[i]->init != NULL)
        {
            App_Status_t ret = g_modules[i]->init();
            if (ret != APP_OK)
            {
                g_error_count++;
                if (g_modules[i]->on_error != NULL)
                {
                    g_modules[i]->on_error(ret);
                }
                status = APP_ERROR;
            }
        }
    }

    /* Transition to running (or error) state */
    g_app_state = (status == APP_OK) ? APP_STATE_RUNNING : APP_STATE_ERROR;

    return status;
}

/**
  * @brief  Main application loop — never returns
  * @note   Calls process() on every registered module each iteration.
  *         Modules execute in registration order (round-robin).
  */
void App_Task_Loop(void)
{
    /* Main loop — process all modules forever */
    while (1U)
    {
        for (uint8_t i = 0U; i < g_module_count; i++)
        {
            if (g_modules[i] != NULL && g_modules[i]->process != NULL)
            {
                App_Status_t ret = g_modules[i]->process();
                if (ret != APP_OK)
                {
                    g_error_count++;
                    g_app_state = APP_STATE_ERROR;
                    if (g_modules[i]->on_error != NULL)
                    {
                        g_modules[i]->on_error(ret);
                    }
                }
            }
        }
    }
}

/* ── State ────────────────────────────────────────────────────────────────── */

/**
  * @brief  Get current application system state
  * @retval Current App_State_t
  */
App_State_t App_GetState(void)
{
    return g_app_state;
}
