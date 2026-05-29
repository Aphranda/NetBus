/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rfsw_task.c
  * @brief   RF Switch control task -- RS485 Modbus RTU master for SP10T switches
  *
  *          Architecture:
  *            - init:   Registers "rfsw" CLI. RS485/Modbus already initialized
  *                      by the Modbus module (registered before RFSW).
  *            - process:Polls RS485 RX buffer, prints unexpected RX to Log
  *
  *          RF Switch Register Map (from Doc/RF_Switch_Commands.md):
  *            Holding Registers (FC 0x03/0x06/0x10):
  *              0x0000 -- Channel (1-10)
  *              0x0001 -- Work mode (0=IO, 1=CMD)
  *              0x0002 -- Device Modbus address
  *              0x0003-0x0004 -- Serial number (32-bit)
  *              0x0005-0x0008 -- Device name (4×uint16, 8 ASCII chars)
  *              0x0009 -- Device status bits
  *              0x000A -- Output control bits
  *              0x000B -- Firmware version
  *              0x001B -- Hardware version
  *            Coils (FC 0x01/0x05/0x0F):
  *              0x0000-0x0005 -- Output coils S0_CA..S2_CB
  *            Discrete Inputs (FC 0x02):
  *              0x0000-0x0003 -- Input pins CTRL1..CTRL4
  *
  *          Debug CLI Usage:
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
#include "rfsw_task.h"
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

/**
  * @brief  Maximum device name string length (8 chars + null)
  */
#define RFSW_NAME_MAX_LEN      9U

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/**
  * @brief  Buffer for received RS485 data (printed to Log console)
  */
static uint8_t g_rs485_rx_buf[RS485_PRINT_MAX];

/* Private function prototypes -----------------------------------------------*/

static App_Status_t _rfsw_task_init(void);
static App_Status_t _rfsw_task_process(void);
static void         _rfsw_task_on_error(App_Status_t err);

/* Debug CLI command handlers */
static void _dbg_cmd_rfsw(int argc, char **argv);

/* High-level RF switch operations */
static Modbus_Result_t _rfsw_read_channel(uint8_t addr, uint8_t *channel);
static Modbus_Result_t _rfsw_write_channel(uint8_t addr, uint8_t channel);
static Modbus_Result_t _rfsw_read_mode(uint8_t addr, uint8_t *mode);
static Modbus_Result_t _rfsw_write_mode(uint8_t addr, uint8_t mode);
static Modbus_Result_t _rfsw_read_info(uint8_t addr,
                                        uint16_t *dev_id, uint32_t *serial,
                                        char *name, uint16_t *status,
                                        uint16_t *out_ctrl, uint16_t *fw_ver,
                                        uint16_t *hw_ver);
static Modbus_Result_t _rfsw_read_outputs(uint8_t addr, uint8_t *coils);
static Modbus_Result_t _rfsw_write_output(uint8_t addr, uint8_t coil_id, uint8_t state);
static Modbus_Result_t _rfsw_read_inputs(uint8_t addr, uint8_t *inputs);
static Modbus_Result_t _rfsw_read_status(uint8_t addr, uint16_t *status);
static Modbus_Result_t _rfsw_write_device_id(uint8_t addr, uint8_t new_id);

/* Internal helpers */
static void     _print_hex(const char *prefix, const uint8_t *data, uint16_t len);
static void     _print_modbus_result(Modbus_Result_t res);
static void     _extract_name_chars(const uint16_t *regs, char *name, uint8_t max_len);

/* Exported variables --------------------------------------------------------*/

/**
  * @brief  RF Switch task module descriptor -- register via App_RegisterModule()
  * @note   Register after Log task so LOG_* macros are usable during init.
  */
const App_Module_t g_rfsw_task_module = {
    .name     = "RFSW",
    .init     = _rfsw_task_init,
    .process  = _rfsw_task_process,
    .on_error = _rfsw_task_on_error,
};

/* Exported functions --------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initialize the RF Switch control subsystem
  * @note   Called by App_Task_Init() during module scan.
  *         RS485 and Modbus are already initialized by the Modbus module
  *         (registered at index 2, before RFSW at index 4).
  *         Only registers the "rfsw" debug CLI command here.
  * @retval APP_OK on success, APP_ERROR on failure
  */
