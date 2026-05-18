/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    log_task.c
  * @brief   Log task implementation — wraps ThirdParty/Log as App_Module_t
  *
  *          Registration order matters:
  *            Log_Task must be registered FIRST so that other modules can
  *            use LOG_INFO/WARN/ERROR during their own initialization.
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
#include "log_task.h"
#include "can.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/**
  * @brief  CAN SN query: maximum timeout waiting for response (ms)
  */
#define CANSN_TIMEOUT_MS      1000U

/**
  * @brief  CAN send (raw): maximum timeout waiting for response (ms)
  */
#define CANRESP_TIMEOUT_MS    500U

/**
  * @brief  CAN SN query: maximum SN string length (per protocol)
  */
#define CANSN_MAX_SN_LEN      64U

/**
  * @brief  Maximum number of CAN raw data bytes per debug CLI command
  */
#define CANRAW_MAX_BYTES      CAN_MAX_DATA_LEN

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/**
  * @brief  Log task initialization — calls Log_Init()
  * @retval APP_OK on success, APP_ERROR on failure
  */
static App_Status_t _log_task_init(void);

/**
  * @brief  Log task periodic process — no-op (log is event-driven)
  * @retval APP_OK
  */
static App_Status_t _log_task_process(void);

/**
  * @brief  Log task error handler — called if Log_Init() fails
  * @param  err  Error status code
  */
static void _log_task_on_error(App_Status_t err);

/**
  * @brief  Debug CLI handler for "cansn" — query CAN device SN via 0x101
  * @param  argc  Argument count
  * @param  argv  Argument vector (argv[0]="cansn", argv[1]=node_id)
  */
static void _dbg_cmd_cansn(int argc, char **argv);

/**
  * @brief  Debug CLI handler for "cansend" — send raw CAN frame
  * @param  argc  Argument count
  * @param  argv  Argument vector
  */
static void _dbg_cmd_cansend(int argc, char **argv);

/**
  * @brief  Debug CLI handler for "canscan" — scan CAN bus for active nodes
  * @param  argc  Argument count
  * @param  argv  Argument vector
  */
static void _dbg_cmd_canscan(int argc, char **argv);

/**
  * @brief  Convert a hex string (e.g. "0x1A", "1A") to a byte
  * @param  hex  Null-terminated hex string
  * @param  out  Output byte
  * @retval 0 on success, -1 on error
  */
static int _can_hex_to_byte(const char *hex, uint8_t *out);

/**
  * @brief  Print hex dump to Log console
  * @param  prefix  Label prefix
  * @param  data    Data buffer
  * @param  len     Length of data
  */
static void _can_print_hex(const char *prefix, const uint8_t *data, uint16_t len);

/* Exported variables --------------------------------------------------------*/

/**
  * @brief  Log task module descriptor — register via App_RegisterModule()
  * @note   Must be the first module registered, so Log is active before
  *         any other module's init() runs.
  */
const App_Module_t g_log_task_module = {
    .name     = "Log",
    .init     = _log_task_init,
    .process  = _log_task_process,
    .on_error = _log_task_on_error,
};

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Set runtime log level
  * @param  level  LOG_LEVEL_* value (0=NONE .. 5=VERBOSE)
  */
void Log_Task_SetLevel(uint8_t level)
{
    Log_SetLevel(level);
    LOG_INFO("Log_Task: level set to %u", (unsigned int)level);
}

/**
  * @brief  Get current log level
  * @retval Current LOG_LEVEL_* value
  */
uint8_t Log_Task_GetLevel(void)
{
    return Log_GetLevel();
}

/**
  * @brief  Re-initialize Log subsystem with optional new config
  * @param  config  New config pointer, or NULL for defaults
  */
