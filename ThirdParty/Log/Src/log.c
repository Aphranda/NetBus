/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    log.c
  * @brief   Log module implementation — UART7 output driver with debug CLI
  *
  *          Output interface: UART7 (PF6-RX, PF7-TX @ 115200 8N1)
  *
  *          Architecture:
  *            LOG_* macros → Log_Print() → vsnprintf() → HAL_UART_Transmit()
  *            UART RX DMA + IDLE → line buffer → Log_DbgProcess() → cmd dispatch
  *
  *          Compile-time filtering via LOG_LEVEL eliminates dead code.
  *          Runtime filtering via g_log_config.level skips unwanted output.
  *
  *          Debug Commands (built-in):
  *            help           — Show available commands
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
#include <stdlib.h>
#include "usart.h"      /* for huart8 in RS485 dispatch */
#include "rs485.h"      /* for RS485_UART_RxEventCallback */

/* Private typedef -----------------------------------------------------------*/

/**
  * @brief Debug command table entry
  */
typedef struct {
    const char        *name;  /*!< Command name (e.g. "att", "help") */
    Log_DbgCmdFunc_t   func;  /*!< Callback function                */
    const char        *help;  /*!< One-line description (NULL = none) */
} Log_DbgCmdEntry_t;

/* Private define ------------------------------------------------------------*/

/**
  * @brief Debug terminal buffer and limits
  */
#define LOG_DBG_BUF_SIZE    64U    /*!< Maximum input line length        */
#define LOG_DBG_MAX_CMDS    10U    /*!< Maximum registered commands      */
#define LOG_DBG_MAX_ARGS    8U     /*!< Maximum arguments per command    */

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

/* ── Debug command subsystem ──────────────────────────────────────────────── */

/**
  * @brief Debug command table — stores registered commands
  */
static Log_DbgCmdEntry_t g_dbg_cmds[LOG_DBG_MAX_CMDS];
static uint8_t           g_dbg_cmd_count = 0U;

/**
  * @brief Debug input buffer and state
  */
static char              g_dbg_buffer[LOG_DBG_BUF_SIZE]; /*!< Line buffer for incoming chars */
static volatile uint8_t  g_dbg_pos;                       /*!< Current write position        */
static volatile uint8_t  g_dbg_cmd_pending;               /*!< Flag: complete line ready     */
static uint8_t           g_dbg_echo = 0U;                 /*!< Echo mode (default: off)      */

/**
  * @brief DMA RX ring buffer — receives UART data via DMA in CIRCULAR mode
  * @note  Accessed by DMA (peripheral) and HAL_UARTEx_RxEventCallback() (CPU).
  *        DMA continuously writes incoming data to the ring buffer, wrapping
  *        at the buffer boundary. On each IDLE (or TC) event, the callback
  *        reads available bytes from g_rb_rd_idx to the current NDTR-derived
  *        write position, then processes them into the line buffer.
  *
  *        No DMA restart is needed — CIRCULAR mode keeps the transfer active.
  */
static uint8_t           g_dma_rx_buf[LOG_DBG_BUF_SIZE]; /*!< DMA RX ring buffer          */
static uint16_t          g_rb_rd_idx;                     /*!< Ring buffer read index [0..BUF_SIZE) */

/* Private function prototypes -----------------------------------------------*/

/**
  * @brief  Format and write timestamp prefix into buffer
  * @param  buf    Output buffer
  * @param  size   Remaining buffer size
  * @return Number of characters written (excluding null terminator)
  */
static int _log_write_timestamp(char *buf, size_t size);

/* ── Debug command built-in handlers ──────────────────────────────────────── */

static void _dbg_cmd_help(int argc, char **argv);

/* ── Debug command internal helpers ───────────────────────────────────────── */

