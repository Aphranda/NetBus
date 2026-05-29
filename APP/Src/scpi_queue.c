/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    scpi_queue.c
  * @brief   SCPI message queue implementation
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "scpi_queue.h"
#include "scpi-def.h"
#include "log.h"
#include "cmsis_os2.h"
#include <string.h>

/* Private variables ---------------------------------------------------------*/

static osMessageQueueId_t g_scpi_queue = NULL;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Public API                                                                 */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
  * @brief  Initialize the SCPI message queue and parser
  */
void SCPI_Queue_Init(void)
{
    g_scpi_queue = osMessageQueueNew(SCPI_QUEUE_DEPTH, SCPI_QUEUE_MSG_SIZE, NULL);
    if (g_scpi_queue == NULL)
    {
        LOG_ERROR("SCPI_Queue: failed to create message queue");
        return;
    }

    SCPI_SystemInit();
    LOG_INFO("SCPI_Queue: initialized (queue depth=%u, msg=%u bytes)",
             (unsigned)SCPI_QUEUE_DEPTH, (unsigned)SCPI_QUEUE_MSG_SIZE);
}

/**
  * @brief  Enqueue a command line (non-blocking, thread-safe)
  */
void SCPI_EnqueueLine(const char *line)
{
    if (g_scpi_queue == NULL || line == NULL || line[0] == '\0')
        return;

    /* Copy into a stack buffer sized to the queue message */
    char msg[SCPI_QUEUE_MSG_SIZE];
    size_t len = strlen(line);
    if (len >= SCPI_QUEUE_MSG_SIZE)
        len = SCPI_QUEUE_MSG_SIZE - 1U;
    memcpy(msg, line, len);
    msg[len] = '\0';

    /* Non-blocking send — drop silently if queue full */
    osMessageQueuePut(g_scpi_queue, msg, 0U, 0U);
}

/**
  * @brief  SCPI_Task main loop — dequeue and process forever
  */
void SCPI_Queue_TaskLoop(void)
{
    char msg[SCPI_QUEUE_MSG_SIZE];

    for (;;)
    {
        osStatus_t st = osMessageQueueGet(g_scpi_queue, msg, NULL, osWaitForever);
        if (st != osOK)
            continue;

        /* Known debug commands bypass SCPI to avoid spurious -113 errors */
        if (Log_DbgIsEnabled() && Log_DbgIsKnownCommand(msg))
        {
            Log_DbgProcessLine(msg);
        }
        else if (SCPI_TryParse(msg))
        {
            /* SCPI recognized and handled */
        }
        else if (Log_DbgIsEnabled())
        {
            Log_DbgProcessLine(msg);
        }
    }
}
