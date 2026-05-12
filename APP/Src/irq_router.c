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
  *            UART7_IRQHandler
  *              └─ HAL_UART_IRQHandler(&huart7)
  *                   └─ HAL_UART_RxCpltCallback() [defined in log.c]
  *                        └─ Debug CLI: receives chars, parses commands,
  *                           controls attenuators via IO module
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
  * @brief  UART7 global interrupt handler
  * @note   Overrides weak default from startup_stm32h743zitx.s.
  *         Delegates to HAL_UART_IRQHandler(), which triggers the
  *         HAL_UART_RxCpltCallback() defined in log.c for debug CLI.
  *
  *         Flow:
  *           UART7_IRQHandler (this file)
  *             → HAL_UART_IRQHandler (HAL driver)
  *               → HAL_UART_RxCpltCallback (log.c)
  *                 → stores byte in line buffer
  *                 → on '\n': sets pending flag
  *           main loop → Log_DbgProcess (log.c)
  *             → parses command
  *             → dispatches to registered handler
  *             → e.g. "att a 5" → IO_SetAttenuatorA(5)
  */
void UART7_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart7);
}
