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
  *            handlers for all interrupts. Strong definitions in this file
  *            override them, providing a single location for interrupt dispatch.
  *
  *          Currently routed:
  *            UART7_IRQHandler  →  HAL_UART_IRQHandler(&huart7)
  *                                  → Log module (debug CLI via UART7 RX)
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

#ifdef __cplusplus
}
#endif

#endif /* __IRQ_ROUTER_H__ */