void Log_Task_ReInit(const Log_Config_t *config)
{
    if (config != NULL)
    {
        if (Log_InitEx(config) == HAL_OK)
        {
            LOG_INFO("Log_Task: re-initialized with custom config");
        }
    }
    else
    {
        if (Log_Init() == HAL_OK)
        {
            LOG_INFO("Log_Task: re-initialized (defaults)");
        }
    }
}

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initialize the Log subsystem (binds UART7)
  * @note   Called by App_Task_Init() during module scan
  * @retval APP_OK on success, APP_ERROR on failure
  */
static App_Status_t _log_task_init(void)
{
    if (Log_Init() != HAL_OK)
    {
        return APP_ERROR;
    }

    /* ── Initialize CAN driver (FDCAN1, classic CAN mode) ─────────────── */
    if (CAN_Init() != HAL_OK)
    {
        LOG_ERROR("Log_Task: CAN_Init() failed");
        return APP_ERROR;
    }
    LOG_INFO("Log_Task: CAN initialized (FDCAN1, Classic CAN)");

    /* ── Register "cansn" debug CLI command ───────────────────────────── */
    if (Log_RegisterDbgCmd("cansn", _dbg_cmd_cansn) != HAL_OK)
    {
        LOG_ERROR("Log_Task: failed to register 'cansn' debug command");
        return APP_ERROR;
    }

    /* ── Register "cansend" debug CLI command ─────────────────────────── */
    if (Log_RegisterDbgCmd("cansend", _dbg_cmd_cansend) != HAL_OK)
    {
        LOG_ERROR("Log_Task: failed to register 'cansend' debug command");
        return APP_ERROR;
    }

    /* ── Register "canscan" debug CLI command ─────────────────────────── */
    if (Log_RegisterDbgCmd("canscan", _dbg_cmd_canscan) != HAL_OK)
    {
        LOG_ERROR("Log_Task: failed to register 'canscan' debug command");
        return APP_ERROR;
    }

    LOG_INFO("Log_Task: type 'cansn <node_id>' to query CAN device SN");
    LOG_INFO("Log_Task: type 'cansend <id> <hex...>' to send raw CAN frame");
    LOG_INFO("Log_Task: type 'canscan [start] [end]' to scan for CAN nodes");

    return APP_OK;
}

/**
  * @brief  Log task periodic process — currently no-op
  * @note   Log is event-driven (called by other modules via LOG_* macros).
  *         Reserved for future use (e.g., periodic flush or stats).
  * @retval APP_OK
  */
static App_Status_t _log_task_process(void)
{
    /* Process any pending debug commands from UART RX */
    Log_DbgProcess();

    return APP_OK;
}

/**
  * @brief  Log task error handler
  * @param  err  Error code from init
  * @note   Cannot use LOG_* here — Log itself may be the cause of failure.
  *         Execution will reach Error_Handler() in main.c if init fails.
  */
