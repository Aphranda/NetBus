/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    log.c
  * @brief   Log module implementation -- UART7 output driver with debug CLI
  *
  *          Output interface: UART7 (PF6-RX, PF7-TX @ 115200 8N1)
  *
  *          Architecture:
  *            TX: LOG_* macros → Log_Print() → vsnprintf() → HAL_UART_Transmit()
  *            RX: UART DMA (256B ring) → ISR assembles lines →
  *                osMessageQueue(4×128B) → Log_DbgGetLine() (task consumer)
  *
  *          Compile-time filtering via LOG_LEVEL eliminates dead code.
  *          Runtime filtering via g_log_config.level skips unwanted output.
  *
  *          Debug Commands (built-in):
  *            help           -- Show available commands
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
#include "cmsis_os2.h"
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
  * @brief DMA ring buffer and message queue sizing
  */
#define LOG_DBG_BUF_SIZE    256U  /*!< DMA RX ring buffer (bytes)        */
#define LOG_DBG_LINE_LEN    128U  /*!< Max line length in message queue  */
#define LOG_DBG_QUEUE_SIZE     8  /*!< Message queue capacity (lines)    */
#define LOG_DBG_MAX_CMDS     10U  /*!< Maximum registered commands       */
#define LOG_DBG_MAX_ARGS      8U  /*!< Maximum arguments per command     */

/**
  * @brief Level tag strings for human-readable prefix
  */
static const char * const LOG_TAG[] = {
    [LOG_LEVEL_NONE]    = "",          /* RAW mode -- no tag */
    [LOG_LEVEL_ERROR]   = "[ERR] ",
    [LOG_LEVEL_WARN]    = "[WARN] ",
    [LOG_LEVEL_INFO]    = "[INFO] ",
    [LOG_LEVEL_DEBUG]   = "[DEBUG] ",
    [LOG_LEVEL_VERBOSE] = "[VERB] ",
};

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/**
  * @brief Default log configuration -- UART7, 100ms timeout, all levels, timestamp enabled
  */
static Log_Config_t g_log_config = {
    .huart     = NULL,     /* Will be set to &huart7 in Log_Init() */
    .timeout   = 100U,
    .level     = LOG_LEVEL_VERBOSE,
    .enable_ts = 1U,
};

/* ── Debug command subsystem ──────────────────────────────────────────────── */

/**
  * @brief Debug command table -- stores registered commands
  */
static Log_DbgCmdEntry_t g_dbg_cmds[LOG_DBG_MAX_CMDS];
static uint8_t           g_dbg_cmd_count = 0U;

/**
  * @brief Debug CLI state
  */
static uint8_t g_dbg_echo    = 0U;   /*!< Echo mode (default: off)         */
static uint8_t g_dbg_enabled = 1U;   /*!< Debug CLI fallback (default: on) */

/**
  * @brief Mutex protecting HAL_UART_Transmit from concurrent task access.
  * @note  Serializes UART TX without disabling IRQs, so RX IDLE interrupts
  *        can still fire during TX -- preventing DMA ring-buffer overrun.
  */
static osMutexId_t g_uart_tx_mutex = NULL;

/**
  * @brief Message queue -- ISR pushes completed lines, task consumer pops them.
  *        Decouples ISR timing from task processing speed.
  */
static osMessageQueueId_t g_line_queue = NULL;

/**
  * @brief DMA RX ring buffer -- receives UART data via DMA in CIRCULAR mode.
  *        DMA continuously writes incoming data; IDLE ISR reads and assembles
  *        lines, pushing them into g_line_queue.
  */
static uint8_t  g_dma_rx_buf[LOG_DBG_BUF_SIZE] __attribute__((aligned(32)));
static uint16_t g_rb_rd_idx;            /*!< Ring buffer read index */

/* Private function prototypes -----------------------------------------------*/

static int  _log_write_timestamp(char *buf, size_t size);

/* ── Debug command built-in handlers ──────────────────────────────────────── */

static void _dbg_cmd_help(int argc, char **argv);

/* ── Debug command internal helpers ───────────────────────────────────────── */

