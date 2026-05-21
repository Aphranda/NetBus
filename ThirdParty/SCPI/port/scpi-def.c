/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    scpi-def.c
  * @brief   NetBus SCPI command table and callbacks
  *
  *          Architecture:
  *            - SCPI callbacks call directly into IO/CAN/Modbus/Detector
  *              driver APIs — no dependency on debug CLI handlers.
  *            - All TX output goes through SCPI_Write(), which routes to
  *              Log_Print() (and thus UART7).
  *            - Responses use standard SCPI result functions when possible;
  *              free-text responses use SCPI_Write() for multi-line output.
  ******************************************************************************
  */
/* USER CODE END Header */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scpi/scpi.h"
#include "scpi/error.h"
#include "scpi/ieee488.h"
#include "scpi-def.h"
#include "io.h"
#include "can.h"
#include "modbus.h"
#include "detector_task.h"
#include "log.h"

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Internal helpers                                                           */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
  * @brief  Extract node number from SCPI command header suffix
  *         e.g. DETector1 → numbers[0]=1, DET2 → 2, DET255 → 255
  */
static int32_t _get_node(scpi_t *context)
{
    int32_t numbers[2] = {1, 0};
    SCPI_CommandNumbers(context, numbers, 1, 1);
    return numbers[0];
}

/**
  * @brief  Extract switch address from ROUTe:SWITch# command header
  */
static int32_t _get_switch_addr(scpi_t *context)
{
    int32_t numbers[2] = {1, 0};
    SCPI_CommandNumbers(context, numbers, 1, 1);
    return numbers[0];
}

/**
  * @brief  Append formatted text to SCPI output
  */
static void _write(scpi_t *context, const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n > 0) SCPI_Write(context, buf, (size_t)n);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  IEEE 488.2 Common Commands                                                 */
/* ═══════════════════════════════════════════════════════════════════════════ */