static void _dbg_parse_and_execute(const char *line);
static int  _dbg_tokenize(char *str, char **argv, int max_args);

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

    /* ── Initialize debug command subsystem ──────────────────────────────── */

    /* Reset debug buffer and ring buffer read index */
    g_dbg_pos         = 0U;
    g_dbg_cmd_pending = 0U;
    g_dbg_echo        = 0U;  /* Echo off by default */
    g_rb_rd_idx       = 0U;

    /* Clear command table and register built-in commands */
    g_dbg_cmd_count = 0U;
    (void)Log_RegisterDbgCmd("help", _dbg_cmd_help);

    /* Start UART RX via DMA with IDLE line detection (CIRCULAR mode).
     * DMA continuously receives bytes into g_dma_rx_buf ring buffer.
     * When the UART line goes idle (after a complete line), the IDLE
     * interrupt fires and HAL_UARTEx_RxEventCallback() processes the
     * received bytes. DMA keeps running — no restart needed. */
    if (HAL_UARTEx_ReceiveToIdle_DMA(g_log_config.huart, g_dma_rx_buf, LOG_DBG_BUF_SIZE) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Disable DMA half-transfer interrupt — we only care about IDLE/TC events */
    __HAL_DMA_DISABLE_IT(g_log_config.huart->hdmarx, DMA_IT_HT);

    /* Send an initialization banner to confirm UART is working */
    Log_Print(LOG_LEVEL_INFO, "Log module initialized (UART7 @ 115200 8N1, DMA+IDLE)");
    Log_Print(LOG_LEVEL_INFO, "Debug CLI ready — type 'help' for commands");

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

    /* ── Transmit via UART ───────────────────────────────────────────────── */
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

/* ── Debug command exported API ───────────────────────────────────────────── */

/**
  * @brief  Process any pending debug commands from UART RX buffer
  * @note   Call this periodically from the main loop or task process hook.
  *         When a complete line (terminated by \r or \n) has been received,
  *         this function parses and dispatches the command.
  */
void Log_DbgProcess(void)
{
    if (g_dbg_cmd_pending == 0U)
    {
        return;
    }

    /*
     * Copy the line into a local buffer INSIDE the critical section
     * so that the UART7 IDLE interrupt callback cannot overwrite
     * g_dbg_buffer while we are parsing it.
     */
    char buf[LOG_DBG_BUF_SIZE];

    __disable_irq();
    g_dbg_cmd_pending = 0U;
    uint8_t len = g_dbg_pos;
    g_dbg_pos = 0U;
    memcpy(buf, g_dbg_buffer, len);
    buf[len] = '\0';
    __enable_irq();

    /* Parse and execute the local copy (immune to interrupt overwrite) */
    _dbg_parse_and_execute(buf);

    /* DMA is already running continuously in CIRCULAR mode —
     * HAL_UARTEx_RxEventCallback() reads from the ring buffer
     * each time IDLE fires. No restart is needed. */
}

/**
  * @brief  Register a custom debug command
  * @param  cmd   Command name string
  * @param  func  Callback function
  * @retval HAL_OK on success, HAL_ERROR if table full
  */
HAL_StatusTypeDef Log_RegisterDbgCmd(const char *cmd, Log_DbgCmdFunc_t func)
{
    return Log_RegisterDbgCmdEx(cmd, func, NULL);
}

/**
  * @brief  Register a custom debug command with help description
  * @param  cmd   Command name string
  * @param  func  Callback function
  * @param  help  One-line description shown by 'help' (NULL = no description)
  * @retval HAL_OK on success, HAL_ERROR if table full
  */
HAL_StatusTypeDef Log_RegisterDbgCmdEx(const char *cmd, Log_DbgCmdFunc_t func, const char *help)
{
    if (cmd == NULL || func == NULL)
    {
        return HAL_ERROR;
    }

    if (g_dbg_cmd_count >= LOG_DBG_MAX_CMDS)
    {
        return HAL_ERROR;
    }

    g_dbg_cmds[g_dbg_cmd_count].name = cmd;
    g_dbg_cmds[g_dbg_cmd_count].func = func;
    g_dbg_cmds[g_dbg_cmd_count].help = help;
    g_dbg_cmd_count++;

    return HAL_OK;
}

/**
  * @brief  Enable or disable UART echo for debug terminal
  * @param  enable  1 = echo on (default), 0 = echo off
  */
void Log_DbgSetEcho(uint8_t enable)
{
    g_dbg_echo = (enable != 0U) ? 1U : 0U;
}

/**
  * @brief  Get the number of received characters pending in the debug buffer
  * @retval Number of characters in buffer (0 = empty)
  */
uint8_t Log_DbgAvailable(void)
{
    return (g_dbg_cmd_pending != 0U) ? g_dbg_pos : 0U;
}

/**
  * @brief  Manually inject a debug command string for processing
  * @param  cmd  Null-terminated command string (e.g. "att a 5")
  * @note   Copies the command into the internal buffer and processes it
  *         immediately (or on next Log_DbgProcess call).
  */
void Log_DbgInject(const char *cmd)
{
    if (cmd == NULL)
    {
        return;
    }

    size_t len = strlen(cmd);
    if (len >= LOG_DBG_BUF_SIZE)
    {
        len = LOG_DBG_BUF_SIZE - 1U;
    }

    memcpy(g_dbg_buffer, cmd, len);
    g_dbg_buffer[len] = '\0';

    _dbg_parse_and_execute(g_dbg_buffer);
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

/* ─────────────────────────────────────────────────────────────────────────── */
/*  HAL UART RX Event callback — ring buffer read via CIRCULAR DMA + IDLE     */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  UART RX Event callback (HAL weak override)
  * @note   Called by HAL on IDLE line detection or DMA transfer complete.
  *         DMA runs in CIRCULAR mode — continuously writing to g_dma_rx_buf.
  *         This callback reads available bytes from the ring buffer using
  *         NDTR to compute the current write position, then processes them
  *         into the debug line buffer. DMA keeps running — no restart needed.
  *
  *         TC events (DMA wrap): read index resets to avoid re-processing
  *         old data (already handled by prior IDLE events for CLI use).
  *
  * @param  huart  UART handle
  * @param  Size   Number of bytes in current DMA cycle (not used directly)
  */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    /* ── Dispatch UART8 (RS485) to its own handler ─────────────────────── */
    if (huart == &huart8)
    {
        RS485_UART_RxEventCallback(huart, Size);
        return;
    }

    /* Only process UART7 — the log/debug UART */
    if (huart != g_log_config.huart)
    {
        return;
    }

    /* ── Compute DMA write position from NDTR ──────────────────────────── */
    /* NDTR decrements from LOG_DBG_BUF_SIZE → 0 as DMA fills the buffer.
     * Write index = (BUF_SIZE - NDTR) gives the current write position.
     * When NDTR was 0 (TC/wrap), it reloads to BUF_SIZE → wr_idx = 0. */
    uint16_t ndtr    = (uint16_t)__HAL_DMA_GET_COUNTER(huart->hdmarx);
    uint16_t wr_idx  = LOG_DBG_BUF_SIZE - ndtr;
    if (wr_idx >= LOG_DBG_BUF_SIZE) wr_idx = 0U;

    /* ── IDLE event — process available bytes from ring buffer ─────────── */
    /* Read bytes from g_rb_rd_idx up to wr_idx, handling ring wrap.
     *
     * The DMA runs in CIRCULAR mode. On each IDLE event, there may be
     * data in the ring buffer from g_rb_rd_idx (the last-read position)
     * up to wr_idx (the current DMA write position). Three cases exist:
     *
     *   Case A (no wrap):  g_rb_rd_idx < wr_idx
     *       Data is contiguous in [g_rb_rd_idx, wr_idx).
     *
     *   Case B (wrap):     g_rb_rd_idx > wr_idx
     *       Data wraps around the buffer boundary:
     *         Segment 1: [g_rb_rd_idx, LOG_DBG_BUF_SIZE)
     *         Segment 2: [0, wr_idx)
     *
     *   Case C (empty):    g_rb_rd_idx == wr_idx
     *       No new data — nothing to process.
     */

    /* --- Determine ring buffer read segments --- */
    uint16_t seg_start;
    uint16_t seg_end;
    int      need_second_seg = 0;

    if (g_rb_rd_idx < wr_idx)
    {
        /* Case A: single contiguous segment */
        seg_start = g_rb_rd_idx;
        seg_end   = wr_idx;
    }
    else if (g_rb_rd_idx > wr_idx)
    {
        /* Case B: two segments due to wrap */
        seg_start       = g_rb_rd_idx;
        seg_end         = LOG_DBG_BUF_SIZE;
        need_second_seg = 1;
    }
    else
    {
        /* Case C: no new data */
        goto _ringbuf_done;
    }

    /* --- Process first segment --- */
    {
        uint16_t i = seg_start;

        while (i < seg_end)
        {
            uint8_t ch = g_dma_rx_buf[i];
            g_rb_rd_idx = i + 1U;  /* Keep rd_idx in sync even on break */

            /* ── Handle Backspace (0x7F / 0x08) ─────────────────────── */
            if (ch == 0x7FU || ch == 0x08U)
            {
                if (g_dbg_pos > 0U)
                {
                    g_dbg_pos--;
                    if (g_dbg_echo != 0U)
                    {
                        uint8_t bs_seq[] = { 0x08U, 0x20U, 0x08U };
                        HAL_UART_Transmit(huart, bs_seq, 3U, 100U);
                    }
                }
                i++;
                continue;
            }

            /* ── Handle End-of-Line: \n or \r ───────────────────────── */
            if (ch == '\n' || ch == '\r')
            {
                if (g_dbg_pos > 0U)
                {
                    g_dbg_cmd_pending = 1U;
                    if (g_dbg_echo != 0U)
                    {
                        uint8_t crlf[] = { '\r', '\n' };
                        HAL_UART_Transmit(huart, crlf, 2U, 100U);
                    }
                    /* Line complete — remaining bytes belong to next
                     * line and will be processed on next IDLE event */
                    goto _ringbuf_done;
                }
                if (g_dbg_echo != 0U)
                {
                    uint8_t crlf[] = { '\r', '\n' };
                    HAL_UART_Transmit(huart, crlf, 2U, 100U);
                }
                i++;
                continue;
            }

            /* ── Handle printable characters ────────────────────────── */
            if (ch >= 0x20U && ch <= 0x7EU)
            {
                if (g_dbg_pos < (LOG_DBG_BUF_SIZE - 1U))
                {
                    g_dbg_buffer[g_dbg_pos++] = (char)ch;
                    if (g_dbg_echo != 0U)
                    {
                        HAL_UART_Transmit(huart, &g_dma_rx_buf[i], 1U, 100U);
                    }
                }
            }

            i++;
        }

        /* --- Process wrap-around segment (if needed) --- */
        if (need_second_seg)
        {
            i = 0U;
            while (i < wr_idx)
            {
                uint8_t ch = g_dma_rx_buf[i];
                g_rb_rd_idx = i + 1U;

                /* ── Handle Backspace (0x7F / 0x08) ─────────────────── */
                if (ch == 0x7FU || ch == 0x08U)
                {
                    if (g_dbg_pos > 0U)
                    {
                        g_dbg_pos--;
                        if (g_dbg_echo != 0U)
                        {
                            uint8_t bs_seq[] = { 0x08U, 0x20U, 0x08U };
                            HAL_UART_Transmit(huart, bs_seq, 3U, 100U);
                        }
                    }
                    i++;
                    continue;
                }

                /* ── Handle End-of-Line: \n or \r ───────────────────── */
                if (ch == '\n' || ch == '\r')
                {
                    if (g_dbg_pos > 0U)
                    {
                        g_dbg_cmd_pending = 1U;
                        if (g_dbg_echo != 0U)
                        {
                            uint8_t crlf[] = { '\r', '\n' };
                            HAL_UART_Transmit(huart, crlf, 2U, 100U);
                        }
                        goto _ringbuf_done;
                    }
                    if (g_dbg_echo != 0U)
                    {
                        uint8_t crlf[] = { '\r', '\n' };
                        HAL_UART_Transmit(huart, crlf, 2U, 100U);
                    }
                    i++;
                    continue;
                }

                /* ── Handle printable characters ────────────────────── */
                if (ch >= 0x20U && ch <= 0x7EU)
                {
                    if (g_dbg_pos < (LOG_DBG_BUF_SIZE - 1U))
                    {
                        g_dbg_buffer[g_dbg_pos++] = (char)ch;
                        if (g_dbg_echo != 0U)
                        {
                            HAL_UART_Transmit(huart, &g_dma_rx_buf[i], 1U, 100U);
                        }
                    }
                }

                i++;
            }
        }
    }

_ringbuf_done:
    /* DMA runs continuously in CIRCULAR mode — no restart needed here.
     * __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT) was done once in init. */
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Built-in Debug Commands                                                   */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  'help' command — list all registered commands with descriptions
  */
static void _dbg_cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    Log_Print(LOG_LEVEL_INFO, "=== Debug CLI Commands ===");
    for (uint8_t i = 0U; i < g_dbg_cmd_count; i++)
    {
        if (g_dbg_cmds[i].help != NULL)
        {
            Log_Print(LOG_LEVEL_INFO, "  %-10s — %s", g_dbg_cmds[i].name, g_dbg_cmds[i].help);
        }
        else
        {
            Log_Print(LOG_LEVEL_INFO, "  %s", g_dbg_cmds[i].name);
        }
    }
    Log_Print(LOG_LEVEL_INFO, "==========================");
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Command Parser                                                             */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  Tokenize a string into an argv-style array (in-place, modifies str)
  * @param  str      Null-terminated input string (will be modified)
  * @param  argv     Output array of string pointers
  * @param  max_args Maximum number of tokens to extract
  * @return Number of tokens found
  */
static int _dbg_tokenize(char *str, char **argv, int max_args)
{
    int argc = 0;
    char *p = str;

    while (*p != '\0' && argc < max_args)
    {
        /* Skip leading whitespace */
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }

        if (*p == '\0')
        {
            break;
        }

        /* Start of token */
        argv[argc++] = p;

        /* Skip to next whitespace or end */
        while (*p != '\0' && *p != ' ' && *p != '\t')
        {
            p++;
        }

        if (*p != '\0')
        {
            *p = '\0'; /* Terminate token */
            p++;
        }
    }

    return argc;
}

/**
  * @brief  Parse a command line and dispatch to registered handler
  * @param  line  Null-terminated command line string
  * @note   Strips trailing newline/carriage-return characters.
  *         Empty lines and comment lines (starting with '#') are ignored.
  */
static void _dbg_parse_and_execute(const char *line)
{
    if (line == NULL || *line == '\0')
    {
        return;
    }

    /* Make a mutable copy */
    char buf[LOG_DBG_BUF_SIZE];
    size_t len = strlen(line);
    if (len >= LOG_DBG_BUF_SIZE)
    {
        len = LOG_DBG_BUF_SIZE - 1U;
    }
    memcpy(buf, line, len);
    buf[len] = '\0';

    /* Strip trailing \r \n */
    while (len > 0U && (buf[len - 1U] == '\r' || buf[len - 1U] == '\n'))
    {
        buf[--len] = '\0';
    }

    /* Skip empty lines and comments */
    if (len == 0U || buf[0] == '#')
    {
        return;
    }

    /* Tokenize */
    char *argv[LOG_DBG_MAX_ARGS];
    int argc = _dbg_tokenize(buf, argv, LOG_DBG_MAX_ARGS);

    if (argc < 1)
    {
        return;
    }

    /* Look up command in registered table */
    for (uint8_t i = 0U; i < g_dbg_cmd_count; i++)
    {
        if (strcmp(argv[0], g_dbg_cmds[i].name) == 0)
        {
            g_dbg_cmds[i].func(argc, argv);
            return;
        }
    }

    /* Command not found */
    Log_Print(LOG_LEVEL_INFO, "Unknown command: '%s'. Type 'help' for available commands.", argv[0]);
}
