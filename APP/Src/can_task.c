/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can_task.c
  * @brief   CAN task -- owns FDCAN1 driver and "can" debug CLI
  *
  *          Architecture:
  *            - init:    CAN_Init(), registers "can" debug CLI command
  *            - process: No-op (CAN is event-driven via interrupts)
  *            - loop:    Can_Task_Loop() runs in StartCanTask thread
  *
  *          Migration from log_task.c:
  *            CAN init and the "can" debug CLI previously lived in
  *            log_task.c. They have been extracted here so CAN has
  *            its own FreeRTOS task, decoupled from Log.
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
#include "can_task.h"
#include "can.h"
#include "log.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Private define ------------------------------------------------------------*/

#define CANSN_TIMEOUT_MS      1000U
#define CANRESP_TIMEOUT_MS    500U
#define CANSN_MAX_SN_LEN      64U
#define CANRAW_MAX_BYTES      CAN_MAX_DATA_LEN

/* Private function prototypes -----------------------------------------------*/

static App_Status_t _can_task_init(void);
static App_Status_t _can_task_process(void);
static void         _can_task_on_error(App_Status_t err);

static void _dbg_cmd_can(int argc, char **argv);
static void _dbg_cmd_cansend(int argc, char **argv);
static void _dbg_cmd_canscan(int argc, char **argv);
static int  _can_hex_to_byte(const char *hex, uint8_t *out);
static void _can_print_hex(const char *prefix, const uint8_t *data, uint16_t len);

/* Exported variables --------------------------------------------------------*/

const App_Module_t g_can_task_module = {
    .name     = "CAN",
    .init     = _can_task_init,
    .process  = NULL,  /* CAN is event/interrupt-driven, no polling needed */
    .on_error = _can_task_on_error,
};

/* Exported functions --------------------------------------------------------*/

void Can_Task_Loop(void)
{
    for (;;)
    {
        osDelay(100);
    }
}

/* Private functions ---------------------------------------------------------*/

static App_Status_t _can_task_init(void)
{
    if (CAN_Init() != HAL_OK)
    {
        LOG_ERROR("CAN: CAN_Init() failed");
        return APP_ERROR;
    }
    LOG_INFO("CAN: FDCAN1 initialized (CAN FD with BRS)");

    if (Log_RegisterDbgCmdEx("can", _dbg_cmd_can, "CAN bus control (send/scan)") != HAL_OK)
    {
        LOG_ERROR("CAN: failed to register 'can' debug command");
        return APP_ERROR;
    }

    LOG_INFO("CAN: type 'can send <id> <hex...>' to send raw CAN frame");
    LOG_INFO("CAN: type 'can scan [start] [end]' to scan CAN bus");
    return APP_OK;
}

static App_Status_t _can_task_process(void)
{
    return APP_OK;
}

static void _can_task_on_error(App_Status_t err)
{
    LOG_ERROR("CAN: error (status=%d)", (int)err);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Debug CLI -- "can" command (CAN bus control dispatcher)                    */
/* ─────────────────────────────────────────────────────────────────────────── */

static void _dbg_cmd_can(int argc, char **argv)
{
    if (argc < 2)
    {
        LOG_INFO("=== CAN Commands ===");
        LOG_INFO("  can send <id> <hex...>    -- send raw CAN frame");
        LOG_INFO("  can scan [start] [end]    -- scan CAN bus");
        return;
    }

    if (strcmp(argv[1], "send") == 0)
    {
        _dbg_cmd_cansend(argc - 1, &argv[1]);
    }
    else if (strcmp(argv[1], "scan") == 0)
    {
        _dbg_cmd_canscan(argc - 1, &argv[1]);
    }
    else
    {
        LOG_INFO("Unknown sub-command: '%s'. Type 'can' for usage.", argv[1]);
    }
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Debug CLI -- "can send" command                                             */
/* ─────────────────────────────────────────────────────────────────────────── */

static void _dbg_cmd_cansend(int argc, char **argv)
{
    CAN_Msg_t tx_msg;
    uint8_t   data_buf[CANRAW_MAX_BYTES];
    uint8_t   data_len = 0U;

    if (argc < 3)
    {
        LOG_INFO("Usage: can send <id> <hex bytes...>");
        LOG_INFO("  Send a raw CAN frame with specified ID and data");
        LOG_INFO("  Examples:");
        LOG_INFO("    can send 0x101 01           -- SN query to node 1");
        LOG_INFO("    can send 0x101 01 02 03     -- 3-byte frame");
        LOG_INFO("    can send 0x105 01 01 0xE8   -- LED blink 1000ms");
        return;
    }

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
/*  Debug CLI -- "can scan" command                                             */
/* ─────────────────────────────────────────────────────────────────────────── */

static void _dbg_cmd_canscan(int argc, char **argv)
{
    long start_id = 1L;
    long end_id   = 40L;

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
            _can_print_hex("CAN Scan TX", tx_msg.data, tx_msg.dlc);

            if (CAN_Send(&tx_msg) == HAL_OK)
            {
                send_count++;
            }

            if ((send_count % 10U) == 0U)
            {
                HAL_Delay(1U);
            }
        }

        LOG_INFO("CAN Scan: sent %lu queries in %lu ms",
                 (unsigned long)send_count,
                 (unsigned long)(HAL_GetTick() - send_start));
    }

    {
        uint32_t scan_timeout = 500U;
        uint32_t tick_start   = HAL_GetTick();
        uint32_t found_count  = 0U;
        CAN_Msg_t rx_msg;

        LOG_INFO("CAN Scan: listening for responses (%u ms)...",
                 (unsigned)scan_timeout);

        while ((HAL_GetTick() - tick_start) < scan_timeout)
        {
            if (CAN_GetRxMessage(&rx_msg) == HAL_OK)
            {
                uint32_t resp_node = rx_msg.id;
                if (resp_node >= (uint32_t)start_id &&
                    resp_node <= (uint32_t)end_id &&
                    rx_msg.dlc >= 3U &&
                    rx_msg.data[0] == 0x01U)
                {
                    found_count++;

                    if (rx_msg.data[1] == 0x00U)
                    {
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
                    LOG_VERBOSE("CAN Scan: ignoring msg id=0x%03X", rx_msg.id);
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

static void _can_print_hex(const char *prefix, const uint8_t *data, uint16_t len)
{
    char hex_str[256U];
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