static void _log_task_on_error(App_Status_t err)
{
    (void)err;
    /* Log subsystem unavailable — no output possible */
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Debug CLI — "cansn" command                                                 */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  'cansn' debug CLI command — query CAN device SN via 0x101
  *
  *         Usage:
  *           cansn <node_id>
  *             - Send CAN ID 0x101 with Byte0 = node_id to query SN
  *             - Wait for response and print the SN string
  *             - Example: "cansn 1"
  *
  *         Protocol (from Doc/V3.x A1检波板指令.docx):
  *           0x101 读取 SN:
  *             请求帧 ID=0x101, Data[0]=Node ID
  *             响应帧 ID=NodeID, Byte0=0x01, Byte1=0x00(成功),
  *             Byte2=SN长度, Byte3+=SN字符串内容(不带\0)
  */
static void _dbg_cmd_cansn(int argc, char **argv)
{
    uint32_t tick_start;
    CAN_Msg_t tx_msg;
    CAN_Msg_t rx_msg;

    if (argc < 2)
    {
        LOG_INFO("Usage: cansn <node_id>");
        LOG_INFO("  Query CAN device SN via CAN ID 0x101");
        LOG_INFO("  Example: cansn 1");
        return;
    }

    /* ── Parse Node ID ──────────────────────────────────────────────────── */
    char *endptr = NULL;
    long node_id = strtol(argv[1], &endptr, 0);

    if (endptr == argv[1] || *endptr != '\0')
    {
        LOG_INFO("Error: invalid node ID '%s' (must be numeric)", argv[1]);
        return;
    }

    if (node_id < 0 || node_id > 0xFF)
    {
        LOG_INFO("Error: node ID out of range (0-255): %ld", node_id);
        return;
    }

    LOG_INFO("CAN SN: querying SN from node %ld ...", node_id);

    /* ── Build and send the 0x101 SN query frame ───────────────────────── */
    tx_msg.id   = 0x101U;
    tx_msg.dlc  = 1U;  /* Only Byte0: Node ID */
    tx_msg.data[0] = (uint8_t)(node_id & 0xFFU);

    /* Print the outgoing CAN frame before sending */
    LOG_INFO("CAN SN: sending ID=0x%03lX, DLC=%u",
             (unsigned long)tx_msg.id, (unsigned)tx_msg.dlc);
    _can_print_hex("CAN SN TX", tx_msg.data, tx_msg.dlc);

    if (CAN_Send(&tx_msg) != HAL_OK)
    {
        LOG_ERROR("CAN SN: CAN_Send() failed");
        return;
    }

    /* ── Poll for response with timeout ─────────────────────────────────── */
    tick_start = HAL_GetTick();

    while ((HAL_GetTick() - tick_start) < CANSN_TIMEOUT_MS)
    {
        if (CAN_GetRxMessage(&rx_msg) == HAL_OK)
        {
            /* Check if this is a response to our SN query:
             *   - Response CAN ID should match the target node ID
             *   - Byte0 should be 0x01 (command echo low byte of 0x101)
             *   - Byte1 should be 0x00 (success) */
            if (rx_msg.id == (uint32_t)node_id &&
                rx_msg.dlc >= 3U &&
                rx_msg.data[0] == 0x01U)
            {
                if (rx_msg.data[1] == 0x00U)
                {
                    /* Success — extract SN string */
                    uint8_t sn_len = rx_msg.data[2];
                    if (sn_len > CANSN_MAX_SN_LEN)
                    {
                        sn_len = CANSN_MAX_SN_LEN;
                    }

                    /* Build null-terminated SN string for safe printing */
                    char sn_str[CANSN_MAX_SN_LEN + 1U];
                    for (uint8_t i = 0U; i < sn_len; i++)
                    {
                        sn_str[i] = (char)rx_msg.data[3U + i];
                    }
                    sn_str[sn_len] = '\0';

                    LOG_INFO("CAN SN: SUCCESS - node %ld SN = '%s' (len=%u)",
                             node_id, sn_str, (unsigned)sn_len);

                    /* Also print hex dump for debugging */
                    {
                        char hex_buf[256U];
                        int pos = 0;
                        for (uint8_t i = 0U; i < sn_len; i++)
                        {
                            pos += snprintf(&hex_buf[pos],
                                            (size_t)(sizeof(hex_buf) - (size_t)pos - 1U),
                                            "%02X ", (unsigned)(uint8_t)sn_str[i]);
                            if (pos >= (int)(sizeof(hex_buf) - 4))
                            {
                                break;
                            }
                        }
                        if (pos > 0)
                        {
                            hex_buf[pos - 1] = '\0';
                        }
                        LOG_INFO("CAN SN: SN hex = %s", hex_buf);
                    }
                }
                else
                {
                    LOG_ERROR("CAN SN: node %ld returned error (result=0x%02X)",
                              node_id, (unsigned)rx_msg.data[1]);
                }
                return;
            }
            else
            {
                /* Not our response — ignore and keep polling */
                LOG_VERBOSE("CAN SN: ignoring unrelated msg id=0x%03X", rx_msg.id);
            }
        }
    }

    /* ── Timeout ────────────────────────────────────────────────────────── */
    LOG_ERROR("CAN SN: TIMEOUT - no response from node %ld within %u ms",
              node_id, (unsigned)CANSN_TIMEOUT_MS);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Debug CLI — "cansend" command                                               */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  'cansend' debug CLI command — send raw CAN frame
  *
  *         Usage:
  *           cansend <id> <hex bytes...>
  *             - Send a raw CAN frame with specified ID and data bytes
  *             - ID: CAN identifier (decimal or 0x hex), 0x000-0x7FF
  *             - hex bytes: space-separated hex byte values (0-8 bytes)
  *             - Example: cansend 0x101 01          (SN query to node 1)
  *             - Example: cansend 0x101 01 02 03    (3-byte data frame)
  */
static void _dbg_cmd_cansend(int argc, char **argv)
{
    CAN_Msg_t tx_msg;
    uint8_t   data_buf[CANRAW_MAX_BYTES];
    uint8_t   data_len = 0U;

    if (argc < 3)
    {
        LOG_INFO("Usage: cansend <id> <hex bytes...>");
        LOG_INFO("  Send a raw CAN frame with specified ID and data");
        LOG_INFO("  Examples:");
        LOG_INFO("    cansend 0x101 01           -- SN query to node 1");
        LOG_INFO("    cansend 0x101 01 02 03     -- 3-byte frame");
        LOG_INFO("    cansend 0x105 01 01 0xE8   -- LED blink 1000ms");
        return;
    }

    /* ── Parse CAN ID ──────────────────────────────────────────────────── */
    char *endptr = NULL;
    long can_id = strtol(argv[1], &endptr, 0);

    if (endptr == argv[1] || *endptr != '\0')
    {
        LOG_INFO("Error: invalid CAN ID '%s' (must be numeric, e.g. 0x101)", argv[1]);
        return;
    }

    if (can_id < 0 || can_id > (long)CAN_STD_ID_MASK)
    {
        LOG_INFO("Error: CAN ID out of range (0x000-0x7FF): %ld", can_id);
        return;
    }

    /* ── Parse data bytes ──────────────────────────────────────────────── */
    for (int i = 2; i < argc && data_len < CANRAW_MAX_BYTES; i++)
    {
        uint8_t byte;
        if (_can_hex_to_byte(argv[i], &byte) != 0)
        {
            LOG_INFO("Error: invalid hex byte '%s' at position %d", argv[i], i - 1);
            return;
        }
        data_buf[data_len++] = byte;
    }

    if (data_len == 0U)
    {
        LOG_INFO("Error: no data bytes specified");
        return;
    }

    /* ── Build and send CAN frame ──────────────────────────────────────── */
    tx_msg.id  = (uint32_t)(can_id & CAN_STD_ID_MASK);
    tx_msg.dlc = data_len;
    memcpy(tx_msg.data, data_buf, data_len);

    LOG_INFO("CAN Send: ID=0x%03lX, DLC=%u", (unsigned long)tx_msg.id, (unsigned)tx_msg.dlc);
    _can_print_hex("CAN TX", data_buf, data_len);

    if (CAN_Send(&tx_msg) != HAL_OK)
    {
        LOG_ERROR("CAN Send: CAN_Send() failed");
        return;
    }

    LOG_INFO("CAN Send: frame sent, listening for response (%u ms)...",
             (unsigned)CANRESP_TIMEOUT_MS);

    /* ── Poll for response with timeout ────────────────────────────────── */
    {
        CAN_Msg_t rx_msg;
        uint32_t  tick_start = HAL_GetTick();
        uint32_t  resp_count = 0U;

        while ((HAL_GetTick() - tick_start) < CANRESP_TIMEOUT_MS)
        {
            if (CAN_GetRxMessage(&rx_msg) == HAL_OK)
            {
                resp_count++;
                LOG_INFO("CAN Response #%lu: ID=0x%03lX, DLC=%u",
                         (unsigned long)resp_count,
                         (unsigned long)rx_msg.id,
                         (unsigned)rx_msg.dlc);
                _can_print_hex("CAN RX", rx_msg.data, rx_msg.dlc);
            }
        }

        if (resp_count == 0U)
        {
            LOG_INFO("CAN Send: no response received within %u ms",
                     (unsigned)CANRESP_TIMEOUT_MS);
        }
        else
        {
            LOG_INFO("CAN Send: received %lu response(s)", (unsigned long)resp_count);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Debug CLI — "canscan" command                                               */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  'canscan' debug CLI command — scan CAN bus for active nodes
  *
  *         Usage:
  *           canscan [start_id] [end_id]
  *             - Scan CAN bus by sending 0x101 SN queries to a range of node IDs
  *             - Default range: 1-40 (per A1 protocol: Node ID limit)
  *             - Reports which nodes respond and their SN strings
  *             - Example: canscan           (scan nodes 1-40)
  *             - Example: canscan 1 10      (scan nodes 1-10)
  *
  *         Algorithm:
  *           1. Rapidly send 0x101 query for each node ID in range
  *           2. Collect all responses within a total timeout window
  *           3. Report responding nodes with their SN
  */
static void _dbg_cmd_canscan(int argc, char **argv)
{
    long start_id = 1L;
    long end_id   = 40L;

    /* ── Parse optional range arguments ─────────────────────────────────── */
    if (argc >= 2)
    {
        char *endptr = NULL;
        start_id = strtol(argv[1], &endptr, 0);
        if (endptr == argv[1] || *endptr != '\0')
        {
            LOG_INFO("Error: invalid start ID '%s'", argv[1]);
            return;
        }
    }

    if (argc >= 3)
    {
        char *endptr = NULL;
        end_id = strtol(argv[2], &endptr, 0);
        if (endptr == argv[2] || *endptr != '\0')
        {
            LOG_INFO("Error: invalid end ID '%s'", argv[2]);
            return;
        }
    }

    /* Validate range */
    if (start_id < 0 || start_id > 0xFF)
    {
        LOG_INFO("Error: start ID out of range (0-255): %ld", start_id);
        return;
    }
    if (end_id < 0 || end_id > 0xFF)
    {
        LOG_INFO("Error: end ID out of range (0-255): %ld", end_id);
        return;
    }
    if (start_id > end_id)
    {
        LOG_INFO("Error: start ID (%ld) > end ID (%ld)", start_id, end_id);
        return;
    }

    long count = end_id - start_id + 1L;
    LOG_INFO("CAN Scan: scanning nodes %ld-%ld (%ld nodes) ...",
             start_id, end_id, count);

    /* ── Stage 1: Send 0x101 SN queries for all node IDs rapidly ────────── */
    {
        CAN_Msg_t tx_msg;
        tx_msg.id  = 0x101U;
        tx_msg.dlc = 1U;
        uint32_t send_start = HAL_GetTick();
        uint32_t send_count = 0U;

        LOG_INFO("CAN Scan: sending queries (ID=0x%03lX, DLC=%u) for nodes %ld-%ld ...",
                 (unsigned long)tx_msg.id, (unsigned)tx_msg.dlc,
                 start_id, end_id);

        for (long id = start_id; id <= end_id; id++)
        {
            tx_msg.data[0] = (uint8_t)(id & 0xFFU);

            /* Print each outgoing query frame before sending */
            _can_print_hex("CAN Scan TX", tx_msg.data, tx_msg.dlc);

            if (CAN_Send(&tx_msg) == HAL_OK)
            {
                send_count++;
            }

            /* Small yield every 10 queries to let CAN controller breathe */
            if ((send_count % 10U) == 0U)
            {
                HAL_Delay(1U);
            }
        }

        LOG_INFO("CAN Scan: sent %lu queries in %lu ms",
                 (unsigned long)send_count,
                 (unsigned long)(HAL_GetTick() - send_start));
    }

    /* ── Stage 2: Collect responses with timeout ───────────────────────── */
    {
        uint32_t scan_timeout = 500U;  /* Total 500ms to collect responses */
        uint32_t tick_start   = HAL_GetTick();
        uint32_t found_count  = 0U;
        CAN_Msg_t rx_msg;

        LOG_INFO("CAN Scan: listening for responses (%u ms)...",
                 (unsigned)scan_timeout);

        while ((HAL_GetTick() - tick_start) < scan_timeout)
        {
            if (CAN_GetRxMessage(&rx_msg) == HAL_OK)
            {
                /* Check if this is a valid 0x101 response:
                 *   - Response CAN ID should be within our scan range
                 *   - Byte0 == 0x01 (command echo)
                 *   - Byte1 == 0x00 (success) */
                uint32_t resp_node = rx_msg.id;
                if (resp_node >= (uint32_t)start_id &&
                    resp_node <= (uint32_t)end_id &&
                    rx_msg.dlc >= 3U &&
                    rx_msg.data[0] == 0x01U)
                {
                    found_count++;

                    if (rx_msg.data[1] == 0x00U)
                    {
                        /* Success — extract SN */
                        uint8_t sn_len = rx_msg.data[2];
                        if (sn_len > CANSN_MAX_SN_LEN)
                        {
                            sn_len = CANSN_MAX_SN_LEN;
                        }
                        char sn_str[CANSN_MAX_SN_LEN + 1U];
                        for (uint8_t i = 0U; i < sn_len; i++)
                        {
                            sn_str[i] = (char)rx_msg.data[3U + i];
                        }
                        sn_str[sn_len] = '\0';

                        LOG_INFO("  Node %lu: FOUND, SN='%s' (len=%u)",
                                 (unsigned long)resp_node, sn_str, (unsigned)sn_len);
                    }
                    else
                    {
                        LOG_INFO("  Node %lu: FOUND (result=0x%02X, no SN data)",
                                 (unsigned long)resp_node,
                                 (unsigned)rx_msg.data[1]);
                    }
                }
                else
                {
                    /* Not our scan response — ignore */
                    LOG_VERBOSE("CAN Scan: ignoring msg id=0x%03X",
                                rx_msg.id);
                }
            }
        }

        if (found_count == 0U)
        {
            LOG_INFO("CAN Scan: no nodes found in range %ld-%ld",
                     start_id, end_id);
        }
        else
        {
            LOG_INFO("CAN Scan: scan complete, %lu node(s) found",
                     (unsigned long)found_count);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Internal Helpers                                                           */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  Convert a hex string (e.g. "0x1A", "1A", "01") to a byte
  * @param  hex  Null-terminated hex string
  * @param  out  Output byte
  * @retval 0 on success, -1 on error
  */
static int _can_hex_to_byte(const char *hex, uint8_t *out)
{
    if (hex == NULL || out == NULL)
    {
        return -1;
    }

    long val = strtol(hex, NULL, 16);
    if (val < 0 || val > 255)
    {
        return -1;
    }

    *out = (uint8_t)(val & 0xFFU);
    return 0;
}

/**
  * @brief  Print a hex dump to the Log console
  * @param  prefix  Label prefix (e.g. "CAN TX")
  * @param  data    Data buffer
  * @param  len     Length of data
  */
static void _can_print_hex(const char *prefix, const uint8_t *data, uint16_t len)
{
    char hex_str[128U];
    int  pos = 0;

    for (uint16_t i = 0U; i < len; i++)
    {
        pos += snprintf(&hex_str[pos],
                        (size_t)(sizeof(hex_str) - (size_t)pos - 1U),
                        "%02X ", (unsigned)data[i]);
        if (pos >= (int)(sizeof(hex_str) - 4))
        {
            break;
        }
    }

    if (pos > 0)
    {
        hex_str[pos - 1] = '\0';
    }

    LOG_INFO("%s (%u bytes): %s", prefix, (unsigned)len, hex_str);
}
