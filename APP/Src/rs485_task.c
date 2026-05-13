/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rs485_task.c
  * @brief   RS485 task implementation — wraps ThirdParty/RS485 as App_Module_t
  *
  *          Architecture:
  *            - init:   Calls RS485_Init() to start UART8 DMA RX (CIRCULAR+IDLE)
  *            - process:Polls RS485 RX buffer, prints received frames to Log
  *            - debug:  Registers "modbus" CLI command for sending Modbus frames
  *
  *          Debug CLI Usage:
  *            modbus read <slave> <reg> <qty>  — Send Modbus FC=03 read query
  *            modbus send <hex...>             — Send raw hex bytes
  *
  *          Example:
  *            modbus read 1 2 1
  *              → Sends: 01 03 00 02 00 01 <CRC16>
  *              → Equivalent to: 01 03 00 02 00 01 24 0A
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
#include "rs485_task.h"
#include "rs485.h"
#include "modbus.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/**
  * @brief  Maximum received data to print per process cycle (bytes)
  */
#define RS485_PRINT_MAX        64U

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/**
  * @brief  Buffer for received RS485 data (printed to Log console)
  */
static uint8_t g_rs485_rx_buf[RS485_PRINT_MAX];

/**
  * @brief  Buffer for Modbus raw send/receive (debug CLI)
  */
static uint8_t g_dbg_modbus_buf[MODBUS_MAX_FRAME_LEN];

/* Private function prototypes -----------------------------------------------*/

static App_Status_t _rs485_task_init(void);
static App_Status_t _rs485_task_process(void);
static void         _rs485_task_on_error(App_Status_t err);

/* Debug CLI command handlers */
static void _dbg_cmd_modbus(int argc, char **argv);

/* Internal helpers */
static int      _hex_to_byte(const char *hex, uint8_t *out);
static void     _print_hex(const char *prefix, const uint8_t *data, uint16_t len);

/* Exported variables --------------------------------------------------------*/

/**
  * @brief  RS485 task module descriptor — register via App_RegisterModule()
  * @note   Register after Log task so LOG_* macros are usable during init.
  */
const App_Module_t g_rs485_task_module = {
    .name     = "RS485",
    .init     = _rs485_task_init,
    .process  = _rs485_task_process,
    .on_error = _rs485_task_on_error,
};

/* Exported functions --------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initialize the RS485 subsystem
  * @note   Called by App_Task_Init() during module scan.
  *         Registers the "modbus" debug CLI command for user interaction.
  *         RS485 hardware (UART8) is initialized by MX_UART8_Init() in main.c.
  * @retval APP_OK on success, APP_ERROR on failure
  */
static App_Status_t _rs485_task_init(void)
{
    /* Initialize RS485 driver (starts DMA CIRCULAR RX + IDLE detection) */
    if (RS485_Init() != HAL_OK)
    {
        LOG_ERROR("RS485_Task: RS485_Init() failed");
        return APP_ERROR;
    }

    /* Initialize Modbus RTU Master (ThirdParty/Modbus) */
    Modbus_Init();
    LOG_INFO("RS485_Task: Modbus master initialized (timeout=%lu ms)",
             (unsigned long)Modbus_GetTimeout());

    /* Register "modbus" debug CLI command */
    if (Log_RegisterDbgCmd("modbus", _dbg_cmd_modbus) != HAL_OK)
    {
        LOG_ERROR("RS485_Task: failed to register 'modbus' debug command");
        return APP_ERROR;
    }

    LOG_INFO("RS485_Task: initialized (UART8, DMA+IDLE, DE=PE3)");
    LOG_INFO("RS485_Task: type 'modbus read <slave> <reg> <qty>' or 'modbus send <hex...>'");

    return APP_OK;
}

/**
  * @brief  RS485 task periodic process
  * @note   Called from App_Task_Loop() each cycle.
  *         Checks for received RS485 data and prints it to Log console.
  * @retval APP_OK
  */
static App_Status_t _rs485_task_process(void)
{
    uint16_t avail = RS485_Available();

    /* If a Modbus transaction is in progress, do NOT consume RX data —
     * the Modbus master library handles response collection internally. */
    if (Modbus_IsTransactionPending())
    {
        return APP_OK;
    }

    if (avail > 0U)
    {
        /* Read available data (up to buffer size) */
        uint16_t len = (avail > RS485_PRINT_MAX) ? RS485_PRINT_MAX : avail;
        len = RS485_Receive(g_rs485_rx_buf, len);

        if (len > 0U)
        {
            _print_hex("RS485 RX", g_rs485_rx_buf, len);
        }
    }

    return APP_OK;
}

