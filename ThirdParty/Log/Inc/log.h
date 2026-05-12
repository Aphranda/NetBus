/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    log.h
  * @brief   Log module header - UART7-based logging with level filtering
  *          Output interface: UART7 (PF6-RX, PF7-TX) @ 115200 8N1
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __LOG_H__
#define __LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdarg.h>
#include "usart.h"

/* Exported defines ----------------------------------------------------------*/

/**
  * @brief Log level enumeration
  */
#define LOG_LEVEL_NONE     0U   /*!< No logging                    */
#define LOG_LEVEL_ERROR    1U   /*!< Error level (red/critical)    */
#define LOG_LEVEL_WARN     2U   /*!< Warning level                 */
#define LOG_LEVEL_INFO     3U   /*!< Informational messages        */
#define LOG_LEVEL_DEBUG    4U   /*!< Debug messages                */
#define LOG_LEVEL_VERBOSE  5U   /*!< Verbose debug (most verbose)  */

/**
  * @brief Default maximum log message length (including null terminator)
  *        Adjust based on available stack size
  */
#ifndef LOG_MAX_MSG_LEN
#define LOG_MAX_MSG_LEN    256U
#endif

/**
  * @brief Compile-time log level filter
  *        Messages with level > LOG_LEVEL will be compiled out.
  *        Override in project settings or before including this header.
  */
#ifndef LOG_LEVEL
#define LOG_LEVEL          LOG_LEVEL_VERBOSE
#endif

/**
  * @brief Enable/disable timestamp prefix in log output
  */
#ifndef LOG_ENABLE_TIMESTAMP
#define LOG_ENABLE_TIMESTAMP  1U
#endif

/**
  * @brief Enable/disable log level tag prefix
  */
#ifndef LOG_ENABLE_TAG
#define LOG_ENABLE_TAG        1U
#endif

/* Exported macro ------------------------------------------------------------*/

/**
  * @brief Log macro wrappers — disabled levels produce zero code overhead
  * @note  Usage: LOG_ERROR("Failed to init sensor, err=%d", status);
  */
#if (LOG_LEVEL >= LOG_LEVEL_ERROR)
#define LOG_ERROR(fmt, ...)    Log_Print(LOG_LEVEL_ERROR, "[ERR]  " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...)    ((void)0U)
#endif

#if (LOG_LEVEL >= LOG_LEVEL_WARN)
#define LOG_WARN(fmt, ...)     Log_Print(LOG_LEVEL_WARN,  "[WARN] " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_WARN(fmt, ...)     ((void)0U)
#endif

#if (LOG_LEVEL >= LOG_LEVEL_INFO)
#define LOG_INFO(fmt, ...)     Log_Print(LOG_LEVEL_INFO,  "[INFO] " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)     ((void)0U)
#endif

#if (LOG_LEVEL >= LOG_LEVEL_DEBUG)
#define LOG_DEBUG(fmt, ...)    Log_Print(LOG_LEVEL_DEBUG, "[DEBUG]" fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)    ((void)0U)
#endif

#if (LOG_LEVEL >= LOG_LEVEL_VERBOSE)
#define LOG_VERBOSE(fmt, ...)  Log_Print(LOG_LEVEL_VERBOSE,"[VERB] " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_VERBOSE(fmt, ...)  ((void)0U)
#endif

/**
  * @brief Initialize log buffer once, then write raw string (no prefix)
  *        Useful for multi-part or binary output.
  */
#define LOG_RAW(fmt, ...)      Log_Print(LOG_LEVEL_NONE,  fmt "\r\n", ##__VA_ARGS__)

/* Exported types ------------------------------------------------------------*/

/**
  * @brief Log configuration structure
  */
typedef struct {
    UART_HandleTypeDef *huart;    /*!< UART handle for output (default: &huart7) */
    uint32_t            timeout;  /*!< UART transmit timeout in ms (default: 100) */
    uint8_t             level;    /*!< Runtime log level filter                    */
    uint8_t             enable_ts;/*!< Enable timestamp prefix                     */
} Log_Config_t;

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize the log module with default configuration (UART7).
  * @note   Must be called once after MX_UART7_Init() and before any log macros.
  * @retval HAL_OK on success, HAL_ERROR otherwise
  */
HAL_StatusTypeDef Log_Init(void);

/**
  * @brief  Initialize the log module with custom configuration.
  * @param  config Pointer to Log_Config_t structure
  * @retval HAL_OK on success, HAL_ERROR otherwise
  */
HAL_StatusTypeDef Log_InitEx(const Log_Config_t *config);

/**
  * @brief  Core print function — formats message and sends via UART
  * @param  level   Log level of the message
  * @param  fmt     printf-style format string
  * @param  ...     Variable arguments for format specifiers
  * @note   Called internally by LOG_* macros; can be called directly.
  */
void Log_Print(uint8_t level, const char *fmt, ...);

/**
  * @brief  Flush pending log data (waits for UART TX to complete)
  * @note   Currently a no-op since HAL_UART_Transmit is blocking.
  *         Reserved for future DMA/IT-based transmit.
  */
void Log_Flush(void);

/**
  * @brief  Set runtime log level (can be changed on-the-fly)
  * @param  level  One of LOG_LEVEL_* constants
  */
void Log_SetLevel(uint8_t level);

/**
  * @brief  Get current runtime log level
  * @retval Current log level
  */
uint8_t Log_GetLevel(void);

#ifdef __cplusplus
}
#endif

#endif /* __LOG_H__ */
