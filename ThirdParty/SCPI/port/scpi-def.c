/*-
 * BSD 2-Clause License
 *
 * Copyright (c) 2012-2018, Jan Breuer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file   scpi-def.c
 * @date   Thu Nov 15 10:58:45 UTC 2012
 *
 * @brief  SCPI parser test
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scpi/scpi.h"
#include "scpi-def.h"
#include "io.h"
#include "debug.h"
#include "flash.h"

/* ========================================================================== */
/*              开关通道控制 (原 debug SET/GET)                                */
/* ========================================================================== */

static scpi_result_t SCPI_ConfigureSwitch(scpi_t *context)
{
    int32_t number[2] = {0,0};
    SCPI_CommandNumbers(context, number, 1, 1);
    /* number[0] = switch index from command header (1-based, e.g. SWITch1=1, SWITch2=2...) */

    /* ================================================================== */
    /*   SWITCH# 匹配检查：命令中的开关编号必须匹配本机设备ID              */
    /*   若 Flash 中 device_id == 0（未配置），则接受任意 SWITCH#          */
    /* ================================================================== */
    uint8_t local_id = Flash_GetDeviceID();
    if (local_id != 0 && number[0] != (int32_t)local_id)
    {
        /* SWITCH# 不匹配本机设备ID → 静默忽略（消费参数后返回 OK） */
        uint32_t discard;
        SCPI_ParamUInt32(context, &discard, FALSE);
        return SCPI_RES_OK;
    }

    /* 读取通道号参数 (1-based: 1~10) */
    uint32_t channel = 0;
    if (!SCPI_ParamUInt32(context, &channel, true))
    {
        return SCPI_RES_ERR;
    }
    if (channel < 1 || channel > SWITCH_CH_COUNT)
    {
        SCPI_Write(context, "ERROR: Invalid channel (1-10)\r\n", 31);
        return SCPI_RES_ERR;
    }

    /* 转换为0-based并设置开关通道 */
    if (IO_SetSwitchChannel((SwitchChannel_t)(channel - 1)) != HAL_OK)
    {
        SCPI_Write(context, "ERROR: Set channel failed\r\n", 27);
        return SCPI_RES_ERR;
    }
    SCPI_Write(context, "OK\r\n", 4);
    return SCPI_RES_OK;
}

static scpi_result_t SCPI_ReadSwitchState(scpi_t *context)
{
    int32_t number[2] = {0};
    SCPI_CommandNumbers(context, number, 1, 1);
    /* number[0] = switch index (1-based) */

    /* ================================================================== */
    /*   SWITCH# 匹配检查：命令中的开关编号必须匹配本机设备ID              */
    /*   若 Flash 中 device_id == 0（未配置），则接受任意 SWITCH#          */
    /* ================================================================== */
    uint8_t local_id = Flash_GetDeviceID();
    if (local_id != 0 && number[0] != (int32_t)local_id)
    {
        /* SWITCH# 不匹配本机设备ID → 静默忽略 */
        return SCPI_RES_OK;
    }

    /* 返回当前通道号（1-based，用户友好） */
    SwitchChannel_t ch = IO_GetCurrentChannel();
    SCPI_ResultInt32(context, (int32_t)(ch + 1));
    return SCPI_RES_OK;
}


/* ========================================================================== */
/*              输入读取 (原 debug INPUT 指令)                                 */
/* ========================================================================== */

static scpi_result_t SCPI_ReadInputAllQ(scpi_t *context)
{
    uint8_t input_mask = IO_ReadAllInputs();
    SCPI_ResultInt32(context, (int32_t)input_mask);
    return SCPI_RES_OK;
}

/* ========================================================================== */
/*              输出控制 (原 debug OUTPUT/OUTPUTS 指令)                        */
/* ========================================================================== */