static void _dbg_parse_and_execute(const char *line);
static int  _dbg_tokenize(char *str, char **argv, int max_args);

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize log module with defaults (UART7, verbose level, timestamp on)
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

    /* ── Create UART TX mutex (one-time) ──────────────────────────────────── */
    if (g_uart_tx_mutex == NULL)
    {
        g_uart_tx_mutex = osMutexNew(NULL);
    }

    /* ── Create line message queue (one-time, or re-create if re-init) ────── */
    if (g_line_queue != NULL)
    {
        osMessageQueueDelete(g_line_queue);
    }
    g_line_queue = osMessageQueueNew(LOG_DBG_QUEUE_SIZE, LOG_DBG_LINE_LEN, NULL);
    if (g_line_queue == NULL)
    {
        return HAL_ERROR;
    }

    /* ── Initialize debug command subsystem ───────────────────────────────── */
    g_dbg_echo        = 0U;
    g_dbg_enabled     = 1U;
    g_rb_rd_idx       = 0U;

    /* Clear command table and register built-in commands */
    g_dbg_cmd_count = 0U;
    (void)Log_RegisterDbgCmd("help", _dbg_cmd_help);

    /* Start UART RX via DMA with IDLE line detection (CIRCULAR mode).
     * DMA continuously receives bytes into g_dma_rx_buf ring buffer.
     * HAL_UARTEx_RxEventCallback() assembles lines and pushes them to
     * g_line_queue. DMA keeps running -- no restart needed. */
    if (HAL_UARTEx_ReceiveToIdle_DMA(g_log_config.huart,
            g_dma_rx_buf, LOG_DBG_BUF_SIZE) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Disable DMA half-transfer interrupt -- only IDLE/TC events matter */
    __HAL_DMA_DISABLE_IT(g_log_config.huart->hdmarx, DMA_IT_HT);

    Log_Print(LOG_LEVEL_INFO, "Log module initialized (UART7 @ 115200 8N1, DMA+IDLE, queue)");
    Log_Print(LOG_LEVEL_INFO, "Debug CLI ready -- type 'help' for commands");

    return HAL_OK;
}

/**
  * @brief  Core print function -- formatted output via UART7
  */
void Log_Print(uint8_t level, const char *fmt, ...)
{
    char    buffer[LOG_MAX_MSG_LEN];
    int     pos = 0;
    va_list args;

    if (level > g_log_config.level)  return;
    if (g_log_config.huart == NULL)  return;

    /* ── Build prefix: [timestamp] [TAG] ────────────────────────────────── */
    if (g_log_config.enable_ts != 0U)
    {
        pos += _log_write_timestamp(&buffer[pos], sizeof(buffer) - (size_t)pos);
    }

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

    if (pos >= (int)sizeof(buffer)) pos = (int)sizeof(buffer) - 1;
    buffer[pos] = '\0';

    /* ── Transmit via UART (mutex-protected, IRQs remain enabled) ────────── */
    if (g_uart_tx_mutex != NULL) osMutexAcquire(g_uart_tx_mutex, osWaitForever);
    HAL_UART_Transmit(g_log_config.huart, (uint8_t *)buffer, (uint16_t)pos, g_log_config.timeout);
    if (g_uart_tx_mutex != NULL) osMutexRelease(g_uart_tx_mutex);
}

/**
  * @brief  Write raw data to UART -- no prefix, timestamp, or formatting
  * @note   Used by SCPI_Write() for clean SCPI protocol responses.
  */
void Log_WriteRaw(const char *data, size_t len)
{
    if (g_log_config.huart == NULL || data == NULL || len == 0U) return;

    if (g_uart_tx_mutex != NULL) osMutexAcquire(g_uart_tx_mutex, osWaitForever);
    HAL_UART_Transmit(g_log_config.huart, (uint8_t *)data, (uint16_t)len, g_log_config.timeout);
    if (g_uart_tx_mutex != NULL) osMutexRelease(g_uart_tx_mutex);
}

/**
  * @brief  Flush log output -- no-op (blocking TX already guarantees completion)
  */
void Log_Flush(void) {}

/**
  * @brief  Set runtime log level threshold
  */
void Log_SetLevel(uint8_t level)
{
    if (level <= LOG_LEVEL_VERBOSE) g_log_config.level = level;
}

/**
  * @brief  Get current runtime log level
  */
uint8_t Log_GetLevel(void)
{
    return g_log_config.level;
}

/* ── Debug command exported API ───────────────────────────────────────────── */

/**
  * @brief  Non-blocking read of one complete line from the RX message queue.
  * @note   Each line is consumed on read -- no separate "consume" step needed.
  */
uint8_t Log_DbgGetLine(char *buf, uint8_t maxlen)
{
    if (g_line_queue == NULL || buf == NULL || maxlen == 0U) return 0U;

    char line[LOG_DBG_LINE_LEN];
    if (osMessageQueueGet(g_line_queue, line, NULL, 0U) != osOK)
    {
        return 0U;  /* Queue empty */
    }

    size_t len = strlen(line);
    if (len >= (size_t)maxlen) len = (size_t)maxlen - 1U;
    memcpy(buf, line, len);
    buf[len] = '\0';
    return (uint8_t)len;
}

