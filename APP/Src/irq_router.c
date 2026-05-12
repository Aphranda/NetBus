/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    irq_router.c
  * @brief   Interrupt router — centralized IRQ handler management in APP layer
  *
  *          Architecture:
  *            Each handler defined here overrides the weak default handler
  *            in startup_stm32h743zitx.s, providing a single dispatch point
  *            for application-specific interrupt handling.
  *
  *          Routed interrupts:
  *            DMA1_Stream0_IRQHandler  (UART7 RX, CIRCULAR mode / ring buffer)
  *              └─ HAL_DMA_IRQHandler()
  *                   └─ UART_DMAReceiveCplt() → HAL_UARTEx_RxEventCallback() [log.c]
  *                        └─ Debug CLI: parses received block, dispatches commands
  *            USART7_IRQHandler (IDLE line detection)
  *              └─ HAL_UART_IRQHandler()
  *                   └─ UART_IDLECplt() → HAL_UARTEx_RxEventCallback() [log.c]
  *                        └─ Debug CLI: parses received block, dispatches commands
  *
  *          Adding new interrupts:
  *            1. Define the handler function here
  *            2. Call the appropriate HAL_*_IRQHandler()
  *            3. Implement the HAL callback in the relevant module
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
#include "irq_router.h"
#include "usart.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/* ── UART7 ─────────────────────────────────────────────────────────────────── */

/**
  * @brief  UART7 global interrupt handler (IDLE line detection)
  * @note   Overrides weak default from startup_stm32h743zitx.s.
  *         Delegates to HAL_UART_IRQHandler(), which handles the IDLE
  *         line interrupt and triggers HAL_UARTEx_RxEventCallback()
  *         defined in log.c for debug CLI.
  *
  *         Flow:
  *           USART7_IRQHandler (this file) — IDLE event
  *             → HAL_UART_IRQHandler (HAL driver)
  *               → UART_IDLECplt() internal handler
  *                 → HAL_UARTEx_RxEventCallback (log.c)
  *                   → processes Size bytes from DMA buffer
  *                   → on '\n': sets pending flag
  *                   → restarts DMA via ReceiveToIdle_DMA()
  *           DMA1_Stream0_IRQHandler — DMA transfer complete
  *             → HAL_DMA_IRQHandler()
  *               → UART_DMAReceiveCplt()
  *                 → HAL_UARTEx_RxEventCallback (log.c)
  *           main loop → Log_DbgProcess (log.c)
  *             → claims pending command atomically
  *             → parses and dispatches to registered handler
  *             → e.g. "att a 5" → IO_SetAttenuatorA(5)
  *             → DMA stays running (restarted by callback)
  */
void UART7_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart7);
}
