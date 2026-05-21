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

/* USER CODE BEGIN Modified for NetBus */
/**
  ******************************************************************************
  * @file    scpi_port.c
  * @brief   SCPI port layer — UART7 via Log module
  *
  *          NetBus adaptation:
  *            - SCPI_Write() routes raw output through Log_WriteRaw() → UART7,
  *              producing clean SCPI responses without timestamps or level tags.
  *            - RX is handled externally by the Log module's DMA + IDLE pipeline.
  *              SCPI command lines arrive via SCPI_TryParse() in log_task.c.
  *            - Error/control messages use Log_Print() for timestamped output.
  ******************************************************************************
  */
/* USER CODE END Modified for NetBus */

#include <stdio.h>
#include <string.h>
#include "scpi/scpi.h"
#include "scpi/ieee488.h"
#include "scpi-def.h"
#include "log.h"

/* ── SCPI Interface Callbacks ──────────────────────────────────────────────── */

/**
 * @brief  Write raw data to the SCPI output channel (UART7)
  * @note   Uses Log_WriteRaw() for clean output — no timestamp, no level tag.
  *         This is critical for SCPI protocol compliance: responses like *IDN?
  *         must return exactly "NetBus,PPA-NB100,..." without decoration.
  */
size_t SCPI_Write(scpi_t *context, const char *data, size_t len)
{
    (void)context;
    Log_WriteRaw(data, len);
    return len;
}

/**
  * @brief  Report SCPI parser errors via the log system
  * @note   Unlike SCPI_Write, errors include timestamp/level for diagnostics.
  *         When err == 0, the error queue is empty — nothing to print.
  */
int SCPI_Error(scpi_t *context, int_fast16_t err)
{
    (void)context;
    if (err != 0)
    {
        LOG_ERROR("SCPI: %d, \"%s\"", (int16_t)err, SCPI_ErrorTranslate(err));
    }
    return 0;
}

/**
  * @brief  Handle SCPI control events (SRQ, etc.)
  * @note   Printed as INFO-level log for operator visibility.
  */
scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val)
{
    (void)context;
    if (SCPI_CTRL_SRQ == ctrl)
    {
        LOG_INFO("SCPI SRQ: 0x%X (%d)", val, val);
    }
    else
    {
        LOG_INFO("SCPI CTRL %02x: 0x%X (%d)", ctrl, val, val);
    }
    return SCPI_RES_OK;
}

/**
  * @brief  Handle SCPI device reset notification
  */
scpi_result_t SCPI_Reset(scpi_t *context)
{
    (void)context;
    LOG_INFO("SCPI: Reset");
    return SCPI_RES_OK;
}

/**
  * @brief  Flush SCPI output
  * @note   No-op — HAL_UART_Transmit (called by Log_WriteRaw) is blocking,
  *         so output is already flushed by the time it returns.
  */
scpi_result_t SCPI_Flush(scpi_t *context)
{
    (void)context;
    return SCPI_RES_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  IEEE 488.2 Status Register Model                                          */
/*  (migrated from libscpi ieee488.c — required by error.c and parser core)   */
/* ═══════════════════════════════════════════════════════════════════════════ */

static const scpi_reg_info_t scpi_reg_details[SCPI_REG_COUNT] = {
    { SCPI_REG_CLASS_STB,  SCPI_REG_GROUP_STB  },
    { SCPI_REG_CLASS_SRE,  SCPI_REG_GROUP_STB  },
    { SCPI_REG_CLASS_EVEN, SCPI_REG_GROUP_ESR  },
    { SCPI_REG_CLASS_ENAB, SCPI_REG_GROUP_ESR  },
    { SCPI_REG_CLASS_EVEN, SCPI_REG_GROUP_OPER },
    { SCPI_REG_CLASS_ENAB, SCPI_REG_GROUP_OPER },
    { SCPI_REG_CLASS_COND, SCPI_REG_GROUP_OPER },
    { SCPI_REG_CLASS_EVEN, SCPI_REG_GROUP_QUES },
    { SCPI_REG_CLASS_ENAB, SCPI_REG_GROUP_QUES },
    { SCPI_REG_CLASS_COND, SCPI_REG_GROUP_QUES },
};

static const scpi_reg_group_info_t scpi_reg_group_details[SCPI_REG_GROUP_COUNT] = {
    {
        SCPI_REG_STB, SCPI_REG_SRE,
        SCPI_REG_NONE, SCPI_REG_NONE, SCPI_REG_NONE, SCPI_REG_NONE, 0
    }, /* SCPI_REG_GROUP_STB */
    {
        SCPI_REG_ESR, SCPI_REG_ESE,
        SCPI_REG_NONE, SCPI_REG_NONE, SCPI_REG_NONE,
        SCPI_REG_STB, STB_ESR
    }, /* SCPI_REG_GROUP_ESR */
    {
        SCPI_REG_OPER, SCPI_REG_OPERE, SCPI_REG_OPERC,
        SCPI_REG_NONE, SCPI_REG_NONE,
        SCPI_REG_STB, STB_OPS
    }, /* SCPI_REG_GROUP_OPER */
    {
        SCPI_REG_QUES, SCPI_REG_QUESE, SCPI_REG_QUESC,
        SCPI_REG_NONE, SCPI_REG_NONE,
        SCPI_REG_STB, STB_QES
    }, /* SCPI_REG_GROUP_QUES */
};

static size_t _reg_write_control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val)
{
    if (context && context->interface && context->interface->control)
        return context->interface->control(context, ctrl, val);
    return 0;
}