/**
  * @brief  Process a command line through the debug CLI parser.
  */
void Log_DbgProcessLine(const char *line)
{
    _dbg_parse_and_execute(line);
}

/**
  * @brief  Register a custom debug command
  */
HAL_StatusTypeDef Log_RegisterDbgCmd(const char *cmd, Log_DbgCmdFunc_t func)
{
    return Log_RegisterDbgCmdEx(cmd, func, NULL);
}

/**
  * @brief  Register a custom debug command with help description
  */
HAL_StatusTypeDef Log_RegisterDbgCmdEx(const char *cmd, Log_DbgCmdFunc_t func, const char *help)
{
    if (cmd == NULL || func == NULL) return HAL_ERROR;
    if (g_dbg_cmd_count >= LOG_DBG_MAX_CMDS) return HAL_ERROR;

    g_dbg_cmds[g_dbg_cmd_count].name = cmd;
    g_dbg_cmds[g_dbg_cmd_count].func = func;
    g_dbg_cmds[g_dbg_cmd_count].help = help;
    g_dbg_cmd_count++;

    return HAL_OK;
}

/**
  * @brief  Enable or disable UART echo (deferred to task context, not ISR)
  */
void Log_DbgSetEcho(uint8_t enable)
{
    g_dbg_echo = (enable != 0U) ? 1U : 0U;
}

/**
  * @brief  Push a line into the message queue for SCPI/debug CLI processing.
  * @note   Used by NetSCPI (TCP SCPI) and programmatic injection.
  */
void Log_DbgInject(const char *line)
{
    if (g_line_queue == NULL || line == NULL) return;

    char msg[LOG_DBG_LINE_LEN];
    size_t len = strlen(line);
    if (len >= LOG_DBG_LINE_LEN) len = LOG_DBG_LINE_LEN - 1U;
    memcpy(msg, line, len);
    msg[len] = '\0';

    osMessageQueuePut(g_line_queue, msg, 0U, 0U);
}

/**
  * @brief  Enable or disable debug CLI fallback mode
  */
void Log_DbgSetEnabled(uint8_t enable)
{
    g_dbg_enabled = (enable != 0U) ? 1U : 0U;
}

/**
  * @brief  Query whether debug CLI fallback is enabled
  */
uint8_t Log_DbgIsEnabled(void)
{
    return g_dbg_enabled;
}

/**
  * @brief  Check if a command line starts with a registered debug command
  */
uint8_t Log_DbgIsKnownCommand(const char *line)
{
    if (line == NULL || *line == '\0') return 0U;

    /* Extract first token */
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0') return 0U;
    const char *start = p;
    while (*p != '\0' && *p != ' ' && *p != '\t') p++;
    size_t token_len = (size_t)(p - start);
    if (token_len == 0U) return 0U;

    /* Look up in registered commands */
    for (uint8_t i = 0U; i < g_dbg_cmd_count; i++)
    {
        if (strlen(g_dbg_cmds[i].name) == token_len &&
            strncmp(g_dbg_cmds[i].name, start, token_len) == 0)
        {
            return 1U;
        }
    }
    return 0U;
}

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Write timestamp "[12345] " into buffer using HAL_GetTick()
  */
