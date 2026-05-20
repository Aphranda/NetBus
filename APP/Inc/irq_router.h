/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    irq_router.h
  * @brief   Interrupt router — centralized IRQ handler management in APP layer
  *
  *          Responsibilities:
  *            - Route peripheral interrupts from vector table to the
  *              appropriate HAL handlers or application callbacks
  *            - Keep Core/ layer free of application-specific IRQ logic
  *
  *          Design:
  *            The startup file (startup_stm32h743zitx.s) defines weak default
  *            handlers for all interrupts. Strong definitions in irq_router.c
  *            override them, providing a single dispatch point in the APP layer.
  *            stm32h7xx_it.c is excluded from build to avoid duplicate symbols.
  *
  *          Currently routed:
  *            DMA1_Stream0_IRQHandler → HAL_DMA_IRQHandler() (UART7 RX DMA)
  *            DMA1_Stream1_IRQHandler → HAL_DMA_IRQHandler() (UART7 TX DMA)
  *            DMA1_Stream2_IRQHandler → HAL_DMA_IRQHandler() (UART8 RX DMA)
  *            DMA1_Stream3_IRQHandler → HAL_DMA_IRQHandler() (UART8 TX DMA)
  *            UART7_IRQHandler        → HAL_UART_IRQHandler(&huart7)
  *            UART8_IRQHandler        → HAL_UART_IRQHandler(&huart8)
  *            FDCAN1_IT0_IRQHandler   → HAL_FDCAN_IRQHandler(&hfdcan1)
  *            FDCAN1_IT1_IRQHandler   → HAL_FDCAN_IRQHandler(&hfdcan1)
  *            TIM1_UP_IRQHandler      → HAL_TIM_IRQHandler(&htim1)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __IRQ_ROUTER_H__
#define __IRQ_ROUTER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/*
 * Interrupt handlers — strong definitions override weak defaults from
 * startup_stm32h743zitx.s.  These are declared here for documentation
 * purposes; the vector table references them by name directly.
 */
void DMA1_Stream0_IRQHandler(void);
void DMA1_Stream1_IRQHandler(void);
void DMA1_Stream2_IRQHandler(void);
void DMA1_Stream3_IRQHandler(void);
void UART7_IRQHandler(void);
void UART8_IRQHandler(void);
void FDCAN1_IT0_IRQHandler(void);
void FDCAN1_IT1_IRQHandler(void);
void TIM1_UP_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __IRQ_ROUTER_H__ */
