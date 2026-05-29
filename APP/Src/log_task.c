/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    log_task.c
  * @brief   Log task — wraps ThirdParty/Log as App_Module_t
  *
  *          Responsibilities:
  *            - Initialize the Log subsystem (UART7 DMA RX)
  *            - Forward received command lines to SCPI queue
  *            - Handle debug CLI commands that bypass SCPI parser
  *
  *          SCPI parsing now runs in a dedicated SCPI_Task thread.
  *          log_task only owns UART RX → line extraction → queue enqueue.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "log_task.h"
#include "scpi_queue.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

static App_Status_t _log_task_init(void);
static App_Status_t _log_task_process(void);
static void         _log_task_on_error(App_Status_t err);

/* Exported variables --------------------------------------------------------*/

const App_Module_t g_log_task_module = {
    .name     = "Log",
    .init     = _log_task_init,
    .process  = _log_task_process,
    .on_error = _log_task_on_error,
};

/* Exported functions --------------------------------------------------------*/

void Log_Task_SetLevel(uint8_t level)
{
    Log_SetLevel(level);
    LOG_INFO("Log_Task: level set to %u", (unsigned int)level);
}

uint8_t Log_Task_GetLevel(void)
{
    return Log_GetLevel();
}

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

static App_Status_t _log_task_init(void)
{
    if (Log_Init() != HAL_OK)
    {
        return APP_ERROR;
    }

    LOG_INFO("Log_Task: UART7 DMA RX started");
    return APP_OK;
}

static App_Status_t _log_task_process(void)
{
    char line[256];
    uint8_t len = Log_DbgGetLine(line, sizeof(line) - 1);

    if (len == 0U)
    {
        return APP_OK;
    }

    /* Forward to SCPI queue — SCPI_Task handles parsing */
    SCPI_EnqueueLine(line);

    return APP_OK;
}

static void _log_task_on_error(App_Status_t err)
{
    (void)err;
}