static int _log_write_timestamp(char *buf, size_t size)
{
    if (buf == NULL || size < 4U) return 0;

    int written = snprintf(buf, size, "[%lu] ", HAL_GetTick());
    return (written < 0) ? 0 : written;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  HAL UART RX Event callback -- DMA ring buffer → message queue              */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  UART RX Event callback (HAL weak override)
  * @note   Called by HAL on IDLE line detection or DMA TC.
  *         Reads bytes from the DMA CIRCULAR ring buffer, assembles them into
  *         lines using a local static buffer, and pushes completed lines into
  *         g_line_queue (ISR-safe with timeout=0).
  *
  *         If the queue is full, the oldest line is silently dropped via
  *         osMessageQueuePut(..., 0) which returns osErrorResource.
  *         Data stays in the DMA ring buffer and will be picked up on
  *         subsequent IDLE events.
  */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    (void)Size;

    /* ── Dispatch UART8 (RS485) to its own handler ─────────────────────── */
    if (huart == &huart8)
    {
        RS485_UART_RxEventCallback(huart, Size);
        return;
    }

    /* Only process UART7 -- the log/debug UART */
    if (huart != g_log_config.huart) return;

    /* ── Invalidate D-Cache for DMA buffer ──────────────────────────────── */
    /* DMA writes directly to SRAM, CPU reads via D-Cache. Without invalidation,
     * the CPU may read stale cached data instead of fresh DMA-written bytes,
     * causing character loss (e.g. "help" → "lp"). */
    SCB_InvalidateDCache_by_Addr((uint32_t *)g_dma_rx_buf, LOG_DBG_BUF_SIZE);

    /* ── Local static line assembly buffer (ISR-private, no concurrent access) */
    static char line_buf[LOG_DBG_LINE_LEN];
    static uint8_t line_pos = 0U;

    /* ── Compute DMA write position ──────────────────────────────────────── */
    uint16_t ndtr   = (uint16_t)__HAL_DMA_GET_COUNTER(huart->hdmarx);
    uint16_t wr_idx = LOG_DBG_BUF_SIZE - ndtr;
    if (wr_idx >= LOG_DBG_BUF_SIZE) wr_idx = 0U;

    /* ── Process bytes from ring buffer ──────────────────────────────────── */
    /* Handle both contiguous (rd < wr) and wrap (rd > wr) cases */
    uint16_t seg_start, seg_end;
    int      second_seg = 0;

    if (g_rb_rd_idx < wr_idx)
    {
        seg_start = g_rb_rd_idx;
        seg_end   = wr_idx;
    }
    else if (g_rb_rd_idx > wr_idx)
    {
        seg_start = g_rb_rd_idx;
        seg_end   = LOG_DBG_BUF_SIZE;
        second_seg = 1;
    }
    else
    {
        return;  /* No new data */
    }

    for (int seg = 0; seg <= second_seg; seg++)
    {
        uint16_t seg_len = (seg == 0) ? (seg_end - seg_start) : wr_idx;
        for (uint16_t j = 0U; j < seg_len; j++)
        {
            uint8_t ch = g_dma_rx_buf[(seg == 0) ? (seg_start + j) : j];

            if (ch == 0x7FU || ch == 0x08U)  /* Backspace */
            {
                if (line_pos > 0U) line_pos--;
                continue;
            }

            if (ch == '\n' || ch == '\r')
            {
                if (line_pos > 0U)
                {
                    line_buf[line_pos] = '\0';
                    line_pos = 0U;
                    /* Push to queue -- drop if full (timeout=0, non-blocking) */
                    osMessageQueuePut(g_line_queue, line_buf, 0U, 0U);
                }
                continue;
            }

            /* Printable characters */
            if (ch >= 0x20U && ch <= 0x7EU && line_pos < (LOG_DBG_LINE_LEN - 1U))
            {
                line_buf[line_pos++] = (char)ch;
            }
        }

        if (seg == 0) seg_start = 0U;  /* Prepare for second segment */
    }

    /* Update read index to current DMA position */
    g_rb_rd_idx = wr_idx;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Built-in Debug Commands                                                   */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  'help' command -- list all registered commands with descriptions
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
            Log_Print(LOG_LEVEL_INFO, "  %-10s -- %s", g_dbg_cmds[i].name, g_dbg_cmds[i].help);
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
  */
static int _dbg_tokenize(char *str, char **argv, int max_args)
{
    int argc = 0;
    char *p = str;

    while (*p != '\0' && argc < max_args)
    {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        argv[argc++] = p;

        while (*p != '\0' && *p != ' ' && *p != '\t') p++;

        if (*p != '\0') { *p = '\0'; p++; }
    }

    return argc;
}

/**
  * @brief  Parse a command line and dispatch to registered handler
  */
static void _dbg_parse_and_execute(const char *line)
{
    if (line == NULL || *line == '\0') return;

    char buf[LOG_DBG_LINE_LEN];
    size_t len = strlen(line);
    if (len >= LOG_DBG_LINE_LEN) len = LOG_DBG_LINE_LEN - 1U;
    memcpy(buf, line, len);
    buf[len] = '\0';

    /* Strip trailing \r \n */
    while (len > 0U && (buf[len - 1U] == '\r' || buf[len - 1U] == '\n'))
        buf[--len] = '\0';

    if (len == 0U || buf[0] == '#') return;

    /* Tokenize */
    char *argv[LOG_DBG_MAX_ARGS];
    int argc = _dbg_tokenize(buf, argv, LOG_DBG_MAX_ARGS);

    if (argc < 1) return;

    /* Look up command in registered table */
    for (uint8_t i = 0U; i < g_dbg_cmd_count; i++)
    {
        if (strcmp(argv[0], g_dbg_cmds[i].name) == 0)
        {
            g_dbg_cmds[i].func(argc, argv);
            return;
        }
    }

    Log_Print(LOG_LEVEL_INFO, "Unknown command: '%s'. Type 'help' for available commands.", argv[0]);
}