scpi_result_t SCPI_CoreCls(scpi_t *context)
{
    SCPI_ErrorClear(context);
    SCPI_RegSet(context, SCPI_REG_ESR,  0);
    SCPI_RegSet(context, SCPI_REG_OPER, 0);
    SCPI_RegSet(context, SCPI_REG_QUES, 0);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CoreIdnQ(scpi_t *context)
{
    _write(context, "%s,%s,%s,%s", SCPI_IDN1, SCPI_IDN2, SCPI_IDN3, SCPI_IDN4);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CoreRst(scpi_t *context)
{
    /* Reset local attenuators to zero */
    IO_SetAttenuatorA(0U);
    IO_SetAttenuatorB(0U);

    /* Clear log level back to INFO */
    Log_SetLevel(LOG_LEVEL_INFO);

    _write(context, "OK");
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CoreStbQ(scpi_t *context)
{
    /* Simple: always return 0 (no SRQ pending) */
    SCPI_ResultUInt32(context, 0U);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CoreWai(scpi_t *context)
{
    (void)context;
    return SCPI_RES_OK;
}

scpi_result_t SCPI_CoreOpcQ(scpi_t *context)
{
    SCPI_ResultUInt32(context, 1U);
    return SCPI_RES_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  SYSTem subsystem                                                           */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* ── SYSTem:ATTenuator:A ──────────────────────────────────────────────────── */

scpi_result_t SCPI_SystemAttA(scpi_t *context)
{
    uint32_t value;
    if (!SCPI_ParamUInt32(context, &value, TRUE))
        return SCPI_RES_ERR;
    if (value > 15U)
    {
        _write(context, "ERROR: Value out of range (0-15)");
        return SCPI_RES_ERR;
    }
    IO_SetAttenuatorA((uint8_t)value);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SystemAttAQ(scpi_t *context)
{
    SCPI_ResultUInt32(context, IO_GetAttenuatorA());
    return SCPI_RES_OK;
}

/* ── SYSTem:ATTenuator:B ──────────────────────────────────────────────────── */

scpi_result_t SCPI_SystemAttB(scpi_t *context)
{
    uint32_t value;
    if (!SCPI_ParamUInt32(context, &value, TRUE))
        return SCPI_RES_ERR;
    if (value > 15U)
    {
        _write(context, "ERROR: Value out of range (0-15)");
        return SCPI_RES_ERR;
    }
    IO_SetAttenuatorB((uint8_t)value);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SystemAttBQ(scpi_t *context)
{
    SCPI_ResultUInt32(context, IO_GetAttenuatorB());
    return SCPI_RES_OK;
}

/* ── SYSTem:COMMunicate:CAN:SEND ──────────────────────────────────────────── */

scpi_result_t SCPI_SystemCommCanSend(scpi_t *context)
{
    uint32_t can_id;
    if (!SCPI_ParamUInt32(context, &can_id, TRUE))
        return SCPI_RES_ERR;

    if (can_id > 0x7FFU)
    {
        _write(context, "ERROR: CAN ID out of range (0x000-0x7FF)");
        return SCPI_RES_ERR;
    }

    /* Collect data bytes — up to CAN_MAX_DATA_LEN (64) */
    CAN_Msg_t tx_msg;
    memset(&tx_msg, 0, sizeof(tx_msg));
    tx_msg.id = can_id;

    uint8_t data_len = 0U;
    while (data_len < CAN_MAX_DATA_LEN)
    {
        uint32_t byte_val;
        if (!SCPI_ParamUInt32(context, &byte_val, FALSE))
            break;
        if (byte_val > 0xFFU) break;
        tx_msg.data[data_len++] = (uint8_t)byte_val;
    }
    tx_msg.dlc = data_len;

    if (CAN_Send(&tx_msg) != HAL_OK)
    {
        _write(context, "ERROR: CAN send failed");
        return SCPI_RES_ERR;
    }

    /* Collect responses for up to 500 ms */
    CAN_Msg_t rx_msg;
    uint8_t resp_count = 0U;
    uint32_t deadline = HAL_GetTick() + 500U;

    while (HAL_GetTick() < deadline)
    {
        if (CAN_GetRxMessage(&rx_msg) == HAL_OK)
        {
            if (resp_count++ == 0U)
                SCPI_Write(context, "", 0);  /* begin response */
            _write(context, "#%u,%u,%u:", resp_count, (unsigned)rx_msg.id, (unsigned)rx_msg.dlc);
            char hex_buf[3];
            for (uint8_t i = 0U; i < rx_msg.dlc; i++)
            {
                snprintf(hex_buf, sizeof(hex_buf), " %02X", rx_msg.data[i]);
                SCPI_Write(context, hex_buf, 3);
            }
            SCPI_Write(context, "\r\n", 2);
        }
    }

    if (resp_count == 0U)
        _write(context, "0 responses");
    return SCPI_RES_OK;
}

/* ── SYSTem:COMMunicate:CAN:SCAN? ─────────────────────────────────────────── */

scpi_result_t SCPI_SystemCommCanScanQ(scpi_t *context)
{
    uint32_t start_id = 1U;
    uint32_t end_id   = 40U;

    SCPI_ParamUInt32(context, &start_id, FALSE);
    SCPI_ParamUInt32(context, &end_id,   FALSE);

    if (start_id < 1U)  start_id = 1U;
    if (end_id > 255U)  end_id   = 255U;
    if (end_id < start_id) end_id = start_id;

    /* Drain stale RX before scanning */
    CAN_Msg_t dummy;
    while (CAN_GetRxMessage(&dummy) == HAL_OK) {}

    /* Send SN queries for each node */
    CAN_Msg_t query;
    memset(&query, 0, sizeof(query));
    query.id  = 0x101U;
    query.dlc = 1U;

    uint32_t t0 = HAL_GetTick();
    for (uint32_t n = start_id; n <= end_id; n++)
    {
        query.data[0] = (uint8_t)n;
        CAN_Send(&query);
    }
    uint32_t tx_ms = HAL_GetTick() - t0;

    /* Collect responses */
    uint32_t deadline = HAL_GetTick() + 500U;
    uint8_t found = 0U;

    while (HAL_GetTick() < deadline)
    {
        CAN_Msg_t rx;
        if (CAN_GetRxMessage(&rx) == HAL_OK)
        {
            /* Decode: rx.data[0]=0x01 cmd echo, [1]=status, [2]=sn_len, [3+]=SN */
            uint8_t sn_len = (rx.dlc > 3U) ? rx.data[2] : 0U;
            if (sn_len > 61U) sn_len = 61U;
            char sn[62];
            memcpy(sn, &rx.data[3], sn_len);
            sn[sn_len] = '\0';

            _write(context, "%u,%u,%u,\"%s\"", found + 1U,
                   (unsigned)rx.id, (unsigned)sn_len, sn);
            found++;
        }
    }

    char header[80];
    snprintf(header, sizeof(header), "%u nodes found (%u queries in %lu ms)",
             found, (unsigned)(end_id - start_id + 1U), tx_ms);
    /* Prefix: count on the first returned line */
    (void)header;

    if (found == 0U)
        SCPI_ResultUInt32(context, 0U);
    else
        SCPI_ResultUInt32(context, found);

    return SCPI_RES_OK;
}

/* ── SYSTem:DETector#:SN? ─────────────────────────────────────────────────── */

scpi_result_t SCPI_SystemDetSnQ(scpi_t *context)
{
    int32_t node = _get_node(context);
    Detector_SN_t result;
    memset(&result, 0, sizeof(result));

    Detector_Status_t st = Detector_ReadSN((uint8_t)node, &result);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: Node %ld read SN failed", (long)node);
        return SCPI_RES_ERR;
    }

    result.sn[result.sn_len] = '\0';
    SCPI_ResultText(context, (const char *)result.sn);
    return SCPI_RES_OK;
}

/* ── SYSTem:DETector#:VERSion? ────────────────────────────────────────────── */

scpi_result_t SCPI_SystemDetVersionQ(scpi_t *context)
{
    int32_t node = _get_node(context);
    Detector_Version_t ver;
    memset(&ver, 0, sizeof(ver));

    Detector_Status_t st = Detector_ReadVersion((uint8_t)node, &ver);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: Node %ld read version failed", (long)node);
        return SCPI_RES_ERR;
    }

    _write(context, "%u.%u.%u", ver.major, ver.minor, ver.patch);
    return SCPI_RES_OK;
}

/* ── SYSTem:DETector#:RESet ───────────────────────────────────────────────── */

scpi_result_t SCPI_SystemDetReset(scpi_t *context)
{
    int32_t node = _get_node(context);
    Detector_Status_t st = Detector_Reset((uint8_t)node);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: Node %ld reset failed", (long)node);
        return SCPI_RES_ERR;
    }
    _write(context, "OK");
    return SCPI_RES_OK;
}

/* ── SYSTem:DETector#:LED ─────────────────────────────────────────────────── */

scpi_result_t SCPI_SystemDetLed(scpi_t *context)
{
    int32_t node = _get_node(context);

    uint32_t state = 0U;
    if (!SCPI_ParamUInt32(context, &state, TRUE))
        return SCPI_RES_ERR;

    uint32_t period = 0U;
    SCPI_ParamUInt32(context, &period, FALSE);
    if (period > 65535U) period = 0U;

    Detector_Status_t st = Detector_LEDControl((uint8_t)node,
        (uint8_t)(state ? 1U : 0U), (uint16_t)period);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: LED control failed");
        return SCPI_RES_ERR;
    }
    return SCPI_RES_OK;
}

/* ── SYSTem:DETector#:ADDRess ─────────────────────────────────────────────── */

static scpi_choice_def_t _addr_reset_choice[] = {
    {"RESet", -1},
    SCPI_CHOICE_LIST_END
};

scpi_result_t SCPI_SystemDetAddress(scpi_t *context)
{
    int32_t node = _get_node(context);

    /* Check if the parameter is the string "RESet" */
    int32_t choice = 0;
    if (SCPI_ParamChoice(context, _addr_reset_choice, &choice, FALSE) && choice == -1)
    {
        /* Reset Node ID to default */
        Detector_NodeId_t result;
        Detector_Status_t st = Detector_WriteNodeId((uint8_t)node, 1U, 1U, &result);
        if (st != DETECTOR_OK)
        {
            _write(context, "ERROR: Node ID reset failed");
            return SCPI_RES_ERR;
        }
        _write(context, "OK (reset to default)");
        return SCPI_RES_OK;
    }

    /* Numeric new ID */
    uint32_t new_id = 0U;
    if (!SCPI_ParamUInt32(context, &new_id, TRUE))
        return SCPI_RES_ERR;
    if (new_id < 1U || new_id > 40U)
    {
        _write(context, "ERROR: Node ID out of range (1-40)");
        return SCPI_RES_ERR;
    }

    Detector_NodeId_t result;
    Detector_Status_t st = Detector_WriteNodeId((uint8_t)node, 0U,
        (uint8_t)new_id, &result);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: Write Node ID failed");
        return SCPI_RES_ERR;
    }
    _write(context, "OK");
    return SCPI_RES_OK;
}

/* ── SYSTem:DETector#:FLASh:INFO? ─────────────────────────────────────────── */

scpi_result_t SCPI_SystemDetFlashInfoQ(scpi_t *context)
{
    int32_t node = _get_node(context);
    Detector_FlashInfo_t info;
    memset(&info, 0, sizeof(info));

    Detector_Status_t st = Detector_GetFlashInfo((uint8_t)node, &info);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: Flash info query failed");
        return SCPI_RES_ERR;
    }

    _write(context,
        "JEDEC ID: 0x%06lX, Status: 0x%02X\r\n"
        "OTA image:   0x%06lX (%lu KB)\r\n"
        "H coarse:    0x%06lX (%lu KB)\r\n"
        "H fine:      0x%06lX (%lu KB)\r\n"
        "V coarse:    0x%06lX (%lu KB)\r\n"
        "V fine:      0x%06lX (%lu KB)\r\n"
        "Ext cal:     0x%06lX (%lu KB)\r\n"
        "Reserved:    0x%06lX (%lu KB)",
        (unsigned long)info.jedec_id, (unsigned)info.status_reg1,
        (unsigned long)info.ota_img_addr, (unsigned long)(info.ota_img_size / 1024UL),
        (unsigned long)info.cal_h_coarse_addr, (unsigned long)(info.cal_h_coarse_size / 1024UL),
        (unsigned long)info.cal_h_fine_addr, (unsigned long)(info.cal_h_fine_size / 1024UL),
        (unsigned long)info.cal_v_coarse_addr, (unsigned long)(info.cal_v_coarse_size / 1024UL),
        (unsigned long)info.cal_v_fine_addr, (unsigned long)(info.cal_v_fine_size / 1024UL),
        (unsigned long)info.cal_ext_addr, (unsigned long)(info.cal_ext_size / 1024UL),
        (unsigned long)info.reserved_addr, (unsigned long)(info.reserved_size / 1024UL));
    return SCPI_RES_OK;
}

/* ── SYSTem:DETector#:FLASh:DATA? ─────────────────────────────────────────── */

scpi_result_t SCPI_SystemDetFlashDataQ(scpi_t *context)
{
    int32_t node = _get_node(context);

    uint32_t addr;
    if (!SCPI_ParamUInt32(context, &addr, TRUE))
        return SCPI_RES_ERR;

    uint32_t len = 32U;
    SCPI_ParamUInt32(context, &len, FALSE);
    if (len < 1U || len > 58U) len = 32U;

    Detector_FlashRead_t result;
    memset(&result, 0, sizeof(result));

    Detector_Status_t st = Detector_ReadFlash((uint8_t)node, addr,
        (uint8_t)len, &result);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: Flash read failed");
        return SCPI_RES_ERR;
    }

    /* Return as hex block */
    char hex[4];
    for (uint8_t i = 0U; i < result.data_len; i++)
    {
        snprintf(hex, sizeof(hex), "%02X ", result.data[i]);
        SCPI_Write(context, hex, 3);
    }
    return SCPI_RES_OK;
}