/**
  * @brief  RS485 task error handler
  * @param  err  Error code from init or process
  */
static void _rs485_task_on_error(App_Status_t err)
{
    LOG_ERROR("RS485_Task: error occurred (status=%d)", (int)err);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Debug CLI — "modbus" command                                                */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  'modbus' debug CLI command
  *
  *         Usage:
  *           modbus read <slave> <reg> <qty>
  *             - Send Modbus FC=03 (Read Holding Registers) query and wait
  *               for response. Displays decoded register values on success.
  *             - slave: slave address (1-247)
  *             - reg:   starting register address (0-65535)
  *             - qty:   number of registers to read (1-125)
  *             - Example: "modbus read 1 2 1"
  *
  *           modbus write <slave> <reg> <value>
  *             - Send Modbus FC=06 (Write Single Register)
  *             - Example: "modbus write 1 0 1234"
  *
  *           modbus send <hex bytes...>
  *             - Send raw hex bytes over RS485 (NO CRC appended, NO response wait)
  *             - Example: "modbus send 01 03 00 02 00 01 24 0A"
  *
  *           Note: "modbus read" uses the Modbus library (ThirdParty/Modbus)
  *                 which waits for response, validates CRC, and parses data.
  *                 "modbus send" is a raw passthrough — no response handling.
  */
static void _dbg_cmd_modbus(int argc, char **argv)
{
    if (argc < 2)
    {
        LOG_INFO("Usage:");
        LOG_INFO("  modbus read <slave> <reg> <qty>   -- Read Holding Registers (FC=03)");
        LOG_INFO("  modbus write <slave> <reg> <val>  -- Write Single Register (FC=06)");
        LOG_INFO("  modbus send <hex bytes...>        -- Send raw hex (no CRC/response)");
        LOG_INFO("Example:");
        LOG_INFO("  modbus read 1 2 1");
        LOG_INFO("  modbus write 1 0 1234");
        LOG_INFO("  modbus send 01 03 00 02 00 01 24 0A");
        return;
    }

    /* ── modbus read <slave> <reg> <qty> ──────────────────────────────────── */
    if (strcmp(argv[1], "read") == 0)
    {
        if (argc < 5)
        {
            LOG_INFO("Error: missing arguments. Usage: modbus read <slave> <reg> <qty>");
            return;
        }

        /* Parse slave address */
        long slave = strtol(argv[2], NULL, 0);
        if (slave < 1 || slave > 247)
        {
            LOG_INFO("Error: invalid slave address '%s' (1-247)", argv[2]);
            return;
        }

        /* Parse starting register */
        long reg = strtol(argv[3], NULL, 0);
        if (reg < 0 || reg > 0xFFFF)
        {
            LOG_INFO("Error: invalid register address '%s' (0-65535)", argv[3]);
            return;
        }

        /* Parse quantity */
        long qty = strtol(argv[4], NULL, 0);
        if (qty < 1 || qty > 125)
        {
            LOG_INFO("Error: invalid quantity '%s' (1-125)", argv[4]);
            return;
        }

        /* ── Execute Modbus Read Holding Registers via library ─────────── */
        LOG_INFO("Modbus: FC=03 read (slave=%ld, reg=%ld, qty=%ld)",
                 slave, reg, qty);

        uint16_t regs[MODBUS_MAX_REGISTERS];
        Modbus_Result_t res = Modbus_ReadHoldingRegisters(
            (uint8_t)slave, (uint16_t)reg, (uint16_t)qty, regs);

        if (res.status == MODBUS_OK)
        {
            LOG_INFO("Modbus: SUCCESS - %ld register(s):", qty);
            for (long i = 0; i < qty; i++)
            {
                LOG_INFO("  reg[%ld] = %u (0x%04X)", reg + i,
                         (unsigned)regs[i], (unsigned)regs[i]);
            }
        }
        else if (res.status == MODBUS_ERR_EXCEPTION)
        {
            LOG_ERROR("Modbus: EXCEPTION (code=0x%02X: %s)",
                      (unsigned)res.exc_code,
                      Modbus_ExceptionString(res.exc_code));
        }
        else
        {
            LOG_ERROR("Modbus: FAILED - %s", Modbus_StatusString(res.status));
        }

        return;
    }

    /* ── modbus write <slave> <reg> <value> ──────────────────────────────── */
    if (strcmp(argv[1], "write") == 0)
    {
        if (argc < 5)
        {
            LOG_INFO("Error: missing arguments. Usage: modbus write <slave> <reg> <val>");
            return;
        }

        long slave = strtol(argv[2], NULL, 0);
        if (slave < 1 || slave > 247)
        {
            LOG_INFO("Error: invalid slave address '%s' (1-247)", argv[2]);
            return;
        }

        long reg = strtol(argv[3], NULL, 0);
        if (reg < 0 || reg > 0xFFFF)
        {
            LOG_INFO("Error: invalid register address '%s' (0-65535)", argv[3]);
            return;
        }

        long val = strtol(argv[4], NULL, 0);
        if (val < 0 || val > 0xFFFF)
        {
            LOG_INFO("Error: invalid value '%s' (0-65535)", argv[4]);
            return;
        }

        /* ── Execute Modbus Write Single Register via library ──────────── */
        LOG_INFO("Modbus: FC=06 write (slave=%ld, reg=%ld, value=%ld)",
                 slave, reg, val);

        Modbus_Result_t res = Modbus_WriteSingleRegister(
            (uint8_t)slave, (uint16_t)reg, (uint16_t)val);

        if (res.status == MODBUS_OK)
        {
            LOG_INFO("Modbus: SUCCESS - wrote %ld to reg[%ld]", val, reg);
        }
        else if (res.status == MODBUS_ERR_EXCEPTION)
        {
            LOG_ERROR("Modbus: EXCEPTION (code=0x%02X: %s)",
                      (unsigned)res.exc_code,
                      Modbus_ExceptionString(res.exc_code));
        }
        else
        {
            LOG_ERROR("Modbus: FAILED - %s", Modbus_StatusString(res.status));
        }

        return;
    }

    /* ── modbus send <hex bytes...> ───────────────────────────────────────── */
    if (strcmp(argv[1], "send") == 0)
    {
        uint16_t frame_len = 0U;

        if (argc < 3)
        {
            LOG_INFO("Error: missing hex bytes. Usage: modbus send <hex...>");
            return;
        }

        /* Parse hex bytes from arguments */
        for (int i = 2; i < argc && frame_len < MODBUS_MAX_FRAME_LEN; i++)
        {
            uint8_t byte;
            if (_hex_to_byte(argv[i], &byte) != 0)
            {
                LOG_INFO("Error: invalid hex byte '%s' at position %d", argv[i], i - 1);
                return;
            }
            g_dbg_modbus_buf[frame_len++] = byte;
        }

        if (frame_len == 0U)
        {
            LOG_INFO("Error: no valid hex bytes to send");
            return;
        }

        LOG_INFO("Modbus: sending raw %u bytes", (unsigned)frame_len);
        _print_hex("Modbus TX", g_dbg_modbus_buf, frame_len);

        /* Send via RS485 (blocking) */
        if (RS485_Send(g_dbg_modbus_buf, frame_len) != HAL_OK)
        {
            LOG_ERROR("Modbus: RS485_Send() failed");
        }
        return;
    }

    /* ── Unknown sub-command ─────────────────────────────────────────────── */
    LOG_INFO("Error: unknown sub-command '%s'. Use 'read', 'write', or 'send'.", argv[1]);
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
static int _hex_to_byte(const char *hex, uint8_t *out)
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
  * @param  prefix  Label prefix (e.g. "Modbus TX")
  * @param  data    Data buffer
  * @param  len     Length of data
  */
static void _print_hex(const char *prefix, const uint8_t *data, uint16_t len)
{
    /* Limit print length to avoid flooding the console */
    uint16_t print_len = (len > 64U) ? 64U : len;

    /* Build hex string in a local buffer */
    char hex_str[256U];
    int pos = 0;

    for (uint16_t i = 0U; i < print_len; i++)
    {
        pos += snprintf(&hex_str[pos], (size_t)(sizeof(hex_str) - (size_t)pos - 1U),
                        "%02X ", (unsigned)data[i]);
        if (pos >= (int)(sizeof(hex_str) - 4))
        {
            break;
        }
    }

    /* Remove trailing space and print */
    if (pos > 0)
    {
        hex_str[pos - 1] = '\0';
    }

    LOG_INFO("%s (%u bytes): %s", prefix, (unsigned)len, hex_str);

    if (len > print_len)
    {
        LOG_INFO("  ... (remaining %u bytes not printed)", (unsigned)(len - print_len));
    }
}
