/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    log_task.c
  * @brief   Log task implementation — wraps ThirdParty/Log as App_Module_t
  *
  *          Registration order matters:
  *            Log_Task must be registered FIRST so that other modules can
  *            use LOG_INFO/WARN/ERROR during their own initialization.
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
#include "log_task.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/**
  * @brief  Log task initialization — calls Log_Init()
  * @retval APP_OK on success, APP_ERROR on failure
  */
static App_Status_t _log_task_init(void);

/**
  * @brief  Log task periodic process — no-op (log is event-driven)
  * @retval APP_OK
  */
static App_Status_t _log_task_process(void);

/**
  * @brief  Log task error handler — called if Log_Init() fails
  * @param  err  Error status code
  */
static void _log_task_on_error(App_Status_t err);

/* Exported variables --------------------------------------------------------*/

/**
  * @brief  Log task module descriptor — register via App_RegisterModule()
  * @note   Must be the first module registered, so Log is active before
  *         any other module's init() runs.
  */
const App_Module_t g_log_task_module = {
    .name     = "Log",
    .init     = _log_task_init,
    .process  = _log_task_process,
    .on_error = _log_task_on_error,
};

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Set runtime log level
  * @param  level  LOG_LEVEL_* value (0=NONE .. 5=VERBOSE)
  */
void Log_Task_SetLevel(uint8_t level)
{
    Log_SetLevel(level);
    LOG_INFO("Log_Task: level set to %u", (unsigned int)level);
}

/**
  * @brief  Get current log level
  * @retval Current LOG_LEVEL_* value
  */
uint8_t Log_Task_GetLevel(void)
{
    return Log_GetLevel();
}

/**
  * @brief  Re-initialize Log subsystem with optional new config
  * @param  config  New config pointer, or NULL for defaults
  */
void Log_Task_ReInit(const Log_Config_t *config)
{
    if (config != NULL)
    {
        if (Log_InitEx(config) == HAL_OK)
        {
            LOG_INFO("Log_Task: re-initialized with custom config");
        }
    }
    else
    {
        if (Log_Init() == HAL_OK)
        {
            LOG_INFO("Log_Task: re-initialized (defaults)");
        }
    }
}

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initialize the Log subsystem (binds UART7)
  * @note   Called by App_Task_Init() during module scan
  * @retval APP_OK on success, APP_ERROR on failure
  */
static App_Status_t _log_task_init(void)
{
    if (Log_Init() != HAL_OK)
    {
        return APP_ERROR;
    }
    return APP_OK;
}

/**
  * @brief  Log task periodic process — currently no-op
  * @note   Log is event-driven (called by other modules via LOG_* macros).
  *         Reserved for future use (e.g., periodic flush or stats).
  * @retval APP_OK
  */
static App_Status_t _log_task_process(void)
{
    /* Process any pending debug commands from UART RX */
    Log_DbgProcess();

    return APP_OK;
}

/**
  * @brief  Log task error handler
  * @param  err  Error code from init
  * @note   Cannot use LOG_* here — Log itself may be the cause of failure.
  *         Execution will reach Error_Handler() in main.c if init fails.
  */
static void _log_task_on_error(App_Status_t err)
{
    (void)err;
    /* Log subsystem unavailable — no output possible */
}
