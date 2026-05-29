/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    scpi_queue.h
  * @brief   SCPI message queue — routes UART and TCP lines to SCPI_Task
  *
  *          Architecture:
  *            UART7 RX ISR → log_queue → log_task → SCPI_EnqueueLine()
  *            TCP socket   → NetSCPI_ProcessClient() → SCPI_EnqueueLine()
  *                                                          │
  *                                          ┌───────────────▼────────────────┐
  *                                          │  osMessageQueue (8 × 256B)     │
  *                                          └───────────────┬────────────────┘
  *                                                          │
  *                                          SCPI_Task → SCPI_Queue_TaskLoop()
  *                                                          │
  *                                          SCPI_TryParse() / Debug CLI
  *
  *          Thread safety:
  *            - Producers (log_task, NetSCPI) call osMessageQueuePut
  *            - Consumer (SCPI_Task) calls osMessageQueueGet
  *            - osMessageQueue is FreeRTOS-backed, inherently thread-safe
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __SCPI_QUEUE_H__
#define __SCPI_QUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported constants --------------------------------------------------------*/

#define SCPI_QUEUE_DEPTH        8U
#define SCPI_QUEUE_MSG_SIZE     256U

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize the SCPI queue and SCPI parser subsystem
  * @note   Call once during App_Init(), after Log_Init()
  */
void SCPI_Queue_Init(void);

/**
  * @brief  Enqueue a command line for SCPI processing
  * @note   Thread-safe. Non-blocking — drops silently if queue is full.
  * @param  line  Null-terminated command string
  */
void SCPI_EnqueueLine(const char *line);

/**
  * @brief  SCPI_Task main loop — dequeue and process commands
  * @note   Call from StartSCPITask(). Never returns.
  */
void SCPI_Queue_TaskLoop(void);

#ifdef __cplusplus
}
#endif

#endif /* __SCPI_QUEUE_H__ */