static App_Status_t _rfsw_task_init(void)
{
    /* RS485 and Modbus are already initialized by the Modbus module
     * (registered before RFSW). Only register debug CLI commands here. */

    /* Register "rfsw" debug CLI command */
    if (Log_RegisterDbgCmdEx("rfsw", _dbg_cmd_rfsw, "RF switch control (Modbus)") != HAL_OK)
    {
        LOG_ERROR("RFSW: failed to register 'rfsw' debug command");
        return APP_ERROR;
    }

    LOG_INFO("RFSW: initialized (UART8 DMA+IDLE, DE=PE3)");
    LOG_INFO("RFSW: type 'rfsw' for SP10T switch control commands");

    return APP_OK;
}

/**
  * @brief  RF Switch task periodic process
  * @note   Called from App_Task_Loop() each cycle.
  *         Prints unexpected RS485 traffic (slaves should not talk unprompted).
  *         Does NOT consume RX data while a Modbus transaction is pending.
  * @retval APP_OK
  */
static App_Status_t _rfsw_task_process(void)
{
    uint16_t avail = RS485_Available();

    /* If a Modbus transaction is in progress, do NOT consume RX data */
    if (Modbus_IsTransactionPending())
    {
        return APP_OK;
    }

    if (avail > 0U)
    {
        uint16_t len = (avail > RS485_PRINT_MAX) ? RS485_PRINT_MAX : avail;
        len = RS485_Receive(g_rs485_rx_buf, len);

        if (len > 0U)
        {
            _print_hex("RFSW RX", g_rs485_rx_buf, len);
        }
    }

    return APP_OK;
}

/**
  * @brief  RF Switch task error handler
  * @param  err  Error code from init or process
  */
