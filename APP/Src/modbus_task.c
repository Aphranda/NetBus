/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    modbus_task.c
  * @brief   Modbus task — owns RS485 + Modbus RTU Master + "modbus" CLI
  *
  *          Runs in StartModbusTask FreeRTOS thread.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "modbus_task.h"
#include "rs485.h"
#include "modbus.h"
#include "log.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MODBUS_RX_PRINT_MAX    64U

static uint8_t g_rx_buf[MODBUS_RX_PRINT_MAX];
static uint8_t g_raw_buf[MODBUS_MAX_FRAME_LEN];

static App_Status_t _modbus_task_init(void);
static App_Status_t _modbus_task_process(void);
static void         _modbus_task_on_error(App_Status_t err);
static void _dbg_cmd_modbus(int argc, char **argv);
static int  _hex_to_byte(const char *hex, uint8_t *out);

const App_Module_t g_modbus_task_module = {
    .name     = "Modbus",
    .init     = _modbus_task_init,
    .process  = NULL,  /* runs in StartModbusTask via Modbus_Task_Loop() */
    .on_error = _modbus_task_on_error,
};

void Modbus_Task_Loop(void)
{
    for (;;)
    {
        _modbus_task_process();
        osDelay(10);
    }
}

/* ── Private ──────────────────────────────────────────────────────────── */

static App_Status_t _modbus_task_init(void)
{
    if (RS485_Init() != HAL_OK)
    {
        LOG_ERROR("Modbus: RS485_Init() failed");
        return APP_ERROR;
    }
    Modbus_Init();
    LOG_INFO("Modbus: RS485 + Modbus RTU master ready (timeout=%lu ms)",
             (unsigned long)Modbus_GetTimeout());

    if (Log_RegisterDbgCmdEx("modbus", _dbg_cmd_modbus,
                             "Raw Modbus RTU frame injection") != HAL_OK)
    {
        LOG_ERROR("Modbus: failed to register 'modbus' debug command");
        return APP_ERROR;
    }
    LOG_INFO("Modbus: type 'modbus <hex...>' to send raw Modbus frame");
    return APP_OK;
}

static App_Status_t _modbus_task_process(void)
{
    uint16_t avail = RS485_Available();
    if (Modbus_IsTransactionPending() || avail == 0U)
    {
        return APP_OK;
    }
    uint16_t len = (avail > MODBUS_RX_PRINT_MAX) ? MODBUS_RX_PRINT_MAX : avail;
    len = RS485_Receive(g_rx_buf, len);
    if (len > 0U)
    {
        char hex[256];
        int pos = 0;
        for (uint16_t i = 0U; i < len && pos < (int)(sizeof(hex) - 4); i++)
        {
            pos += snprintf(&hex[pos], sizeof(hex) - (size_t)pos - 1U,
                           "%02X ", (unsigned)g_rx_buf[i]);
        }
        LOG_INFO("MODBUS RX[%u]: %s", (unsigned)len, hex);
    }
    return APP_OK;
}

static void _modbus_task_on_error(App_Status_t err)
{
    LOG_ERROR("Modbus: error (status=%d)", (int)err);
}

/* ── "modbus" CLI ─────────────────────────────────────────────────────── */

static void _dbg_cmd_modbus(int argc, char **argv)
{
    if (argc < 2)
    {
        LOG_INFO("Usage: modbus <hex bytes...>  — send raw Modbus frame (CRC auto)");
        LOG_INFO("  modbus 01 03 00 00 00 01    — read channel from device 1");
        return;
    }

    uint16_t frame_len = 0U;
    for (int i = 1; i < argc && frame_len < (MODBUS_MAX_FRAME_LEN - 2U); i++)
    {
        uint8_t byte;
        if (_hex_to_byte(argv[i], &byte) != 0)
        {
            LOG_INFO("Error: invalid hex '%s'", argv[i]);
            return;
        }
        g_raw_buf[frame_len++] = byte;
    }
    if (frame_len < 2U)
    {
        LOG_INFO("Error: need at least address + function code");
        return;
    }

    uint16_t crc = Modbus_CRC16(g_raw_buf, frame_len);
    g_raw_buf[frame_len++] = (uint8_t)(crc & 0xFFU);
    g_raw_buf[frame_len++] = (uint8_t)((crc >> 8) & 0xFFU);

    uint8_t  resp[MODBUS_MAX_FRAME_LEN];
    uint16_t resp_len = sizeof(resp);

    Modbus_Result_t res = Modbus_SendRaw(g_raw_buf, frame_len, resp, &resp_len);
    if (res.status == MODBUS_OK)
    {
        char hex[256];
        int pos = 0;
        for (uint16_t i = 0U; i < resp_len && pos < (int)(sizeof(hex) - 4); i++)
        {
            pos += snprintf(&hex[pos], sizeof(hex) - (size_t)pos - 1U,
                           "%02X ", (unsigned)resp[i]);
        }
        LOG_INFO("Modbus RX[%u]: %s", (unsigned)resp_len, hex);
    }
    else
    {
        LOG_ERROR("Modbus: %s", Modbus_StatusString(res.status));
    }
}

static int _hex_to_byte(const char *hex, uint8_t *out)
{
    if (hex == NULL || out == NULL) return -1;
    long val = strtol(hex, NULL, 16);
    if (val < 0 || val > 255) return -1;
    *out = (uint8_t)(val & 0xFFU);
    return 0;
}
