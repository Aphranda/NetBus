/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    log.c
  * @brief   Log module implementation — UART7 output driver
  *          Provides level-filtered, timestamped logging over UART7
  *          (PF6-RX, PF7-TX @ 115200 8N1).
  *
  *          Architecture:
  *            LOG_* macros → Log_Print() → vsnprintf() → HAL_UART_Transmit()
  *
  *          Compile-time filtering via LOG_LEVEL eliminates dead code.
  *          Runtime filtering via g_log_config.level skips unwanted output.
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
#include "log.h"
#include <stdio.h>
#include <string.h>

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/**
  * @brief Level tag strings for human-readable prefix
  */
static const char * const LOG_TAG[] = {
    [LOG_LEVEL_NONE]    = "",          /* RAW mode — no tag */
    [LOG_LEVEL_ERROR]   = "[ERR] ",
    [LOG_LEVEL_WARN]    = "[WARN] ",
    [LOG_LEVEL_INFO]    = "[INFO] ",
    [LOG_LEVEL_DEBUG]   = "[DEBUG] ",
    [LOG_LEVEL_VERBOSE] = "[VERB] ",
};

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/**
  * @brief Default log configuration — UART7, 100ms timeout, all levels, timestamp enabled
  */
static Log_Config_t g_log_config = {
    .huart     = NULL,     /* Will be set to &huart7 in Log_Init() */
    .timeout   = 100U,
    .level     = LOG_LEVEL_VERBOSE,
    .enable_ts = 1U,
};

/* Private function prototypes -----------------------------------------------*/

/**
  * @brief  Format and write timestamp prefix into buffer
  * @param  buf    Output buffer
  * @param  size   Remaining buffer size
  * @return Number of characters written (excluding null terminator)
  */
static int _log_write_timestamp(char *buf, size_t size);

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize log module with defaults (UART7, verbose level, timestamp on)
  * @note   Call after MX_UART7_Init() in main()
  * @retval HAL_OK always (UART handle pointer is stored, no HAL ops here)
  */
HAL_StatusTypeDef Log_Init(void)
{
    Log_Config_t default_config = {
        .huart     = &huart7,
        .timeout   = 100U,
        .level     = LOG_LEVEL_VERBOSE,
        .enable_ts = 1U,
    };

    return Log_InitEx(&default_config);
}

/**
  * @brief  Initialize log module with custom configuration
  * @param  config  Pointer to Log_Config_t with desired settings
  * @retval HAL_ERROR if config is NULL or huart is NULL
  * @retval HAL_OK   on success
  */
HAL_StatusTypeDef Log_InitEx(const Log_Config_t *config)
{
    if (config == NULL || config->huart == NULL)
    {
        return HAL_ERROR;
    }

    /* Copy configuration */
    g_log_config.huart     = config->huart;
    g_log_config.timeout   = config->timeout;
    g_log_config.level     = config->level;
    g_log_config.enable_ts = config->enable_ts;

    /* Send an initialization banner to confirm UART is working */
    Log_Print(LOG_LEVEL_INFO, "Log module initialized (UART7 @ 115200 8N1)");

    return HAL_OK;
}

/**
  * @brief  Core print function — thread-safe, interrupt-safe formatted output
  * @param  level  Message log level
  * @param  fmt    printf-style format string
  * @param  ...    Variable arguments
  * @note   If level > current runtime level, the message is silently dropped.
  *         Uses vsnprintf for safe bounded formatting.
  *         Temporarily disables interrupts to protect the shared buffer.
  */
void Log_Print(uint8_t level, const char *fmt, ...)
{
    char    buffer[LOG_MAX_MSG_LEN];
    int     pos = 0;
    va_list args;

    /* Runtime level filtering */
    if (level > g_log_config.level)
    {
        return;
    }

    /* Guard against null handle (not initialized) */
    if (g_log_config.huart == NULL)
    {
        return;
    }

    /* ── Build prefix: [timestamp] [TAG] ────────────────────────────────── */
    if (g_log_config.enable_ts != 0U)
    {
        pos += _log_write_timestamp(&buffer[pos], sizeof(buffer) - (size_t)pos);
    }

    /* Write level tag (only for non-RAW messages) */
    if (level != LOG_LEVEL_NONE && level <= LOG_LEVEL_VERBOSE)
    {
        const char *tag = LOG_TAG[level];
        while (*tag != '\0' && pos < (int)(sizeof(buffer) - 2))
        {
            buffer[pos++] = *tag++;
        }
    }

    /* ── Format user message ─────────────────────────────────────────────── */
    if (pos < (int)(sizeof(buffer) - 2))
    {
        va_start(args, fmt);
        pos += vsnprintf(&buffer[pos], (size_t)(sizeof(buffer) - (size_t)pos - 1U), fmt, args);
        va_end(args);
    }

    /* Ensure null termination */
    if (pos >= (int)sizeof(buffer))
    {
        pos = (int)sizeof(buffer) - 1;
    }
    buffer[pos] = '\0';

    /* ── Transmit via UART7 ──────────────────────────────────────────────── */
    /* Temporarily disable interrupts to prevent concurrent UART access */
    __disable_irq();
    HAL_UART_Transmit(g_log_config.huart, (uint8_t *)buffer, (uint16_t)pos, g_log_config.timeout);
    __enable_irq();
}

/**
  * @brief  Flush log output — wait for UART TX to complete
  * @note   Currently a no-op since HAL_UART_Transmit is blocking.
  *         If later switched to DMA/IT mode, this will wait for completion.
  */
void Log_Flush(void)
{
    /* Blocking HAL_UART_Transmit already guarantees completion.
     * Reserved for future non-blocking transmission. */
}

/**
  * @brief  Set runtime log level threshold
  * @param  level  One of LOG_LEVEL_* constants
  */
void Log_SetLevel(uint8_t level)
{
    if (level <= LOG_LEVEL_VERBOSE)
    {
        g_log_config.level = level;
    }
}

/**
  * @brief  Get current runtime log level
  * @retval Current log level
  */
uint8_t Log_GetLevel(void)
{
    return g_log_config.level;
}

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Write timestamp "[12345] " into buffer using HAL_GetTick()
  * @param  buf   Output buffer
  * @param  size  Remaining size in buffer
  * @return Number of characters written, not including null terminator
  */
static int _log_write_timestamp(char *buf, size_t size)
{
    int written;

    if (buf == NULL || size < 4U)
    {
        return 0;
    }

    /* Format: [tick_ms] */
    written = snprintf(buf, size, "[%lu] ", HAL_GetTick());

    /* Clamp negative return (shouldn't happen with valid buffer) */
    return (written < 0) ? 0 : written;
}