/* ── SYSTem:ERRor ─────────────────────────────────────────────────────────── */

scpi_result_t SCPI_SystemErrorNextQ(scpi_t *context)
{
    (void)context;
    SCPI_ResultInt32(context, 0);
    SCPI_ResultText(context, "No error");
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SystemErrorCountQ(scpi_t *context)
{
    SCPI_ResultUInt32(context, 0U);
    return SCPI_RES_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  SENSe subsystem                                                            */
/* ═══════════════════════════════════════════════════════════════════════════ */

scpi_result_t SCPI_SenseDetControl(scpi_t *context)
{
    int32_t node = _get_node(context);

    uint32_t mode, hold, thr;
    if (!SCPI_ParamUInt32(context, &mode, TRUE)) return SCPI_RES_ERR;
    if (!SCPI_ParamUInt32(context, &hold, TRUE)) return SCPI_RES_ERR;
    if (!SCPI_ParamUInt32(context, &thr,  TRUE)) return SCPI_RES_ERR;

    uint32_t gate = 0U;
    SCPI_ParamUInt32(context, &gate, FALSE);

    if (mode > 2U || hold > 255U || thr > 65535U || gate > 65535U)
    {
        _write(context, "ERROR: Parameter out of range");
        return SCPI_RES_ERR;
    }

    Detector_ControlResult_t result;
    memset(&result, 0, sizeof(result));

    Detector_Status_t st = Detector_Control((uint8_t)node, (Detector_Mode_t)mode,
        (uint8_t)hold, (uint16_t)thr, (uint16_t)gate, &result);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: Control command failed");
        return SCPI_RES_ERR;
    }

    _write(context, "%d,%d,%u,%u", result.h_adc_avg, result.v_adc_avg,
           (unsigned)result.mode, (unsigned)result.state);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SenseDetStop(scpi_t *context)
{
    int32_t node = _get_node(context);
    Detector_Status_t st = Detector_Stop((uint8_t)node);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: Stop failed");
        return SCPI_RES_ERR;
    }
    _write(context, "OK");
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SenseDetTempQ(scpi_t *context)
{
    int32_t node = _get_node(context);

    uint32_t det = 1U, mcu = 1U;
    SCPI_ParamUInt32(context, &det, FALSE);
    SCPI_ParamUInt32(context, &mcu, FALSE);

    Detector_Temp_t result;
    memset(&result, 0, sizeof(result));

    Detector_Status_t st = Detector_ReadTemperature((uint8_t)node,
        (uint8_t)(det ? 1U : 0U), (uint8_t)(mcu ? 1U : 0U), &result);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: Temperature read failed");
        return SCPI_RES_ERR;
    }

    _write(context, "%d,%d,%d", result.detector_temp, result.mcu_temp, result.reserved);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SenseDetPowerQ(scpi_t *context)
{
    int32_t node = _get_node(context);

    uint32_t khz, hold, mode, thr, gate;
    if (!SCPI_ParamUInt32(context, &khz,  TRUE)) return SCPI_RES_ERR;
    if (!SCPI_ParamUInt32(context, &hold, TRUE)) return SCPI_RES_ERR;
    if (!SCPI_ParamUInt32(context, &mode, TRUE)) return SCPI_RES_ERR;
    if (!SCPI_ParamUInt32(context, &thr,  TRUE)) return SCPI_RES_ERR;
    if (!SCPI_ParamUInt32(context, &gate, TRUE)) return SCPI_RES_ERR;

    Detector_Power_t result;
    memset(&result, 0, sizeof(result));

    Detector_Status_t st = Detector_QueryPower((uint8_t)node, khz,
        (uint8_t)hold, (Detector_Mode_t)mode, (uint16_t)thr, (uint16_t)gate,
        0U, &result);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: Power query failed");
        return SCPI_RES_ERR;
    }

    _write(context, "%d,%d,%u,%u",
           result.h_power_dbm100, result.v_power_dbm100,
           (unsigned)result.mode, (unsigned)result.state);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SenseDetBand(scpi_t *context)
{
    int32_t node = _get_node(context);

    uint32_t mhz, mode, mask;
    if (!SCPI_ParamUInt32(context, &mhz,  TRUE)) return SCPI_RES_ERR;
    if (!SCPI_ParamUInt32(context, &mode, TRUE)) return SCPI_RES_ERR;
    if (!SCPI_ParamUInt32(context, &mask, TRUE)) return SCPI_RES_ERR;

    Detector_BandResult_t result;
    memset(&result, 0, sizeof(result));

    Detector_Status_t st = Detector_SelectBand((uint8_t)node, (uint8_t)mhz,
        (uint8_t)mode, (uint64_t)mask, &result);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: Band select failed");
        return SCPI_RES_ERR;
    }

    _write(context, "%u,%u,%u",
           (unsigned)result.mode, (unsigned)result.v_selected, (unsigned)result.h_selected);
    return SCPI_RES_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  SOURce subsystem                                                           */
/* ═══════════════════════════════════════════════════════════════════════════ */

scpi_result_t SCPI_SourceDetFreq(scpi_t *context)
{
    int32_t node = _get_node(context);

    uint32_t ch, khz;
    if (!SCPI_ParamUInt32(context, &ch,  TRUE)) return SCPI_RES_ERR;
    if (!SCPI_ParamUInt32(context, &khz, TRUE)) return SCPI_RES_ERR;

    uint32_t pwr = 0U;
    SCPI_ParamUInt32(context, &pwr, FALSE);
    if (pwr > 63U) pwr = 0U;

    Detector_FreqResult_t result;
    memset(&result, 0, sizeof(result));

    Detector_Status_t st = Detector_SetFrequency((uint8_t)node,
        (Detector_Channel_t)ch, khz, (uint8_t)pwr, &result);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: Frequency set failed");
        return SCPI_RES_ERR;
    }

    _write(context, "%u,%lu,%u",
           (unsigned)result.channel, (unsigned long)result.freq_hz, (unsigned)result.lmx_power);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SourceDetPower(scpi_t *context)
{
    int32_t node = _get_node(context);

    uint32_t ch, khz, dbm100;
    if (!SCPI_ParamUInt32(context, &ch,    TRUE)) return SCPI_RES_ERR;
    if (!SCPI_ParamUInt32(context, &khz,   TRUE)) return SCPI_RES_ERR;
    if (!SCPI_ParamUInt32(context, &dbm100, TRUE)) return SCPI_RES_ERR;

    uint32_t gps = 0U, comp = 0U;
    SCPI_ParamUInt32(context, &gps,  FALSE);
    SCPI_ParamUInt32(context, &comp, FALSE);

    Detector_TxPowerResult_t result;
    memset(&result, 0, sizeof(result));

    Detector_Status_t st = Detector_SetTxPower((uint8_t)node,
        (Detector_Channel_t)ch, khz, (int16_t)dbm100,
        (uint8_t)(gps ? 1U : 0U), (uint8_t)(comp ? 1U : 0U), &result);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: Tx power set failed");
        return SCPI_RES_ERR;
    }

    _write(context, "%u,%lu,%u,%u,%u,%u,%u,%u,%u,%d",
           (unsigned)result.channel, (unsigned long)result.freq_hz,
           (unsigned)result.att1, (unsigned)result.att2, (unsigned)result.att3,
           (unsigned)result.att4, (unsigned)result.att5, (unsigned)result.att6,
           (unsigned)result.att7, result.predicted_power_dbm100);
    return SCPI_RES_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  ROUTe subsystem                                                            */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* ── ROUTe:DETector#:SWITch ───────────────────────────────────────────────── */

scpi_result_t SCPI_RouteDetSwitch(scpi_t *context)
{
    int32_t node = _get_node(context);

    uint32_t sw[6];
    for (int i = 0; i < 6; i++)
    {
        if (!SCPI_ParamUInt32(context, &sw[i], TRUE))
            return SCPI_RES_ERR;
        if (sw[i] > 1U) sw[i] = 2U;  /* >=2 means keep current */
    }

    Detector_Status_t st = Detector_SwitchControl((uint8_t)node,
        (uint8_t)sw[0], (uint8_t)sw[1], (uint8_t)sw[2],
        (uint8_t)sw[3], (uint8_t)sw[4], (uint8_t)sw[5]);
    if (st != DETECTOR_OK)
    {
        _write(context, "ERROR: Switch control failed");
        return SCPI_RES_ERR;
    }
    return SCPI_RES_OK;
}

/* ── ROUTe:SWITch#:CHANnel? ───────────────────────────────────────────────── */

scpi_result_t SCPI_RouteSwitchChannelQ(scpi_t *context)
{
    int32_t addr = _get_switch_addr(context);
    uint16_t reg_val = 0U;

    Modbus_Result_t mr = Modbus_ReadHoldingRegisters((uint8_t)addr, 0x0000U, 1U, &reg_val);
    if (mr.status != MODBUS_OK)
    {
        _write(context, "ERROR: Modbus read failed (%s)", Modbus_StatusString(mr.status));
        return SCPI_RES_ERR;
    }

    SCPI_ResultUInt32(context, reg_val);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_RouteSwitchChannel(scpi_t *context)
{
    int32_t addr = _get_switch_addr(context);

    uint32_t ch;
    if (!SCPI_ParamUInt32(context, &ch, TRUE))
        return SCPI_RES_ERR;
    if (ch < 1U || ch > 10U)
    {
        _write(context, "ERROR: Channel out of range (1-10)");
        return SCPI_RES_ERR;
    }

    Modbus_Result_t mr = Modbus_WriteSingleRegister((uint8_t)addr, 0x0000U, (uint16_t)ch);
    if (mr.status != MODBUS_OK)
    {
        _write(context, "ERROR: Write failed (%s)", Modbus_StatusString(mr.status));
        return SCPI_RES_ERR;
    }
    return SCPI_RES_OK;
}

/* ── ROUTe:SWITch#:MODE? / MODE ───────────────────────────────────────────── */

static scpi_choice_def_t _rfsw_mode_choice[] = {
    {"IO",  0},
    {"CMD", 1},
    {"DIRECT", 0},
    {"COMMAND", 1},
    SCPI_CHOICE_LIST_END
};

scpi_result_t SCPI_RouteSwitchModeQ(scpi_t *context)
{
    int32_t addr = _get_switch_addr(context);
    uint16_t reg_val = 0U;

    Modbus_Result_t mr = Modbus_ReadHoldingRegisters((uint8_t)addr, 0x0001U, 1U, &reg_val);
    if (mr.status != MODBUS_OK)
    {
        _write(context, "ERROR: Modbus read failed (%s)", Modbus_StatusString(mr.status));
        return SCPI_RES_ERR;
    }

    SCPI_ResultUInt32(context, reg_val);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_RouteSwitchMode(scpi_t *context)
{
    int32_t addr = _get_switch_addr(context);

    int32_t mode;
    if (!SCPI_ParamChoice(context, _rfsw_mode_choice, &mode, TRUE))
        return SCPI_RES_ERR;

    Modbus_Result_t mr = Modbus_WriteSingleRegister((uint8_t)addr, 0x0001U, (uint16_t)mode);
    if (mr.status != MODBUS_OK)
    {
        _write(context, "ERROR: Write mode failed (%s)", Modbus_StatusString(mr.status));
        return SCPI_RES_ERR;
    }
    return SCPI_RES_OK;
}

/* ── ROUTe:SWITch#:IDENtity? ──────────────────────────────────────────────── */

scpi_result_t SCPI_RouteSwitchIdentityQ(scpi_t *context)
{
    int32_t addr = _get_switch_addr(context);

    /* Read 10 consecutive registers starting at 0x0000 */
    uint16_t regs[10];
    Modbus_Result_t mr = Modbus_ReadHoldingRegisters((uint8_t)addr, 0x0000U, 10U, regs);
    if (mr.status != MODBUS_OK)
    {
        _write(context, "ERROR: Read info failed (%s)", Modbus_StatusString(mr.status));
        return SCPI_RES_ERR;
    }

    /* regs[0]=channel, [1]=mode, [2]=dev_id, [3-4]=serial, [5-8]=name, [9]=status */
    char name[9] = {0};
    name[0] = (char)(regs[5] >> 8);
    name[1] = (char)(regs[5] & 0xFF);
    name[2] = (char)(regs[6] >> 8);
    name[3] = (char)(regs[6] & 0xFF);
    name[4] = (char)(regs[7] >> 8);
    name[5] = (char)(regs[7] & 0xFF);
    name[6] = (char)(regs[8] >> 8);
    name[7] = (char)(regs[8] & 0xFF);
    name[8] = '\0';

    uint32_t serial = ((uint32_t)regs[3] << 16) | regs[4];

    _write(context,
        "\"%s\",%lu,%u,%u,%u,%u",
        name, (unsigned long)serial,
        (unsigned)regs[2],  /* Modbus ID */
        (unsigned)regs[0],  /* Channel */
        (unsigned)regs[1],  /* Mode */
        (unsigned)regs[9]); /* Status */
    return SCPI_RES_OK;
}

/* ── ROUTe:SWITch#:OUTPut? / OUTPut ───────────────────────────────────────── */

scpi_result_t SCPI_RouteSwitchOutputQ(scpi_t *context)
{
    int32_t addr = _get_switch_addr(context);
    uint8_t coils[1] = {0};

    Modbus_Result_t mr = Modbus_ReadCoils((uint8_t)addr, 0x0000U, 6U, coils);
    if (mr.status != MODBUS_OK)
    {
        _write(context, "ERROR: Read coils failed (%s)", Modbus_StatusString(mr.status));
        return SCPI_RES_ERR;
    }

    for (int i = 0; i < 6; i++)
    {
        _write(context, "%u", (unsigned)((coils[0] >> i) & 1U));
        if (i < 5) SCPI_Write(context, ",", 1);
    }
    return SCPI_RES_OK;
}

scpi_result_t SCPI_RouteSwitchOutput(scpi_t *context)
{
    int32_t addr = _get_switch_addr(context);

    uint32_t coil_id;
    if (!SCPI_ParamUInt32(context, &coil_id, TRUE))
        return SCPI_RES_ERR;
    if (coil_id > 5U)
    {
        _write(context, "ERROR: Coil ID out of range (0-5)");
        return SCPI_RES_ERR;
    }

    uint32_t state;
    if (!SCPI_ParamUInt32(context, &state, TRUE))
        return SCPI_RES_ERR;

    Modbus_Result_t mr = Modbus_WriteSingleCoil((uint8_t)addr,
        (uint16_t)coil_id, (uint8_t)(state ? 1U : 0U));
    if (mr.status != MODBUS_OK)
    {
        _write(context, "ERROR: Write coil failed (%s)", Modbus_StatusString(mr.status));
        return SCPI_RES_ERR;
    }
    return SCPI_RES_OK;
}

/* ── ROUTe:SWITch#:INPut? ─────────────────────────────────────────────────── */

scpi_result_t SCPI_RouteSwitchInputQ(scpi_t *context)
{
    int32_t addr = _get_switch_addr(context);
    uint8_t inputs[1] = {0};

    Modbus_Result_t mr = Modbus_ReadDiscreteInputs((uint8_t)addr, 0x0000U, 4U, inputs);
    if (mr.status != MODBUS_OK)
    {
        _write(context, "ERROR: Read inputs failed (%s)", Modbus_StatusString(mr.status));
        return SCPI_RES_ERR;
    }

    for (int i = 0; i < 4; i++)
    {
        _write(context, "%u", (unsigned)((inputs[0] >> i) & 1U));
        if (i < 3) SCPI_Write(context, ",", 1);
    }
    return SCPI_RES_OK;
}

/* ── ROUTe:SWITch#:CONDition? ─────────────────────────────────────────────── */

scpi_result_t SCPI_RouteSwitchConditionQ(scpi_t *context)
{
    int32_t addr = _get_switch_addr(context);
    uint16_t status_reg = 0U;

    Modbus_Result_t mr = Modbus_ReadHoldingRegisters((uint8_t)addr, 0x0020U, 1U, &status_reg);
    if (mr.status != MODBUS_OK)
    {
        _write(context, "ERROR: Read status failed (%s)", Modbus_StatusString(mr.status));
        return SCPI_RES_ERR;
    }

    SCPI_ResultUInt32(context, status_reg);
    return SCPI_RES_OK;
}

/* ── ROUTe:SWITch#:ADDRess ────────────────────────────────────────────────── */

scpi_result_t SCPI_RouteSwitchAddress(scpi_t *context)
{
    int32_t addr = _get_switch_addr(context);

    uint32_t new_id;
    if (!SCPI_ParamUInt32(context, &new_id, TRUE))
        return SCPI_RES_ERR;
    if (new_id < 1U || new_id > 247U)
    {
        _write(context, "ERROR: Address out of range (1-247)");
        return SCPI_RES_ERR;
    }

    Modbus_Result_t mr = Modbus_WriteSingleRegister((uint8_t)addr, 0x0002U, (uint16_t)new_id);
    if (mr.status != MODBUS_OK)
    {
        _write(context, "ERROR: Write address failed (%s)", Modbus_StatusString(mr.status));
        return SCPI_RES_ERR;
    }
    _write(context, "OK (new address: %lu)", (unsigned long)new_id);
    return SCPI_RES_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  STATus subsystem                                                           */
/* ═══════════════════════════════════════════════════════════════════════════ */

scpi_result_t SCPI_StatusOperationEventQ(scpi_t *context)
{
    SCPI_ResultUInt32(context, 0U);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_StatusQuestionableEventQ(scpi_t *context)
{
    SCPI_ResultUInt32(context, 0U);
    return SCPI_RES_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  DIAGnostic subsystem                                                       */
/* ═══════════════════════════════════════════════════════════════════════════ */

static scpi_choice_def_t _on_off_choice[] = {
    {"ON",  1},
    {"OFF", 0},
    {"1",   1},
    {"0",   0},
    SCPI_CHOICE_LIST_END
};

scpi_result_t SCPI_DiagDebug(scpi_t *context)
{
    int32_t state;
    if (!SCPI_ParamChoice(context, _on_off_choice, &state, TRUE))
        return SCPI_RES_ERR;

    Log_DbgSetEnabled(state ? 1U : 0U);
    _write(context, "Debug CLI %s", state ? "ON" : "OFF");
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DiagDebugQ(scpi_t *context)
{
    SCPI_ResultUInt32(context, Log_DbgIsEnabled() ? 1U : 0U);
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DiagEcho(scpi_t *context)
{
    int32_t state;
    if (!SCPI_ParamChoice(context, _on_off_choice, &state, TRUE))
        return SCPI_RES_ERR;

    Log_DbgSetEcho(state ? 1U : 0U);
    _write(context, "Echo %s", state ? "ON" : "OFF");
    return SCPI_RES_OK;
}

scpi_result_t SCPI_DiagEchoQ(scpi_t *context)
{
    /* Echo state is simple — no public getter, just report ON */
    _write(context, "ON");
    return SCPI_RES_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  SCPI Command Table                                                         */
/* ═══════════════════════════════════════════════════════════════════════════ */

const scpi_command_t scpi_commands[] = {
    /* ── IEEE 488.2 Mandatory ─────────────────────────────────────────────── */
    { .pattern = "*CLS",  .callback = SCPI_CoreCls,  },
    { .pattern = "*IDN?", .callback = SCPI_CoreIdnQ, },
    { .pattern = "*RST",  .callback = SCPI_CoreRst,  },
    { .pattern = "*STB?", .callback = SCPI_CoreStbQ, },
    { .pattern = "*WAI",  .callback = SCPI_CoreWai,  },
    { .pattern = "*OPC?", .callback = SCPI_CoreOpcQ, },

    /* ── SYSTem:ERRor ─────────────────────────────────────────────────────── */
    { .pattern = "SYSTem:ERRor[:NEXT]?", .callback = SCPI_SystemErrorNextQ,  },
    { .pattern = "SYSTem:ERRor:COUNt?",  .callback = SCPI_SystemErrorCountQ, },

    /* ── SYSTem:ATTenuator ─────────────────────────────────────────────────── */
    { .pattern = "SYSTem:ATTenuator:A",    .callback = SCPI_SystemAttA,  },
    { .pattern = "SYSTem:ATTenuator:A?",   .callback = SCPI_SystemAttAQ, },
    { .pattern = "SYSTem:ATTenuator:B",    .callback = SCPI_SystemAttB,  },
    { .pattern = "SYSTem:ATTenuator:B?",   .callback = SCPI_SystemAttBQ, },

    /* ── SYSTem:COMMunicate:CAN ────────────────────────────────────────────── */
    { .pattern = "SYSTem:COMMunicate:CAN:SEND",  .callback = SCPI_SystemCommCanSend,  },
    { .pattern = "SYSTem:COMMunicate:CAN:SCAN?", .callback = SCPI_SystemCommCanScanQ, },

    /* ── SYSTem:DETector# ──────────────────────────────────────────────────── */
    { .pattern = "SYSTem:DETector#:SN?",                  .callback = SCPI_SystemDetSnQ,        },
    { .pattern = "SYSTem:DETector#:VERSion?",             .callback = SCPI_SystemDetVersionQ,   },
    { .pattern = "SYSTem:DETector#:RESet",                 .callback = SCPI_SystemDetReset,      },
    { .pattern = "SYSTem:DETector#:LED",                   .callback = SCPI_SystemDetLed,        },
    { .pattern = "SYSTem:DETector#:ADDRess",               .callback = SCPI_SystemDetAddress,    },
    { .pattern = "SYSTem:DETector#:FLASh:INFO?",          .callback = SCPI_SystemDetFlashInfoQ, },
    { .pattern = "SYSTem:DETector#:FLASh:DATA?",          .callback = SCPI_SystemDetFlashDataQ, },

    /* ── SENSe:DETector# ───────────────────────────────────────────────────── */
    { .pattern = "SENSe:DETector#:CONTrol",               .callback = SCPI_SenseDetControl, },
    { .pattern = "SENSe:DETector#:STOP",                   .callback = SCPI_SenseDetStop,    },
    { .pattern = "SENSe:DETector#:TEMPerature?",          .callback = SCPI_SenseDetTempQ,   },
    { .pattern = "SENSe:DETector#:POWer?",                .callback = SCPI_SenseDetPowerQ,  },
    { .pattern = "SENSe:DETector#:BAND",                   .callback = SCPI_SenseDetBand,    },

    /* ── SOURce:DETector# ──────────────────────────────────────────────────── */
    { .pattern = "SOURce:DETector#:FREQuency",            .callback = SCPI_SourceDetFreq,  },
    { .pattern = "SOURce:DETector#:POWer",                .callback = SCPI_SourceDetPower, },

    /* ── ROUTe:DETector#:SWITch ────────────────────────────────────────────── */
    { .pattern = "ROUTe:DETector#:SWITch",                .callback = SCPI_RouteDetSwitch, },

    /* ── ROUTe:SWITch# ─────────────────────────────────────────────────────── */
    { .pattern = "ROUTe:SWITch#:CHANnel?",     .callback = SCPI_RouteSwitchChannelQ,     },
    { .pattern = "ROUTe:SWITch#:CHANnel",       .callback = SCPI_RouteSwitchChannel,      },
    { .pattern = "ROUTe:SWITch#:MODE?",         .callback = SCPI_RouteSwitchModeQ,        },
    { .pattern = "ROUTe:SWITch#:MODE",           .callback = SCPI_RouteSwitchMode,         },
    { .pattern = "ROUTe:SWITch#:IDENtity?",     .callback = SCPI_RouteSwitchIdentityQ,    },
    { .pattern = "ROUTe:SWITch#:OUTPut?",       .callback = SCPI_RouteSwitchOutputQ,      },
    { .pattern = "ROUTe:SWITch#:OUTPut",         .callback = SCPI_RouteSwitchOutput,       },
    { .pattern = "ROUTe:SWITch#:INPut?",        .callback = SCPI_RouteSwitchInputQ,       },
    { .pattern = "ROUTe:SWITch#:CONDition?",    .callback = SCPI_RouteSwitchConditionQ,   },
    { .pattern = "ROUTe:SWITch#:ADDRess",       .callback = SCPI_RouteSwitchAddress,      },

    /* ── STATus ────────────────────────────────────────────────────────────── */
    { .pattern = "STATus:OPERation:EVENt?",     .callback = SCPI_StatusOperationEventQ,     },
    { .pattern = "STATus:QUEStionable:EVENt?",  .callback = SCPI_StatusQuestionableEventQ,  },

    /* ── DIAGnostic ────────────────────────────────────────────────────────── */
    { .pattern = "DIAGnostic:DEBUg",     .callback = SCPI_DiagDebug,  },
    { .pattern = "DIAGnostic:DEBUg?",    .callback = SCPI_DiagDebugQ, },
    { .pattern = "DIAGnostic:ECHO",      .callback = SCPI_DiagEcho,   },
    { .pattern = "DIAGnostic:ECHO?",     .callback = SCPI_DiagEchoQ,  },

    SCPI_CMD_LIST_END
};

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  SCPI Context & Interface                                                   */
/* ═══════════════════════════════════════════════════════════════════════════ */

scpi_interface_t scpi_interface = {
    .error   = SCPI_Error,
    .write   = SCPI_Write,
    .control = SCPI_Control,
    .flush   = SCPI_Flush,
    .reset   = SCPI_Reset,
};

char         scpi_input_buffer[SCPI_INPUT_BUFFER_LENGTH];
scpi_error_t scpi_error_queue_data[SCPI_ERROR_QUEUE_SIZE];
scpi_t       scpi_context;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  SCPI Lifecycle                                                             */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
  * @brief  Initialize SCPI subsystem
  */
void SCPI_SystemInit(void)
{
    SCPI_Init(&scpi_context,
              scpi_commands,
              &scpi_interface,
              NULL,  /* no unit definitions */
              SCPI_IDN1, SCPI_IDN2, SCPI_IDN3, SCPI_IDN4,
              scpi_input_buffer, SCPI_INPUT_BUFFER_LENGTH,
              scpi_error_queue_data, SCPI_ERROR_QUEUE_SIZE);
}

/**
  * @brief  Try to parse a line as SCPI. Returns TRUE if recognized.
  */
scpi_bool_t SCPI_TryParse(const char *line)
{
    if (line == NULL || line[0] == '\0')
        return FALSE;

    /* Append \n terminator — SCPI_Parse requires it to dispatch */
    size_t len = strlen(line);
    if (len >= SCPI_INPUT_BUFFER_LENGTH - 2)
        len = SCPI_INPUT_BUFFER_LENGTH - 2;
    char buf[SCPI_INPUT_BUFFER_LENGTH];
    memcpy(buf, line, len);
    buf[len] = '\n';
    buf[len + 1] = '\0';

    /* Track whether SCPI matched a command — findCommandHeader() updates
       param_list.cmd when it finds a match. If the pointer doesn't change,
       SCPI didn't recognize the input and we should let the debug CLI try. */
    const scpi_command_t *prev_cmd = scpi_context.param_list.cmd;

    scpi_bool_t result = SCPI_Parse(&scpi_context, buf, (int)(len + 1));

    /* Flush the SCPI output */
    SCPI_Flush(&scpi_context);

    /* Return TRUE if the command was recognized, even if the callback
       returned an error. Only return FALSE for truly unrecognized input,
       so the debug CLI fallback doesn't pollute SCPI error responses. */
    return result || (scpi_context.param_list.cmd != prev_cmd);
}