static void _rfsw_task_on_error(App_Status_t err)
{
    LOG_ERROR("RFSW: error occurred (status=%d)", (int)err);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  High-level RF Switch operations                                            */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  Read current switch channel from device
  * @param  addr     Slave device address (1-247)
  * @param  channel  Output: channel number (1-10)
  * @retval Modbus_Result_t
  */
static Modbus_Result_t _rfsw_read_channel(uint8_t addr, uint8_t *channel)
{
    uint16_t reg_val = 0U;
    Modbus_Result_t res = Modbus_ReadHoldingRegisters(addr, RFSW_REG_CHANNEL, 1U, &reg_val);

    if (res.status == MODBUS_OK)
    {
        *channel = (uint8_t)(reg_val & 0xFFU);
    }
    return res;
}

/**
  * @brief  Set switch channel on device
  * @param  addr     Slave device address (1-247)
  * @param  channel  Channel number (1-10)
  * @retval Modbus_Result_t
  */
static Modbus_Result_t _rfsw_write_channel(uint8_t addr, uint8_t channel)
{
    return Modbus_WriteSingleRegister(addr, RFSW_REG_CHANNEL, (uint16_t)channel);
}

/**
  * @brief  Read work mode from device
  * @param  addr  Slave device address (1-247)
  * @param  mode  Output: 0=IO DIRECT, 1=COMMAND
  * @retval Modbus_Result_t
  */
static Modbus_Result_t _rfsw_read_mode(uint8_t addr, uint8_t *mode)
{
    uint16_t reg_val = 0U;
    Modbus_Result_t res = Modbus_ReadHoldingRegisters(addr, RFSW_REG_WORK_MODE, 1U, &reg_val);

    if (res.status == MODBUS_OK)
    {
        *mode = (uint8_t)(reg_val & 0xFFU);
    }
    return res;
}

/**
  * @brief  Set work mode on device
  * @param  addr  Slave device address (1-247)
  * @param  mode  0=IO DIRECT, 1=COMMAND
  * @retval Modbus_Result_t
  */
static Modbus_Result_t _rfsw_write_mode(uint8_t addr, uint8_t mode)
{
    return Modbus_WriteSingleRegister(addr, RFSW_REG_WORK_MODE, (uint16_t)mode);
}

/**
  * @brief  Read comprehensive device info block
  * @note   Reads registers 0x0002-0x000B in one block (10 regs), then 0x001B.
  * @param  addr     Slave device address (1-247)
  * @param  dev_id   Output: Modbus device address
  * @param  serial   Output: 32-bit serial number
  * @param  name     Output: null-terminated name string (min 9 bytes)
  * @param  status   Output: device status bits
  * @param  out_ctrl Output: output control bits
  * @param  fw_ver   Output: firmware version (major<<8 | minor)
  * @param  hw_ver   Output: hardware version
  * @retval Modbus_Result_t
  */
static Modbus_Result_t _rfsw_read_info(uint8_t addr,
                                        uint16_t *dev_id, uint32_t *serial,
                                        char *name, uint16_t *status,
                                        uint16_t *out_ctrl, uint16_t *fw_ver,
                                        uint16_t *hw_ver)
{
    uint16_t regs[10U];
    Modbus_Result_t res;

    /* Read registers 0x0002 through 0x000B (10 registers) */
    res = Modbus_ReadHoldingRegisters(addr, RFSW_REG_DEVICE_ID, 10U, regs);
    if (res.status != MODBUS_OK)
    {
        return res;
    }

    /* Parse the block */
    /* regs[0]  = REG 0x0002: device ID */
    /* regs[1]  = REG 0x0003: serial high */
    /* regs[2]  = REG 0x0004: serial low */
    /* regs[3]  = REG 0x0005: name[0..1] */
    /* regs[4]  = REG 0x0006: name[2..3] */
    /* regs[5]  = REG 0x0007: name[4..5] */
    /* regs[6]  = REG 0x0008: name[6..7] */
    /* regs[7]  = REG 0x0009: device status */
    /* regs[8]  = REG 0x000A: output control */
    /* regs[9]  = REG 0x000B: firmware version */

    if (dev_id != NULL)  { *dev_id  = regs[0]; }
    if (serial != NULL)  { *serial  = ((uint32_t)regs[1] << 16) | (uint32_t)regs[2]; }
    if (name != NULL)    { _extract_name_chars(&regs[3], name, RFSW_NAME_MAX_LEN); }
    if (status != NULL)  { *status  = regs[7]; }
    if (out_ctrl != NULL){ *out_ctrl = regs[8]; }
    if (fw_ver != NULL)  { *fw_ver  = regs[9]; }

    /* Read hardware version (non-contiguous register 0x001B) */
    if (hw_ver != NULL)
    {
        uint16_t hw = 0U;
        res = Modbus_ReadHoldingRegisters(addr, RFSW_REG_HW_VERSION, 1U, &hw);
        if (res.status == MODBUS_OK)
        {
            *hw_ver = hw;
        }
        else
        {
            *hw_ver = 0U;
            /* Return the error from this read only if the first block succeeded */
            return res;
        }
    }

    return res;
}

/**
  * @brief  Read all 6 output coil states from device
  * @param  addr   Slave device address (1-247)
  * @param  coils  Output: coil states byte (bit 0 = coil 0, ..., bit 5 = coil 5)
  * @retval Modbus_Result_t
  */
static Modbus_Result_t _rfsw_read_outputs(uint8_t addr, uint8_t *coils)
{
    return Modbus_ReadCoils(addr, 0U, RFSW_COIL_COUNT, coils);
}

/**
  * @brief  Write a single output coil on device
  * @param  addr     Slave device address (1-247)
  * @param  coil_id  Coil index (0=S0_CA .. 5=S2_CB)
  * @param  state    0=OFF, non-zero=ON
  * @retval Modbus_Result_t
  */
static Modbus_Result_t _rfsw_write_output(uint8_t addr, uint8_t coil_id, uint8_t state)
{
    return Modbus_WriteSingleCoil(addr, (uint16_t)coil_id, state);
}

/**
  * @brief  Read 4 discrete inputs from device
  * @param  addr    Slave device address (1-247)
  * @param  inputs  Output: input states byte (bit 0 = CTRL1, ..., bit 3 = CTRL4)
  * @retval Modbus_Result_t
  */
static Modbus_Result_t _rfsw_read_inputs(uint8_t addr, uint8_t *inputs)
{
    return Modbus_ReadDiscreteInputs(addr, 0U, RFSW_INPUT_COUNT, inputs);
}

/**
  * @brief  Read device status register
  * @param  addr    Slave device address (1-247)
  * @param  status  Output: status bits
  * @retval Modbus_Result_t
  */
static Modbus_Result_t _rfsw_read_status(uint8_t addr, uint16_t *status)
{
    return Modbus_ReadHoldingRegisters(addr, RFSW_REG_DEVICE_STATUS, 1U, status);
}

/**
  * @brief  Change device Modbus address
  * @note   Writes REG_DEVICE_ID (0x0002). Device will respond to new address
  *         immediately. Use SYSTem:CONFigure:SAVE (via SCPI) or write to
  *         Flash via Modbus to persist.
  * @param  addr    Current slave device address (1-247)
  * @param  new_id  New Modbus address (1-247)
  * @retval Modbus_Result_t
  */
static Modbus_Result_t _rfsw_write_device_id(uint8_t addr, uint8_t new_id)
{
    return Modbus_WriteSingleRegister(addr, RFSW_REG_DEVICE_ID, (uint16_t)new_id);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Debug CLI -- "rfsw" command                                                  */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  'rfsw' debug CLI command -- control SP10T RF switches via Modbus RTU
  *
  *         Usage:
  *           rfsw get <addr>              -- Read current channel
  *           rfsw set <addr> <ch>         -- Set channel (1-10)
  *           rfsw mode <addr> [io|cmd]    -- Get/set work mode
  *           rfsw info <addr>             -- Read device identity & status
  *           rfsw output <addr> <id> <0|1>-- Set single output coil
  *           rfsw outputs <addr>          -- Read all 6 output coils
  *           rfsw inputs <addr>           -- Read 4 discrete inputs
  *           rfsw status <addr>           -- Read device status register
  *           rfsw id <addr> <new_id>      -- Change device Modbus address
  *
  *         Examples:
  *           rfsw get 1                   -- Read channel from device at addr 1
  *           rfsw set 1 5                 -- Set device 1 to channel 5
  *           rfsw mode 1 cmd              -- Set device 1 to COMMAND mode
  *           rfsw info 1                  -- Dump all device info
  *           rfsw outputs 1               -- Read all output coil states
  *           rfsw output 1 0 1            -- Turn ON output 0 (S0_CA) on device 1
  */
static void _dbg_cmd_rfsw(int argc, char **argv)
{
    if (argc < 2)
    {
        LOG_INFO("RF Switch Control Commands (SP10T via Modbus RTU):");
        LOG_INFO("  rfsw get <addr>              -- Read current channel");
        LOG_INFO("  rfsw set <addr> <ch>         -- Set channel (1-10)");
        LOG_INFO("  rfsw mode <addr> [io|cmd]    -- Get/set work mode");
        LOG_INFO("  rfsw info <addr>             -- Read device identity & status");
        LOG_INFO("  rfsw output <addr> <id> <0|1>-- Set single output coil");
        LOG_INFO("  rfsw outputs <addr>          -- Read all 6 output coils");
        LOG_INFO("  rfsw inputs <addr>           -- Read 4 discrete inputs");
        LOG_INFO("  rfsw status <addr>           -- Read device status register");
        LOG_INFO("  rfsw id <addr> <new_id>      -- Change device Modbus address");
        LOG_INFO("Examples:");
        LOG_INFO("  rfsw get 1         -- read channel from device 1");
        LOG_INFO("  rfsw set 1 5       -- set device 1 to channel 5");
        LOG_INFO("  rfsw mode 1 cmd    -- set device 1 to COMMAND mode");
        LOG_INFO("  rfsw info 1        -- dump all device info");
        return;
    }

    const char *sub = argv[1];

    /* ── rfsw get <addr> ──────────────────────────────────────────────────── */
    if (strcmp(sub, "get") == 0)
    {
        if (argc < 3)
        {
            LOG_INFO("Error: missing address. Usage: rfsw get <addr>");
            return;
        }

        long addr = strtol(argv[2], NULL, 0);
        if (addr < 1 || addr > 247)
        {
            LOG_INFO("Error: invalid address '%s' (1-247)", argv[2]);
            return;
        }

        uint8_t channel = 0U;
        Modbus_Result_t res = _rfsw_read_channel((uint8_t)addr, &channel);

        if (res.status == MODBUS_OK)
        {
            LOG_INFO("RFSW[%ld]: channel = %u", addr, (unsigned)channel);
        }
        else
        {
            _print_modbus_result(res);
        }
        return;
    }

    /* ── rfsw set <addr> <channel> ────────────────────────────────────────── */
    if (strcmp(sub, "set") == 0)
    {
        if (argc < 4)
        {
            LOG_INFO("Error: missing arguments. Usage: rfsw set <addr> <channel>");
            return;
        }

        long addr = strtol(argv[2], NULL, 0);
        if (addr < 1 || addr > 247)
        {
            LOG_INFO("Error: invalid address '%s' (1-247)", argv[2]);
            return;
        }

        long channel = strtol(argv[3], NULL, 0);
        if (channel < (long)RFSW_CHANNEL_MIN || channel > (long)RFSW_CHANNEL_MAX)
        {
            LOG_INFO("Error: channel must be %u-%u, got '%s'",
                     (unsigned)RFSW_CHANNEL_MIN, (unsigned)RFSW_CHANNEL_MAX, argv[3]);
            return;
        }

        Modbus_Result_t res = _rfsw_write_channel((uint8_t)addr, (uint8_t)channel);

        if (res.status == MODBUS_OK)
        {
            LOG_INFO("RFSW[%ld]: channel set to %ld", addr, channel);
        }
        else
        {
            _print_modbus_result(res);
        }
        return;
    }

    /* ── rfsw mode <addr> [io|cmd] ────────────────────────────────────────── */
    if (strcmp(sub, "mode") == 0)
    {
        if (argc < 3)
        {
            LOG_INFO("Error: missing address. Usage: rfsw mode <addr> [io|cmd]");
            return;
        }

        long addr = strtol(argv[2], NULL, 0);
        if (addr < 1 || addr > 247)
        {
            LOG_INFO("Error: invalid address '%s' (1-247)", argv[2]);
            return;
        }

        /* Get mode if no value argument */
        if (argc < 4)
        {
            uint8_t mode = 0U;
            Modbus_Result_t res = _rfsw_read_mode((uint8_t)addr, &mode);

            if (res.status == MODBUS_OK)
            {
                LOG_INFO("RFSW[%ld]: mode = %s (%u)",
                         addr,
                         (mode == RFSW_MODE_IO) ? "IO DIRECT" :
                         (mode == RFSW_MODE_COMMAND) ? "COMMAND" : "UNKNOWN",
                         (unsigned)mode);
            }
            else
            {
                _print_modbus_result(res);
            }
            return;
        }

        /* Set mode */
        uint8_t mode;
        if (strcmp(argv[3], "cmd") == 0 || strcmp(argv[3], "command") == 0)
        {
            mode = RFSW_MODE_COMMAND;
        }
        else if (strcmp(argv[3], "io") == 0 || strcmp(argv[3], "direct") == 0)
        {
            mode = RFSW_MODE_IO;
        }
        else
        {
            LOG_INFO("Error: invalid mode '%s'. Use 'io' or 'cmd'.", argv[3]);
            return;
        }

        Modbus_Result_t res = _rfsw_write_mode((uint8_t)addr, mode);

        if (res.status == MODBUS_OK)
        {
            LOG_INFO("RFSW[%ld]: mode set to %s", addr,
                     (mode == RFSW_MODE_IO) ? "IO DIRECT" : "COMMAND");
        }
        else
        {
            _print_modbus_result(res);
        }
        return;
    }

    /* ── rfsw info <addr> ─────────────────────────────────────────────────── */
    if (strcmp(sub, "info") == 0)
    {
        if (argc < 3)
        {
            LOG_INFO("Error: missing address. Usage: rfsw info <addr>");
            return;
        }

        long addr = strtol(argv[2], NULL, 0);
        if (addr < 1 || addr > 247)
        {
            LOG_INFO("Error: invalid address '%s' (1-247)", argv[2]);
            return;
        }

        uint16_t dev_id = 0U;
        uint32_t serial = 0UL;
        char     name[RFSW_NAME_MAX_LEN];
        uint16_t status = 0U;
        uint16_t out_ctrl = 0U;
        uint16_t fw_ver = 0U;
        uint16_t hw_ver = 0U;

        Modbus_Result_t res = _rfsw_read_info((uint8_t)addr,
                                               &dev_id, &serial, name,
                                               &status, &out_ctrl, &fw_ver, &hw_ver);

        if (res.status != MODBUS_OK)
        {
            _print_modbus_result(res);
            return;
        }

        LOG_INFO("── RFSW[%ld] Device Info ──────────────────────", addr);
        LOG_INFO("  Name:        '%s'", name);
        LOG_INFO("  Serial:      %lu", (unsigned long)serial);
        LOG_INFO("  Modbus ID:   %u", (unsigned)dev_id);
        LOG_INFO("  FW Version:  v%u.%u", (unsigned)((fw_ver >> 8) & 0xFFU),
                 (unsigned)(fw_ver & 0xFFU));
        LOG_INFO("  HW Version:  %u (0x%04X)", (unsigned)hw_ver, (unsigned)hw_ver);
        LOG_INFO("  Status:      0x%04X (init_done=%u, comm=%u)",
                 (unsigned)status,
                 (unsigned)((status & RFSW_STATUS_INIT_DONE) ? 1U : 0U),
                 (unsigned)((status & RFSW_STATUS_COMM_ACTIVE) ? 1U : 0U));
        LOG_INFO("  Output Ctrl: 0x%04X", (unsigned)out_ctrl);
        LOG_INFO("──────────────────────────────────────────────");

        /* Also read and display current channel and mode */
        {
            uint8_t channel = 0U;
            uint8_t mode_val = 0U;

            res = _rfsw_read_channel((uint8_t)addr, &channel);
            if (res.status == MODBUS_OK)
            {
                LOG_INFO("  Channel:     %u", (unsigned)channel);
            }

            res = _rfsw_read_mode((uint8_t)addr, &mode_val);
            if (res.status == MODBUS_OK)
            {
                LOG_INFO("  Mode:        %s (%u)",
                         (mode_val == RFSW_MODE_IO) ? "IO DIRECT" :
                         (mode_val == RFSW_MODE_COMMAND) ? "COMMAND" : "UNKNOWN",
                         (unsigned)mode_val);
            }
        }
        return;
    }

    /* ── rfsw output <addr> <id> <0|1> ────────────────────────────────────── */
    if (strcmp(sub, "output") == 0)
    {
        if (argc < 5)
        {
            LOG_INFO("Error: missing arguments. Usage: rfsw output <addr> <id> <0|1>");
            LOG_INFO("  Output IDs: 0=S0_CA, 1=S0_CB, 2=S1_CA, 3=S1_CB, 4=S2_CA, 5=S2_CB");
            return;
        }

        long addr = strtol(argv[2], NULL, 0);
        if (addr < 1 || addr > 247)
        {
            LOG_INFO("Error: invalid address '%s' (1-247)", argv[2]);
            return;
        }

        long coil_id = strtol(argv[3], NULL, 0);
        if (coil_id < 0 || coil_id >= (long)RFSW_COIL_COUNT)
        {
            LOG_INFO("Error: output id must be 0-%u, got '%s'",
                     (unsigned)(RFSW_COIL_COUNT - 1U), argv[3]);
            return;
        }

        long state = strtol(argv[4], NULL, 0);
        if (state != 0 && state != 1)
        {
            LOG_INFO("Error: state must be 0 (OFF) or 1 (ON), got '%s'", argv[4]);
            return;
        }

        Modbus_Result_t res = _rfsw_write_output((uint8_t)addr, (uint8_t)coil_id, (uint8_t)state);

        if (res.status == MODBUS_OK)
        {
            const char *out_names[] = {"S0_CA", "S0_CB", "S1_CA", "S1_CB", "S2_CA", "S2_CB"};
            LOG_INFO("RFSW[%ld]: output %ld (%s) = %s",
                     addr, coil_id, out_names[coil_id],
                     (state != 0) ? "ON" : "OFF");
        }
        else
        {
            _print_modbus_result(res);
        }
        return;
    }

    /* ── rfsw outputs <addr> ──────────────────────────────────────────────── */
    if (strcmp(sub, "outputs") == 0)
    {
        if (argc < 3)
        {
            LOG_INFO("Error: missing address. Usage: rfsw outputs <addr>");
            return;
        }

        long addr = strtol(argv[2], NULL, 0);
        if (addr < 1 || addr > 247)
        {
            LOG_INFO("Error: invalid address '%s' (1-247)", argv[2]);
            return;
        }

        uint8_t coils = 0U;
        Modbus_Result_t res = _rfsw_read_outputs((uint8_t)addr, &coils);

        if (res.status == MODBUS_OK)
        {
            LOG_INFO("RFSW[%ld] output coils:", addr);
            LOG_INFO("  [0] S0_CA = %s", ((coils >> 0) & 1U) ? "ON" : "OFF");
            LOG_INFO("  [1] S0_CB = %s", ((coils >> 1) & 1U) ? "ON" : "OFF");
            LOG_INFO("  [2] S1_CA = %s", ((coils >> 2) & 1U) ? "ON" : "OFF");
            LOG_INFO("  [3] S1_CB = %s", ((coils >> 3) & 1U) ? "ON" : "OFF");
            LOG_INFO("  [4] S2_CA = %s", ((coils >> 4) & 1U) ? "ON" : "OFF");
            LOG_INFO("  [5] S2_CB = %s", ((coils >> 5) & 1U) ? "ON" : "OFF");
        }
        else
        {
            _print_modbus_result(res);
        }
        return;
    }

    /* ── rfsw inputs <addr> ───────────────────────────────────────────────── */
    if (strcmp(sub, "inputs") == 0)
    {
        if (argc < 3)
        {
            LOG_INFO("Error: missing address. Usage: rfsw inputs <addr>");
            return;
        }

        long addr = strtol(argv[2], NULL, 0);
        if (addr < 1 || addr > 247)
        {
            LOG_INFO("Error: invalid address '%s' (1-247)", argv[2]);
            return;
        }

        uint8_t inputs = 0U;
        Modbus_Result_t res = _rfsw_read_inputs((uint8_t)addr, &inputs);

        if (res.status == MODBUS_OK)
        {
            LOG_INFO("RFSW[%ld] discrete inputs:", addr);
            LOG_INFO("  CTRL1 = %s", ((inputs >> 0) & 1U) ? "HIGH" : "LOW");
            LOG_INFO("  CTRL2 = %s", ((inputs >> 1) & 1U) ? "HIGH" : "LOW");
            LOG_INFO("  CTRL3 = %s", ((inputs >> 2) & 1U) ? "HIGH" : "LOW");
            LOG_INFO("  CTRL4 = %s", ((inputs >> 3) & 1U) ? "HIGH" : "LOW");
        }
        else
        {
            _print_modbus_result(res);
        }
        return;
    }

    /* ── rfsw status <addr> ───────────────────────────────────────────────── */
    if (strcmp(sub, "status") == 0)
    {
        if (argc < 3)
        {
            LOG_INFO("Error: missing address. Usage: rfsw status <addr>");
            return;
        }

        long addr = strtol(argv[2], NULL, 0);
        if (addr < 1 || addr > 247)
        {
            LOG_INFO("Error: invalid address '%s' (1-247)", argv[2]);
            return;
        }

        uint16_t status = 0U;
        Modbus_Result_t res = _rfsw_read_status((uint8_t)addr, &status);

        if (res.status == MODBUS_OK)
        {
            LOG_INFO("RFSW[%ld]: status = 0x%04X", addr, (unsigned)status);
            LOG_INFO("  Init done:  %s", (status & RFSW_STATUS_INIT_DONE) ? "YES" : "NO");
            LOG_INFO("  Comm active: %s", (status & RFSW_STATUS_COMM_ACTIVE) ? "YES" : "NO");
        }
        else
        {
            _print_modbus_result(res);
        }
        return;
    }

    /* ── rfsw id <addr> <new_id> ──────────────────────────────────────────── */
    if (strcmp(sub, "id") == 0)
    {
        if (argc < 4)
        {
            LOG_INFO("Error: missing arguments. Usage: rfsw id <addr> <new_id>");
            LOG_INFO("  WARNING: Changes the device Modbus address immediately.");
            LOG_INFO("  Use SCPI 'SYSTem:CONFigure:SAVE' to persist the change.");
            return;
        }

        long addr = strtol(argv[2], NULL, 0);
        if (addr < 1 || addr > 247)
        {
            LOG_INFO("Error: invalid address '%s' (1-247)", argv[2]);
            return;
        }

        long new_id = strtol(argv[3], NULL, 0);
        if (new_id < 1 || new_id > 247)
        {
            LOG_INFO("Error: invalid new address '%s' (1-247)", argv[3]);
            return;
        }

        LOG_INFO("RFSW: changing device address %ld → %ld ...", addr, new_id);

        Modbus_Result_t res = _rfsw_write_device_id((uint8_t)addr, (uint8_t)new_id);

        if (res.status == MODBUS_OK)
        {
            LOG_INFO("RFSW: device address changed to %ld (will respond at new address now)", new_id);
            LOG_INFO("  NOTE: Address change is immediate but may NOT be persisted.");
            LOG_INFO("  Use 'SYSTem:CONFigure:SAVE' via SCPI to save to Flash.");
        }
        else
        {
            _print_modbus_result(res);
        }
        return;
    }

    /* ── Unknown sub-command ─────────────────────────────────────────────── */
    LOG_INFO("Error: unknown sub-command '%s'. Type 'rfsw' for usage.", sub);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Internal Helpers                                                           */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  Print a hex dump to the Log console
  * @param  prefix  Label prefix (e.g. "RFSW TX")
  * @param  data    Data buffer
  * @param  len     Length of data
  */
static void _print_hex(const char *prefix, const uint8_t *data, uint16_t len)
{
    uint16_t print_len = (len > 64U) ? 64U : len;

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

/**
  * @brief  Print a human-readable Modbus result on error
  * @param  res  Modbus_Result_t from any Modbus API call
  */
static void _print_modbus_result(Modbus_Result_t res)
{
    if (res.status == MODBUS_ERR_EXCEPTION)
    {
        LOG_ERROR("RFSW: Modbus EXCEPTION (code=0x%02X: %s)",
                  (unsigned)res.exc_code,
                  Modbus_ExceptionString(res.exc_code));
    }
    else if (res.status != MODBUS_OK)
    {
        LOG_ERROR("RFSW: Modbus FAILED -- %s", Modbus_StatusString(res.status));
    }
}

/**
  * @brief  Extract null-terminated ASCII name from 4 Modbus registers
  * @note   Each uint16_t register holds 2 ASCII chars (big-endian).
  *          e.g. reg[0]=0x5377 → 'S','w'; reg[1]=0x6974 → 'i','t'
  * @param  regs    Pointer to 4 uint16_t name registers
  * @param  name    Output buffer (at least max_len bytes)
  * @param  max_len Maximum chars including null terminator
  */
static void _extract_name_chars(const uint16_t *regs, char *name, uint8_t max_len)
{
    if (name == NULL || regs == NULL)
    {
        return;
    }

    uint8_t pos = 0U;
    for (uint8_t r = 0U; r < 4U && pos < (max_len - 1U); r++)
    {
        char hi = (char)((regs[r] >> 8) & 0xFFU);
        char lo = (char)(regs[r] & 0xFFU);

        if (hi != '\0' && pos < (max_len - 1U))
        {
            name[pos++] = hi;
        }
        else if (hi == '\0')
        {
            break;
        }

        if (lo != '\0' && pos < (max_len - 1U))
        {
            name[pos++] = lo;
        }
        else if (lo == '\0')
        {
            break;
        }
    }
    name[pos] = '\0';
}
