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
  *            stm32h7xx_it.c is excluded from build (via EIDE excludeList)
  *            to prevent duplicate symbol errors. All interrupt routing
  *            lives here in the APP layer.
  *
  *          Routed interrupts:
  *            DMA1_Stream0_IRQHandler (UART7 RX, CIRCULAR mode)
  *              └─ HAL_DMA_IRQHandler(huart7.hdmarx)
  *            DMA1_Stream1_IRQHandler (UART7 TX, NORMAL mode)
  *              └─ HAL_DMA_IRQHandler(huart7.hdmatx)
  *            DMA1_Stream2_IRQHandler (UART8 RX, CIRCULAR mode)
  *              └─ HAL_DMA_IRQHandler(huart8.hdmarx)
  *            DMA1_Stream3_IRQHandler (UART8 TX, NORMAL mode)
  *              └─ HAL_DMA_IRQHandler(huart8.hdmatx)
  *            UART7_IRQHandler (IDLE line detection)
  *              └─ HAL_UART_IRQHandler(&huart7)
  *            UART8_IRQHandler (IDLE line detection for RS485)
  *              └─ HAL_UART_IRQHandler(&huart8)
  *            FDCAN1_IT0_IRQHandler
  *              └─ HAL_FDCAN_IRQHandler(&hfdcan1)
  *            FDCAN1_IT1_IRQHandler
  *              └─ HAL_FDCAN_IRQHandler(&hfdcan1)
  *            TIM1_UP_IRQHandler (HAL timebase)
  *              └─ HAL_TIM_IRQHandler(&htim1)
  *                   └─ HAL_TIM_PeriodElapsedCallback() [main.c]
  *                        └─ HAL_IncTick()
  *
  *          Adding new interrupts:
  *            1. Declare the handler in irq_router.h
  *            2. Define the handler function here
  *            3. Call the appropriate HAL_*_IRQHandler()
  *            4. Implement the HAL callback in the relevant module
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
#include "dma.h"
#include "fdcan.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern TIM_HandleTypeDef htim1;

/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/* ── UART7 ─────────────────────────────────────────────────────────────────── */

/**
  * @brief  DMA1 Stream0 interrupt handler (UART7 RX, CIRCULAR mode)
  * @note   Overrides weak default from startup_stm32h743zitx.s.
  *         Delegates to HAL_DMA_IRQHandler(), which dispatches to
  *         UART_DMAReceiveCplt() → HAL_UARTEx_RxEventCallback() [log.c]
  */
void DMA1_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(huart7.hdmarx);
}

/**
  * @brief  DMA1 Stream1 interrupt handler (UART7 TX, NORMAL mode)
  * @note   Overrides weak default from startup_stm32h743zitx.s.
  *         Delegates to HAL_DMA_IRQHandler().
  */
void DMA1_Stream1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(huart7.hdmatx);
}

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

/* ── UART8 (RS485) ─────────────────────────────────────────────────────────── */

/**
  * @brief  DMA1 Stream2 interrupt handler (UART8 RX, CIRCULAR mode)
  * @note   Overrides weak default from startup_stm32h743zitx.s.
  *         Delegates to HAL_DMA_IRQHandler().
  */
void DMA1_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(huart8.hdmarx);
}

/**
  * @brief  DMA1 Stream3 interrupt handler (UART8 TX, NORMAL mode)
  * @note   Overrides weak default from startup_stm32h743zitx.s.
  *         Delegates to HAL_DMA_IRQHandler().
  */
void DMA1_Stream3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(huart8.hdmatx);
}

/**
  * @brief  UART8 global interrupt handler (IDLE line detection for RS485)
  * @note   Overrides weak default from startup_stm32h743zitx.s.
  *         Delegates to HAL_UART_IRQHandler(), which handles the IDLE
  *         line interrupt and triggers HAL_UARTEx_RxEventCallback()
  *         defined in log.c (dispatches to RS485 for UART8).
  */
void UART8_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart8);
}

/* ── FDCAN1 ───────────────────────────────────────────────────────────────── */

/**
  * @brief  FDCAN1 Interrupt line 0 handler
  * @note   Overrides weak default from startup_stm32h743zitx.s.
  *         Delegates to HAL_FDCAN_IRQHandler(), which dispatches to:
  *           - HAL_FDCAN_RxFifo0Callback()  [can.c]
  *           - HAL_FDCAN_TxFifoQueueCallback() [can.c]
  *           - HAL_FDCAN_ErrorCallback()     [can.c]
  */
void FDCAN1_IT0_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&hfdcan1);
}

/**
  * @brief  FDCAN1 Interrupt line 1 handler
  * @note   Overrides weak default from startup_stm32h743zitx.s.
  *         Line 1 typically handles FIFO1 / status interrupts.
  *         Delegates to HAL_FDCAN_IRQHandler().
  */
void FDCAN1_IT1_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&hfdcan1);
}

/* ── TIM1 (HAL Timebase) ──────────────────────────────────────────────────── */

/**
  * @brief  TIM1 Update interrupt handler (HAL timebase)
  * @note   Overrides weak default from startup_stm32h743zitx.s.
  *         Delegates to HAL_TIM_IRQHandler(), which calls
  *         HAL_TIM_PeriodElapsedCallback() defined in main.c.
  *         This is the system tick source (HAL_IncTick).
  */
void TIM1_UP_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim1);
}