scpi_reg_val_t SCPI_RegGet(scpi_t *context, scpi_reg_name_t name)
{
    if ((name < SCPI_REG_COUNT) && context)
        return context->registers[name];
    return 0;
}

void SCPI_RegSet(scpi_t *context, scpi_reg_name_t name, scpi_reg_val_t val)
{
    if ((name >= SCPI_REG_COUNT) || (context == NULL))
        return;

    scpi_reg_group_info_t register_group;

    do {
        scpi_reg_class_t register_type = scpi_reg_details[name].type;
        register_group = scpi_reg_group_details[scpi_reg_details[name].group];

        scpi_reg_val_t ptrans;
        scpi_reg_val_t old_val = context->registers[name];

        if (old_val == val)
            return;

        context->registers[name] = val;

        switch (register_type) {
            case SCPI_REG_CLASS_STB:
            case SCPI_REG_CLASS_SRE: {
                scpi_reg_val_t stb = context->registers[SCPI_REG_STB] & ~STB_SRQ;
                scpi_reg_val_t sre = context->registers[SCPI_REG_SRE] & ~STB_SRQ;

                if (stb & sre) {
                    ptrans = ((old_val ^ val) & val);
                    context->registers[SCPI_REG_STB] |= STB_SRQ;
                    if (ptrans & val)
                        _reg_write_control(context, SCPI_CTRL_SRQ, context->registers[SCPI_REG_STB]);
                } else {
                    context->registers[SCPI_REG_STB] &= ~STB_SRQ;
                }
                break;
            }
            case SCPI_REG_CLASS_EVEN: {
                scpi_reg_val_t enable;
                if (register_group.enable != SCPI_REG_NONE)
                    enable = SCPI_RegGet(context, register_group.enable);
                else
                    enable = 0xFFFF;

                scpi_bool_t summary = val & enable;
                name = register_group.parent_reg;
                val = SCPI_RegGet(context, register_group.parent_reg);
                if (summary)
                    val |= register_group.parent_bit;
                else
                    val &= ~(register_group.parent_bit);
                break;
            }
            case SCPI_REG_CLASS_COND: {
                name = register_group.event;
                if (register_group.ptfilt == SCPI_REG_NONE && register_group.ntfilt == SCPI_REG_NONE) {
                    val = ((old_val ^ val) & val) | SCPI_RegGet(context, register_group.event);
                } else {
                    scpi_reg_val_t ptfilt = 0, ntfilt = 0;
                    scpi_reg_val_t transitions, ntrans;

                    if (register_group.ptfilt != SCPI_REG_NONE)
                        ptfilt = SCPI_RegGet(context, register_group.ptfilt);
                    if (register_group.ntfilt != SCPI_REG_NONE)
                        ntfilt = SCPI_RegGet(context, register_group.ntfilt);

                    transitions = old_val ^ val;
                    ptrans = transitions & val;
                    ntrans = transitions & ~ptrans;

                    val = ((ptrans & ptfilt) | (ntrans & ntfilt)) | SCPI_RegGet(context, register_group.event);
                }
                break;
            }
            case SCPI_REG_CLASS_ENAB:
            case SCPI_REG_CLASS_NTR:
            case SCPI_REG_CLASS_PTR:
                return;
        }
    } while (register_group.parent_reg != SCPI_REG_NONE);
}

void SCPI_RegSetBits(scpi_t *context, scpi_reg_name_t name, scpi_reg_val_t bits)
{
    SCPI_RegSet(context, name, SCPI_RegGet(context, name) | bits);
}

void SCPI_RegClearBits(scpi_t *context, scpi_reg_name_t name, scpi_reg_val_t bits)
{
    SCPI_RegSet(context, name, SCPI_RegGet(context, name) & ~bits);
}