static scpi_result_t SCPI_ConfigureOutput(scpi_t *context)
{
    int32_t output_id;
    if (!SCPI_ParamInt32(context, &output_id, TRUE)) {
        return SCPI_RES_ERR;
    }

    uint32_t state;
    if (!SCPI_ParamUInt32(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (output_id < 0 || output_id >= IO_OUTPUT_COUNT) {
        return SCPI_RES_ERR;
    }

    IO_SetOutput((IO_OutputID_t)output_id, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return SCPI_RES_OK;
}

static scpi_result_t SCPI_ReadOutputAllQ(scpi_t *context)
{
    uint8_t output_mask = IO_ReadAllOutputs();
    SCPI_ResultInt32(context, (int32_t)output_mask);
    return SCPI_RES_OK;
}

/* ========================================================================== */
/*              模式设置 (原 debug MODE 指令)                                 */
/* ========================================================================== */

scpi_choice_def_t io_mode_choice[] = {
    {"DIRECT",  IO_MODE_DIRECT},
    {"COMMAND", IO_MODE_COMMAND},
    {"IO",      IO_MODE_DIRECT},
    {"CMD",     IO_MODE_COMMAND},
    SCPI_CHOICE_LIST_END
};

scpi_choice_def_t on_off_choice[] = {
    {"ON",  1},
    {"OFF", 0},
    {"1",   1},
    {"0",   0},
    SCPI_CHOICE_LIST_END
};

static scpi_result_t SCPI_ConfigureMode(scpi_t *context)
{
    int32_t mode;
    if (!SCPI_ParamChoice(context, io_mode_choice, &mode, TRUE)) {
        return SCPI_RES_ERR;
    }

    IO_SetMode((IO_Mode_t)mode);
    return SCPI_RES_OK;
}

static scpi_result_t SCPI_ConfigureDebug(scpi_t *context)
{
    int32_t state;
    if (!SCPI_ParamChoice(context, on_off_choice, &state, TRUE)) {
        return SCPI_RES_ERR;
    }
    Debug_SetEnabled(state ? 1 : 0);
    if (state) {
        SCPI_Write(context, "Debug ON\r\n", 10);
    } else {
        SCPI_Write(context, "Debug OFF\r\n", 11);
    }
    return SCPI_RES_OK;
}

static scpi_result_t SCPI_ReadModeStateQ(scpi_t *context)
{
    IO_Mode_t mode = IO_GetMode();
    const char *name = "UNKNOWN";

    if (mode == IO_MODE_DIRECT) {
        name = "DIRECT";
    } else if (mode == IO_MODE_COMMAND) {
        name = "COMMAND";
    }

    SCPI_ResultCharacters(context, name, strlen(name));
    return SCPI_RES_OK;
}

/* ========================================================================== */
/*              Flash 配置 SCPI 命令                                           */
/* ========================================================================== */

/**
 * @brief SYSTem:CONFigure:IDENtity <id>
 * @note 设置RS485设备地址 (1-247)
 */
scpi_result_t SCPI_SystemConfigureIdentity(scpi_t *context)
{
    uint32_t id;
    if (!SCPI_ParamUInt32(context, &id, TRUE))
        return SCPI_RES_ERR;

    if (id > 247)
    {
        SCPI_Write(context, "ERROR: Device ID must be 1-247\r\n", 32);
        return SCPI_RES_ERR;
    }

    Flash_SetDeviceID((uint8_t)id);
    SCPI_Write(context, "OK\r\n", 4);
    return SCPI_RES_OK;
}

/**
 * @brief SYSTem:CONFigure:IDENtity?
 * @return 当前设备ID
 */
scpi_result_t SCPI_SystemConfigureIdentityQ(scpi_t *context)
{
    SCPI_ResultUInt32(context, Flash_GetDeviceID());
    return SCPI_RES_OK;
}

/**
 * @brief SYSTem:CONFigure:NAME <name>
 * @note 设置设备名称
 */
scpi_result_t SCPI_SystemConfigureName(scpi_t *context)
{
    const char *name;
    size_t len;

    if (!SCPI_ParamCharacters(context, &name, &len, TRUE))
        return SCPI_RES_ERR;

    if (len >= FLASH_DEVICE_NAME_LEN)
    {
        SCPI_Write(context, "ERROR: Name too long\r\n", 22);
        return SCPI_RES_ERR;
    }

    /* 复制到临时缓冲区并设置 */
    char buf[FLASH_DEVICE_NAME_LEN];
    memset(buf, 0, sizeof(buf));
    strncpy(buf, name, len);
    buf[len] = '\0';

    Flash_SetDeviceName(buf);
    SCPI_Write(context, "OK\r\n", 4);
    return SCPI_RES_OK;
}

/**
 * @brief SYSTem:CONFigure:NAME?
 * @return 设备名称
 */
scpi_result_t SCPI_SystemConfigureNameQ(scpi_t *context)
{
    const char *name = Flash_GetDeviceName();
    SCPI_ResultText(context, name);
    return SCPI_RES_OK;
}

/**
 * @brief SYSTem:CONFigure:SERIAL <sn>
 * @note 设置序列号 (32位无符号整数)
 */
scpi_result_t SCPI_SystemConfigureSerial(scpi_t *context)
{
    uint32_t sn;
    if (!SCPI_ParamUInt32(context, &sn, TRUE))
        return SCPI_RES_ERR;

    Flash_SetSerialNumber(sn);
    SCPI_Write(context, "OK\r\n", 4);
    return SCPI_RES_OK;
}

/**
 * @brief SYSTem:CONFigure:SERIAL?
 * @return 序列号
 */
scpi_result_t SCPI_SystemConfigureSerialQ(scpi_t *context)
{
    SCPI_ResultUInt32(context, Flash_GetSerialNumber());
    return SCPI_RES_OK;
}

/**
 * @brief SYSTem:CONFigure:SAVE
 * @note 将当前配置保存到Flash (持久化)
 */
scpi_result_t SCPI_SystemConfigureSave(scpi_t *context)
{
    Flash_Status_t status = Flash_Save();

    if (status == FLASH_OK)
    {
        SCPI_Write(context, "OK\r\n", 4);
    }
    else
    {
        SCPI_Write(context, "ERROR: Save failed\r\n", 20);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

/**
 * @brief SYSTem:CONFigure:DEFaults
 * @note 恢复配置为默认值 (仅内存，不写入Flash)
 */
scpi_result_t SCPI_SystemConfigureDefaults(scpi_t *context)
{
    Flash_LoadDefaults();
    SCPI_Write(context, "OK\r\n", 4);
    return SCPI_RES_OK;
}

/**
 * @brief SYSTem:CONFigure:STATus?
 * @return 配置状态: "VALID" 或 "INVALID" (首次上电未配置)
 */
scpi_result_t SCPI_SystemConfigureStatusQ(scpi_t *context)
{
    if (Flash_IsValid())
    {
        SCPI_ResultText(context, "VALID");
    }
    else
    {
        SCPI_ResultText(context, "INVALID");
    }
    return SCPI_RES_OK;
}

/* ========================================================================== */
/*              SCPI 命令表                                                    */
/* ========================================================================== */

const scpi_command_t scpi_commands[] = {
    /* IEEE Mandated Commands (SCPI std V1999.0 4.1.1) */
    {
        .pattern = "*CLS",
        .callback = SCPI_CoreCls,
    },
    {
        .pattern = "*IDN?",
        .callback = SCPI_CoreIdnQ,
    },
    {
        .pattern = "*RST",
        .callback = SCPI_CoreRst,
    },
    {
        .pattern = "*STB?",
        .callback = SCPI_CoreStbQ,
    },
    {
        .pattern = "*WAI",
        .callback = SCPI_CoreWai,
    },
    {
        .pattern = "*OPC?",
        .callback = SCPI_CoreOpcQ,
    },
    /* Required SCPI commands (SCPI std V1999.0 4.2.1) */
    {
        .pattern = "SYSTem:ERRor[:NEXT]?",
        .callback = SCPI_SystemErrorNextQ,
    },
    {
        .pattern = "SYSTem:ERRor:COUNt?",
        .callback = SCPI_SystemErrorCountQ,
    },

    /* ---- 开关通道配置 (原 debug SET/GET) ---- */
    {
        .pattern = "CONFigure:SWITch#",
        .callback = SCPI_ConfigureSwitch,
    },
    {
        .pattern = "READ:SWITch#:STATe?",
        .callback = SCPI_ReadSwitchState,
    },

    /* ================================================================== */
    /*  以下为从 debug 指令转换而来的 SCPI 指令                            */
    /* ================================================================== */

    /* ---- 输入读取 (原 debug INPUT) ---- */
    {
        .pattern = "READ:INPut:ALL?",
        .callback = SCPI_ReadInputAllQ,
    },

    /* ---- 输出控制 (原 debug OUTPUT) ---- */
    {
        .pattern = "CONFigure:OUTPut",
        .callback = SCPI_ConfigureOutput,
    },

    /* ---- 输出读取 (原 debug OUTPUTS) ---- */
    {
        .pattern = "READ:OUTPut:ALL?",
        .callback = SCPI_ReadOutputAllQ,
    },

    /* ---- 模式设置 (原 debug MODE) ---- */
    {
        .pattern = "CONFigure:MODE",
        .callback = SCPI_ConfigureMode,
    },
    {
        .pattern = "READ:MODE:STATe?",
        .callback = SCPI_ReadModeStateQ,
    },

    /* ---- Debug 控制 ---- */
    {
        .pattern = "CONFigure:DEBUG",
        .callback = SCPI_ConfigureDebug,
    },

    /* ================================================================== */
    /*              Flash 配置管理 (SYSTem 子系统)                          */
    /* ================================================================== */

    /* ---- 设备ID ---- */
    {
        .pattern = "SYSTem:CONFigure:IDENtity",
        .callback = SCPI_SystemConfigureIdentity,
    },
    {
        .pattern = "SYSTem:CONFigure:IDENtity?",
        .callback = SCPI_SystemConfigureIdentityQ,
    },

    /* ---- 设备名称 ---- */
    {
        .pattern = "SYSTem:CONFigure:NAME",
        .callback = SCPI_SystemConfigureName,
    },
    {
        .pattern = "SYSTem:CONFigure:NAME?",
        .callback = SCPI_SystemConfigureNameQ,
    },

    /* ---- 序列号 ---- */
    {
        .pattern = "SYSTem:CONFigure:SERIAL",
        .callback = SCPI_SystemConfigureSerial,
    },
    {
        .pattern = "SYSTem:CONFigure:SERIAL?",
        .callback = SCPI_SystemConfigureSerialQ,
    },

    /* ---- 保存/默认/状态 ---- */
    {
        .pattern = "SYSTem:CONFigure:SAVE",
        .callback = SCPI_SystemConfigureSave,
    },
    {
        .pattern = "SYSTem:CONFigure:DEFaults",
        .callback = SCPI_SystemConfigureDefaults,
    },
    {
        .pattern = "SYSTem:CONFigure:STATus?",
        .callback = SCPI_SystemConfigureStatusQ,
    },

    SCPI_CMD_LIST_END};

scpi_interface_t scpi_interface = {
    .error = SCPI_Error,
    .write = SCPI_Write,
    .control = SCPI_Control,
    .flush = SCPI_Flush,
    .reset = SCPI_Reset,
};

char scpi_input_buffer[SCPI_INPUT_BUFFER_LENGTH];
scpi_error_t scpi_error_queue_data[SCPI_ERROR_QUEUE_SIZE];

scpi_t scpi_context;
