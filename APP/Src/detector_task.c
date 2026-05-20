/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    detector_task.c
  * @brief   A1 Detector Board CAN protocol implementation
  *
  *          All commands follow the same request/response pattern:
  *            1. Build CAN frame with CAN ID = command_id, Byte0 = node_id
  *            2. Send via CAN_Send()
  *            3. Poll for response (CAN ID matches node_id, Byte0 matches
  *               command echo)
  *            4. Parse and return structured result
  *
  *          Protocol reference: Doc/V3.x A1检波板指令.docx
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
#include "detector_task.h"
#include "can.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Private define ------------------------------------------------------------*/

/** @brief Round up a node_id timeout: minimum 100ms for basic commands */
#define DETECTOR_MIN_TIMEOUT_MS     100U

/** @brief Maximum poll interval between CAN RX checks (ms) */
#define DETECTOR_POLL_INTERVAL_MS   5U

/** @brief Broadcast node ID */
#define DETECTOR_BROADCAST          0xFFU

/** @brief Helper: extract the command echo byte from CAN ID (low byte) */
#define CMD_ECHO(id)                ((uint8_t)((id) & 0xFFU))

/* Private typedef -----------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

static App_Status_t _detector_task_init(void);
static App_Status_t _detector_task_process(void);
static void         _detector_task_on_error(App_Status_t err);

/* Debug CLI handlers */
static void _dbg_cmd_detector(int argc, char **argv);

/* Internal helpers */
static int  _hex_to_byte(const char *hex, uint8_t *out);
static void _print_hex(const char *prefix, const uint8_t *data, uint16_t len);

/**
  * @brief  Core send-and-wait-for-response helper
  * @param  can_id       CAN frame ID (command)
  * @param  tx_data      Data buffer to send
  * @param  tx_dlc       TX data length
  * @param  node_id      Target node ID (used to match response CAN ID)
  * @param  cmd_echo     Expected response Byte0 value (low byte of cmd)
  * @param  timeout_ms   Response wait timeout
  * @param  rx_data      Output buffer for received response data (can be NULL)
  * @param  rx_max       Max bytes to receive
  * @param  rx_len       Output: actual received length
  * @retval DETECTOR_OK on success
  * @retval DETECTOR_ERR_* on failure
  */
static Detector_Status_t _send_and_wait(uint32_t can_id,
                                         const uint8_t *tx_data, uint8_t tx_dlc,
                                         uint8_t node_id, uint8_t cmd_echo,
                                         uint32_t timeout_ms,
                                         uint8_t *rx_data, uint8_t rx_max,
                                         uint8_t *rx_len);

/**
  * @brief  Wait for a specific response frame
  * @param  node_id      Expected response CAN ID (or accept any if broadcast)
  * @param  cmd_echo     Expected response Byte0
  * @param  timeout_ms   Timeout
  * @param  rx_out       Output CAN message
  * @retval 1 if response received, 0 if timeout
  */
static int _wait_response(uint8_t node_id, uint8_t cmd_echo,
                          uint32_t timeout_ms, CAN_Msg_t *rx_out);

/* Exported variables --------------------------------------------------------*/

const App_Module_t g_detector_task_module = {
    .name     = "Detector",
    .init     = _detector_task_init,
    .process  = _detector_task_process,
    .on_error = _detector_task_on_error,
};

/* Exported functions --------------------------------------------------------*/

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Init / Process                                                              */
/* ─────────────────────────────────────────────────────────────────────────── */

static App_Status_t _detector_task_init(void)
{
    if (Log_RegisterDbgCmd("detector", _dbg_cmd_detector) != HAL_OK)
    {
        LOG_ERROR("Detector: failed to register 'detector' debug command");
        return APP_ERROR;
    }

    LOG_INFO("Detector: initialized, type 'detector help' for usage");
    return APP_OK;
}

static App_Status_t _detector_task_process(void)
{
    return APP_OK;
}

static void _detector_task_on_error(App_Status_t err)
{
    LOG_ERROR("Detector: error (status=%d)", (int)err);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  System Commands                                                             */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  0x101 Read device serial number
  *         Request:  ID=0x101, D[0]=node_id
  *         Response: ID=nodeId, D[0]=0x01, D[1]=0x00, D[2]=sn_len, D[3+]=SN chars
  */
Detector_Status_t Detector_ReadSN(uint8_t node_id, Detector_SN_t *result)
{
    uint8_t tx[1] = { node_id };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x101U, tx, 1U, node_id, CMD_ECHO(0x101U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    /* rx[0]=0x01, rx[1]=result, rx[2]=sn_len, rx[3+]=SN data */
    if (rx_len < 3U)
    {
        return DETECTOR_ERR_RESP;
    }

    if (rx[1] != 0x00U)
    {
        return (rx[1] == 0x01U) ? DETECTOR_ERR_FAIL : rx[1];
    }

    uint8_t sn_len = rx[2];
    if (sn_len > DETECTOR_SN_MAX_LEN)
    {
        sn_len = DETECTOR_SN_MAX_LEN;
    }

    if (result != NULL)
    {
        memcpy(result->sn, &rx[3], sn_len);
        result->sn[sn_len] = '\0';
        result->sn_len = sn_len;
    }

    return DETECTOR_OK;
}

/**
  * @brief  0x102 Write Node ID
  *         Request:  ID=0x102, D[0]=node_id, D[1]=reset_flag, D[2]=new_id
  *         Response: ID=nodeId, D[0]=0x02, D[1]=result, D[2]=current_id
  */
Detector_Status_t Detector_WriteNodeId(uint8_t node_id, uint8_t reset_flag,
                                        uint8_t new_id, Detector_NodeId_t *result)
{
    uint8_t tx[3] = { node_id, reset_flag ? 1U : 0U, new_id };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x102U, tx, 3U, node_id, CMD_ECHO(0x102U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 3U)
    {
        return DETECTOR_ERR_RESP;
    }

    if (rx[1] != 0x00U)
    {
        if (result != NULL)
        {
            result->node_id = rx[2];
        }
        return (rx[1] == 0x01U) ? DETECTOR_ERR_FAIL : rx[1];
    }

    if (result != NULL)
    {
        result->node_id = rx[2];
    }

    return DETECTOR_OK;
}

/**
  * @brief  0x103 Read firmware version
  *         Request:  ID=0x103, D[0]=node_id
  *         Response: ID=nodeId, D[0]=0x03, D[1]=0x00, D[2-4]=major.minor.patch
  */
Detector_Status_t Detector_ReadVersion(uint8_t node_id, Detector_Version_t *result)
{
    uint8_t tx[1] = { node_id };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x103U, tx, 1U, node_id, CMD_ECHO(0x103U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 5U || rx[1] != 0x00U)
    {
        return (rx_len >= 2U && rx[1] != 0x00U) ? DETECTOR_ERR_FAIL : DETECTOR_ERR_RESP;
    }

    if (result != NULL)
    {
        result->major = rx[2];
        result->minor = rx[3];
        result->patch = rx[4];
    }

    return DETECTOR_OK;
}

/**
  * @brief  0x104 Reset MCU
  *         Request:  ID=0x104, D[0]=node_id
  *         Response: ID=nodeId, D[0]=0x04, D[1]=0x00 (ack before reset)
  */
Detector_Status_t Detector_Reset(uint8_t node_id)
{
    uint8_t tx[1] = { node_id };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    return _send_and_wait(
        0x104U, tx, 1U, node_id, CMD_ECHO(0x104U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);
}

/**
  * @brief  0x105 LED control
  *         Request:  ID=0x105, D[0]=node_id, D[1]=0/1, D[2-3]=period_ms
  *         Response: ID=nodeId, D[0]=0x05, D[1]=0x00
  */
Detector_Status_t Detector_LEDControl(uint8_t node_id, uint8_t enable,
                                       uint16_t period_ms)
{
    uint8_t tx[4] = {
        node_id,
        enable ? 1U : 0U,
        (uint8_t)(period_ms & 0xFFU),
        (uint8_t)((period_ms >> 8) & 0xFFU)
    };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    return _send_and_wait(
        0x105U, tx, 4U, node_id, CMD_ECHO(0x105U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);
}

/**
  * @brief  0x106 Write SN string
  *         Request:  ID=0x106, D[0]=node_id, D[1]=sn_len, D[2+]=SN chars
  *         Response: ID=nodeId, D[0]=0x06, D[1]=result, D[2]=written_len, D[3+]=SN
  */
Detector_Status_t Detector_WriteSN(uint8_t node_id, const char *sn,
                                    uint8_t sn_len, uint8_t *written_len)
{
    if (sn == NULL || sn_len < 1U || sn_len > DETECTOR_SN_MAX_LEN)
    {
        return DETECTOR_ERR_PARAM;
    }

    uint8_t tx[1U + 1U + DETECTOR_SN_MAX_LEN];
    tx[0] = node_id;
    tx[1] = sn_len;
    memcpy(&tx[2], sn, sn_len);

    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x106U, tx, (uint8_t)(2U + sn_len), node_id, CMD_ECHO(0x106U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 3U)
    {
        return DETECTOR_ERR_RESP;
    }

    if (written_len != NULL)
    {
        *written_len = rx[2];
    }

    if (rx[1] != 0x00U)
    {
        return rx[1];  /* Return device error code directly */
    }

    return DETECTOR_OK;
}

/**
  * @brief  0x107 Enter SN write state
  *         Request:  ID=0x107, D[0]=node_id, D[1-3]="gts"
  */
Detector_Status_t Detector_EnterSNWrite(uint8_t node_id)
{
    uint8_t tx[4] = { node_id, 'g', 't', 's' };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    return _send_and_wait(
        0x107U, tx, 4U, node_id, CMD_ECHO(0x107U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);
}

/**
  * @brief  0x108 Exit SN write state
  *         Request:  ID=0x108, D[0]=node_id, D[1-4]="exit"
  */
Detector_Status_t Detector_ExitSNWrite(uint8_t node_id)
{
    uint8_t tx[5] = { node_id, 'e', 'x', 'i', 't' };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    return _send_and_wait(
        0x108U, tx, 5U, node_id, CMD_ECHO(0x108U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Detector Operation Commands                                                 */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  0x110 Detector measurement control
  *         Request:  ID=0x110, D[0]=node_id, D[1]=channel, D[2]=hold_ms,
  *                   D[3]=mode, D[4-5]=threshold, D[6-7]=gate_ms
  *         Response: ID=nodeId, D[0]=0x10, D[1]=result, D[2-3]=H_avg,
  *                   D[4-5]=V_avg, D[6]=mode_echo, D[7]=state
  */
Detector_Status_t Detector_Control(uint8_t node_id, Detector_Mode_t mode,
                                    uint8_t hold_ms, uint16_t threshold,
                                    uint16_t gate_ms,
                                    Detector_ControlResult_t *result)
{
    uint8_t tx[8] = {
        node_id,
        1U,  /* Channel — default V channel for basic control; use 0x114 for H */
        hold_ms,
        (uint8_t)mode,
        (uint8_t)(threshold & 0xFFU),
        (uint8_t)((threshold >> 8) & 0xFFU),
        (uint8_t)(gate_ms & 0xFFU),
        (uint8_t)((gate_ms >> 8) & 0xFFU)
    };

    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x110U, tx, 8U, node_id, CMD_ECHO(0x110U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 8U)
    {
        return DETECTOR_ERR_RESP;
    }

    if (rx[1] != 0x00U)
    {
        return DETECTOR_ERR_FAIL;
    }

    if (result != NULL)
    {
        result->h_adc_avg = (int16_t)((uint16_t)rx[2] | ((uint16_t)rx[3] << 8));
        result->v_adc_avg = (int16_t)((uint16_t)rx[4] | ((uint16_t)rx[5] << 8));
        result->mode      = rx[6];
        result->state     = rx[7];
    }

    return DETECTOR_OK;
}

/**
  * @brief  0x111 Switch control
  *         Request:  ID=0x111, D[0]=node_id, D[1-6]=SW1-SW6 (0/1, >=2 = keep)
  *         Response: ID=nodeId, D[0]=0x11, D[1]=0x00
  */
Detector_Status_t Detector_SwitchControl(uint8_t node_id,
                                          uint8_t sw1, uint8_t sw2,
                                          uint8_t sw3, uint8_t sw4,
                                          uint8_t sw5, uint8_t sw6)
{
    uint8_t tx[7] = { node_id, sw1, sw2, sw3, sw4, sw5, sw6 };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    return _send_and_wait(
        0x111U, tx, 7U, node_id, CMD_ECHO(0x111U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);
}

/**
  * @brief  0x112 Stop detection
  *         Request:  ID=0x112, D[0]=node_id
  *         Response: ID=nodeId, D[0]=0x12, D[1]=0x00 (ok) or 0x01 (not measuring)
  */
Detector_Status_t Detector_Stop(uint8_t node_id)
{
    uint8_t tx[1] = { node_id };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x112U, tx, 1U, node_id, CMD_ECHO(0x112U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 2U)
    {
        return DETECTOR_ERR_RESP;
    }

    return (rx[1] == 0x00U) ? DETECTOR_OK : DETECTOR_OK;  /* 0x01 = already idle, still OK */
}

/**
  * @brief  0x113 Attenuator manual setting
  *         Request:  ID=0x113, D[0]=node_id, D[1-7]=ATT1-ATT7
  *         Response: ID=nodeId, D[0]=0x13, D[1]=0x00
  */
Detector_Status_t Detector_SetAttenuator(uint8_t node_id,
                                          const Detector_Attenuator_t *att)
{
    if (att == NULL)
    {
        return DETECTOR_ERR_PARAM;
    }

    uint8_t tx[8] = {
        node_id, att->att1, att->att2, att->att3,
        att->att4, att->att5, att->att6, att->att7
    };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    return _send_and_wait(
        0x113U, tx, 8U, node_id, CMD_ECHO(0x113U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);
}

/**
  * @brief  0x114 VCO frequency setting
  *         Request:  ID=0x114, D[0]=node_id, D[1]=channel, D[2-5]=freq_khz,
  *                   D[6]=lmx_power, D[7]=0
  *         Response: ID=nodeId, D[0]=0x14, D[1]=result, D[2]=channel,
  *                   D[3-6]=actual_freq_khz, D[7]=lmx_power_echo
  */
Detector_Status_t Detector_SetFrequency(uint8_t node_id,
                                         Detector_Channel_t channel,
                                         uint32_t freq_khz,
                                         uint8_t lmx_power,
                                         Detector_FreqResult_t *result)
{
    uint8_t tx[8] = {
        node_id,
        (uint8_t)channel,
        (uint8_t)(freq_khz & 0xFFU),
        (uint8_t)((freq_khz >> 8) & 0xFFU),
        (uint8_t)((freq_khz >> 16) & 0xFFU),
        (uint8_t)((freq_khz >> 24) & 0xFFU),
        lmx_power,
        0x00U
    };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x114U, tx, 8U, node_id, CMD_ECHO(0x114U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 8U)
    {
        return DETECTOR_ERR_RESP;
    }

    if (rx[1] != 0x00U)
    {
        return DETECTOR_ERR_FAIL;
    }

    if (result != NULL)
    {
        result->channel   = rx[2];
        result->freq_hz   = ((uint32_t)rx[3])
                          | ((uint32_t)rx[4] << 8)
                          | ((uint32_t)rx[5] << 16)
                          | ((uint32_t)rx[6] << 24);
        result->lmx_power = rx[7];
    }

    return DETECTOR_OK;
}

/**
  * @brief  0x115 Band selection
  *         Request:  ID=0x115, D[0]=node_id, D[1]=band_mhz, D[2]=mode,
  *                   D[3-7]=40-bit bitmask (LSB first, D[3]=bits 0-7)
  *         Response: ID=nodeId, D[0]=0x15, D[1]=0x00, D[2]=mode_echo,
  *                   D[3]=v_status, D[4]=h_status
  */
Detector_Status_t Detector_SelectBand(uint8_t node_id, uint8_t band_mhz,
                                       uint8_t band_mode, uint64_t bitmask,
                                       Detector_BandResult_t *result)
{
    uint8_t tx[8] = {
        node_id,
        band_mhz,
        band_mode,
        (uint8_t)(bitmask & 0xFFU),
        (uint8_t)((bitmask >> 8) & 0xFFU),
        (uint8_t)((bitmask >> 16) & 0xFFU),
        (uint8_t)((bitmask >> 24) & 0xFFU),
        (uint8_t)((bitmask >> 32) & 0xFFU)
    };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x115U, tx, 8U, node_id, CMD_ECHO(0x115U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 5U)
    {
        return DETECTOR_ERR_RESP;
    }

    if (result != NULL)
    {
        result->mode       = rx[2];
        result->v_selected = rx[3];
        result->h_selected = rx[4];
    }

    return DETECTOR_OK;
}

/**
  * @brief  0x116 Read temperature
  *         Request:  ID=0x116, D[0]=node_id, D[1]=read_detector, D[2]=read_mcu
  *         Response: ID=nodeId, D[0]=0x16, D[1]=result, D[2-3]=detector_temp,
  *                   D[4-5]=mcu_temp, D[6-7]=reserved
  */
Detector_Status_t Detector_ReadTemperature(uint8_t node_id,
                                            uint8_t read_detector,
                                            uint8_t read_mcu,
                                            Detector_Temp_t *result)
{
    uint8_t tx[3] = { node_id, read_detector ? 1U : 0U, read_mcu ? 1U : 0U };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x116U, tx, 3U, node_id, CMD_ECHO(0x116U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 8U)
    {
        return DETECTOR_ERR_RESP;
    }

    if (rx[1] != 0x00U)
    {
        return DETECTOR_ERR_FAIL;
    }

    if (result != NULL)
    {
        result->detector_temp = (int16_t)((uint16_t)rx[2] | ((uint16_t)rx[3] << 8));
        result->mcu_temp      = (int16_t)((uint16_t)rx[4] | ((uint16_t)rx[5] << 8));
        result->reserved      = (int16_t)((uint16_t)rx[6] | ((uint16_t)rx[7] << 8));
    }

    return DETECTOR_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Power Commands                                                              */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  0x11B Query detection power (12-byte CAN FD frame)
  */
Detector_Status_t Detector_QueryPower(uint8_t node_id, uint32_t freq_khz,
                                       uint8_t hold_ms, Detector_Mode_t mode,
                                       uint16_t threshold, uint16_t gate_ms,
                                       uint8_t compensate,
                                       Detector_Power_t *result)
{
    uint8_t tx[12] = {
        node_id,
        (uint8_t)(freq_khz & 0xFFU),
        (uint8_t)((freq_khz >> 8) & 0xFFU),
        (uint8_t)((freq_khz >> 16) & 0xFFU),
        hold_ms,
        (uint8_t)mode,
        (uint8_t)(threshold & 0xFFU),
        (uint8_t)((threshold >> 8) & 0xFFU),
        (uint8_t)(gate_ms & 0xFFU),
        (uint8_t)((gate_ms >> 8) & 0xFFU),
        (compensate ? 1U : 0U),
        0x00U  /* Reserved */
    };

    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x11BU, tx, 12U, node_id, CMD_ECHO(0x11BU),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 8U)
    {
        return DETECTOR_ERR_RESP;
    }

    if (rx[1] != 0x00U)
    {
        return DETECTOR_ERR_FAIL;
    }

    if (result != NULL)
    {
        result->h_power_dbm100 = (int16_t)((uint16_t)rx[2] | ((uint16_t)rx[3] << 8));
        result->v_power_dbm100 = (int16_t)((uint16_t)rx[4] | ((uint16_t)rx[5] << 8));
        result->mode           = rx[6];
        result->state          = rx[7];
    }

    return DETECTOR_OK;
}

/**
  * @brief  0x11C Set transmit power (12-byte CAN FD frame)
  */
Detector_Status_t Detector_SetTxPower(uint8_t node_id,
                                       Detector_Channel_t channel,
                                       uint32_t freq_khz,
                                       int16_t target_power_dbm100,
                                       uint8_t gps_flag,
                                       uint8_t compensate,
                                       Detector_TxPowerResult_t *result)
{
    uint8_t tx[12] = {
        node_id,
        (uint8_t)channel,
        (uint8_t)(freq_khz & 0xFFU),
        (uint8_t)((freq_khz >> 8) & 0xFFU),
        (uint8_t)((freq_khz >> 16) & 0xFFU),
        (uint8_t)((freq_khz >> 24) & 0xFFU),
        (uint8_t)(target_power_dbm100 & 0xFFU),
        (uint8_t)((target_power_dbm100 >> 8) & 0xFFU),
        gps_flag ? 1U : 0U,
        (compensate ? 1U : 0U),
        0x00U,
        0x00U
    };

    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x11CU, tx, 12U, node_id, CMD_ECHO(0x11CU),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 16U)
    {
        return DETECTOR_ERR_RESP;
    }

    if (rx[1] != 0x00U)
    {
        return DETECTOR_ERR_FAIL;
    }

    if (result != NULL)
    {
        result->channel = rx[2];
        result->freq_hz = ((uint32_t)rx[3])
                        | ((uint32_t)rx[4] << 8)
                        | ((uint32_t)rx[5] << 16)
                        | ((uint32_t)rx[6] << 24);
        result->att1 = rx[7];
        result->att2 = rx[8];
        result->att3 = rx[9];
        result->att4 = rx[10];
        result->att5 = rx[11];
        result->att6 = rx[12];
        result->att7 = rx[13];
        result->predicted_power_dbm100 = (int16_t)((uint16_t)rx[14] | ((uint16_t)rx[15] << 8));
    }

    return DETECTOR_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Flash Commands                                                              */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  0x117 Query external flash layout (returns 48-byte response)
  */
Detector_Status_t Detector_GetFlashInfo(uint8_t node_id,
                                         Detector_FlashInfo_t *result)
{
    uint8_t tx[1] = { node_id };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x117U, tx, 1U, node_id, CMD_ECHO(0x117U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 48U)
    {
        return DETECTOR_ERR_RESP;
    }

    if (rx[1] != 0x00U)
    {
        return DETECTOR_ERR_FAIL;
    }

    if (result != NULL)
    {
        result->jedec_id          = ((uint32_t)rx[2]) | ((uint32_t)rx[3] << 8) | ((uint32_t)rx[4] << 16);
        result->status_reg1       = rx[5];
        result->ota_img_addr      = ((uint32_t)rx[6])  | ((uint32_t)rx[7] << 8)  | ((uint32_t)rx[8] << 16);
        result->ota_img_size      = ((uint32_t)rx[9])  | ((uint32_t)rx[10] << 8) | ((uint32_t)rx[11] << 16);
        result->cal_h_coarse_addr = ((uint32_t)rx[12]) | ((uint32_t)rx[13] << 8) | ((uint32_t)rx[14] << 16);
        result->cal_h_coarse_size = ((uint32_t)rx[15]) | ((uint32_t)rx[16] << 8) | ((uint32_t)rx[17] << 16);
        result->cal_h_fine_addr   = ((uint32_t)rx[18]) | ((uint32_t)rx[19] << 8) | ((uint32_t)rx[20] << 16);
        result->cal_h_fine_size   = ((uint32_t)rx[21]) | ((uint32_t)rx[22] << 8) | ((uint32_t)rx[23] << 16);
        result->cal_v_coarse_addr = ((uint32_t)rx[24]) | ((uint32_t)rx[25] << 8) | ((uint32_t)rx[26] << 16);
        result->cal_v_coarse_size = ((uint32_t)rx[27]) | ((uint32_t)rx[28] << 8) | ((uint32_t)rx[29] << 16);
        result->cal_ext_addr      = ((uint32_t)rx[30]) | ((uint32_t)rx[31] << 8) | ((uint32_t)rx[32] << 16);
        result->cal_ext_size      = ((uint32_t)rx[33]) | ((uint32_t)rx[34] << 8) | ((uint32_t)rx[35] << 16);
        result->reserved_addr     = ((uint32_t)rx[36]) | ((uint32_t)rx[37] << 8) | ((uint32_t)rx[38] << 16);
        result->reserved_size     = ((uint32_t)rx[39]) | ((uint32_t)rx[40] << 8) | ((uint32_t)rx[41] << 16);
        result->cal_v_fine_addr   = ((uint32_t)rx[42]) | ((uint32_t)rx[43] << 8) | ((uint32_t)rx[44] << 16);
        result->cal_v_fine_size   = ((uint32_t)rx[45]) | ((uint32_t)rx[46] << 8) | ((uint32_t)rx[47] << 16);
    }

    return DETECTOR_OK;
}

/**
  * @brief  0x118 Erase external flash sector (4KB-aligned)
  */
Detector_Status_t Detector_EraseFlash(uint8_t node_id, uint32_t addr,
                                       uint32_t len,
                                       Detector_FlashOpResult_t *result)
{
    uint8_t tx[8] = {
        node_id,
        (uint8_t)(addr & 0xFFU),
        (uint8_t)((addr >> 8) & 0xFFU),
        (uint8_t)((addr >> 16) & 0xFFU),
        (uint8_t)(len & 0xFFU),
        (uint8_t)((len >> 8) & 0xFFU),
        (uint8_t)((len >> 16) & 0xFFU),
        0x00U
    };

    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x118U, tx, 8U, node_id, CMD_ECHO(0x118U),
        DETECTOR_TIMEOUT_FLASH_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 7U)
    {
        return DETECTOR_ERR_RESP;
    }

    if (rx[1] != 0x00U)
    {
        return rx[1];
    }

    if (result != NULL)
    {
        result->addr   = ((uint32_t)rx[2]) | ((uint32_t)rx[3] << 8) | ((uint32_t)rx[4] << 16);
        result->length = ((uint32_t)rx[5]) | ((uint32_t)rx[6] << 8) | ((uint32_t)rx[7] << 16);
    }

    return DETECTOR_OK;
}

/**
  * @brief  0x119 Write external flash data
  */
Detector_Status_t Detector_WriteFlash(uint8_t node_id, uint32_t addr,
                                       const uint8_t *data, uint16_t data_len,
                                       Detector_FlashOpResult_t *result)
{
    if (data == NULL || data_len == 0U)
    {
        return DETECTOR_ERR_PARAM;
    }

    /* Build frame: D[0]=node_id, D[1-3]=addr, D[4+]=data
     * CAN FD supports up to 60 data bytes after the 4-byte header */
    uint8_t tx[64];
    tx[0] = node_id;
    tx[1] = (uint8_t)(addr & 0xFFU);
    tx[2] = (uint8_t)((addr >> 8) & 0xFFU);
    tx[3] = (uint8_t)((addr >> 16) & 0xFFU);

    uint16_t copy_len = data_len;
    if (copy_len > (uint16_t)(sizeof(tx) - 4U))
    {
        copy_len = (uint16_t)(sizeof(tx) - 4U);
    }
    memcpy(&tx[4], data, copy_len);

    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x119U, tx, (uint8_t)(4U + copy_len), node_id, CMD_ECHO(0x119U),
        DETECTOR_TIMEOUT_FLASH_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 7U)
    {
        return DETECTOR_ERR_RESP;
    }

    if (rx[1] != 0x00U)
    {
        return rx[1];
    }

    if (result != NULL)
    {
        result->addr   = ((uint32_t)rx[2]) | ((uint32_t)rx[3] << 8) | ((uint32_t)rx[4] << 16);
        result->length = rx[5];  /* Bytes written this frame */
    }

    return DETECTOR_OK;
}

/**
  * @brief  0x11A Read external flash data (1-58 bytes)
  */
Detector_Status_t Detector_ReadFlash(uint8_t node_id, uint32_t addr,
                                      uint8_t read_len,
                                      Detector_FlashRead_t *result)
{
    if (read_len < 1U || read_len > DETECTOR_FLASH_READ_MAX)
    {
        return DETECTOR_ERR_PARAM;
    }

    uint8_t tx[4] = {
        node_id,
        (uint8_t)(addr & 0xFFU),
        (uint8_t)((addr >> 8) & 0xFFU),
        (uint8_t)((addr >> 16) & 0xFFU),
    };

    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x11AU, tx, 4U, node_id, CMD_ECHO(0x11AU),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 6U)
    {
        return DETECTOR_ERR_RESP;
    }

    if (rx[1] != 0x00U)
    {
        return rx[1];
    }

    if (result != NULL)
    {
        result->addr = ((uint32_t)rx[2]) | ((uint32_t)rx[3] << 8) | ((uint32_t)rx[4] << 16);
        result->data_len = rx[5];
        if (result->data_len > DETECTOR_FLASH_READ_MAX)
        {
            result->data_len = DETECTOR_FLASH_READ_MAX;
        }
        if (rx_len >= (uint8_t)(6U + result->data_len))
        {
            memcpy(result->data, &rx[6], result->data_len);
        }
    }

    return DETECTOR_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Calibration Commands                                                        */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  0x120 Enter calibration data write mode
  */
Detector_Status_t Detector_EnterCalibration(uint8_t node_id)
{
    uint8_t tx[1] = { node_id };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    return _send_and_wait(
        0x120U, tx, 1U, node_id, CMD_ECHO(0x120U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);
}

/**
  * @brief  0x121 Exit calibration data write mode
  */
Detector_Status_t Detector_ExitCalibration(uint8_t node_id)
{
    uint8_t tx[1] = { node_id };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    return _send_and_wait(
        0x121U, tx, 1U, node_id, CMD_ECHO(0x121U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  OTA Commands                                                                */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  0x1A0 Prepare OTA update
  */
Detector_Status_t Detector_OTAPrepare(uint8_t node_id, uint8_t ver_major,
                                       uint8_t ver_minor, uint8_t ver_patch,
                                       uint8_t *status_code)
{
    uint8_t tx[4] = { node_id, ver_major, ver_minor, ver_patch };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x1A0U, tx, 4U, node_id, CMD_ECHO(0x1A0U),
        DETECTOR_TIMEOUT_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 2U)
    {
        return DETECTOR_ERR_RESP;
    }

    if (status_code != NULL)
    {
        *status_code = rx[1];
    }

    if (rx[1] != 0x00U)
    {
        return rx[1];
    }

    return DETECTOR_OK;
}

/**
  * @brief  0x1A1 Begin OTA page write
  */
Detector_Status_t Detector_OTABegin(uint8_t node_id, uint16_t page_num)
{
    uint8_t tx[4] = {
        node_id,
        0x00U,
        (uint8_t)(page_num & 0xFFU),
        (uint8_t)((page_num >> 8) & 0xFFU)
    };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x1A1U, tx, 4U, node_id, CMD_ECHO(0x1A1U),
        DETECTOR_TIMEOUT_OTA_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 2U)
    {
        return DETECTOR_ERR_RESP;
    }

    return (rx[1] == 0x00U) ? DETECTOR_OK : DETECTOR_ERR_FAIL;
}

/**
  * @brief  Send OTA data block (CAN ID = 0x300 + block_num)
  *         Response: CAN ID = 0x400 + node_id, D[0]=block_num, D[1]=result
  */
Detector_Status_t Detector_OTAData(uint8_t node_id, uint8_t block_num,
                                    const uint8_t *data, uint8_t data_len)
{
    if (data == NULL || data_len == 0U)
    {
        return DETECTOR_ERR_PARAM;
    }

    CAN_Msg_t tx_msg;
    tx_msg.id  = 0x300U + (uint32_t)block_num;
    tx_msg.dlc = data_len;
    memcpy(tx_msg.data, data, data_len);

    if (CAN_Send(&tx_msg) != HAL_OK)
    {
        return DETECTOR_ERR_SEND;
    }

    /* Wait for ACK: CAN ID = 0x400 + node_id, D[0]=block_num, D[1]=0x00 */
    CAN_Msg_t rx_msg;
    uint32_t tick = HAL_GetTick();
    while ((HAL_GetTick() - tick) < DETECTOR_TIMEOUT_OTA_MS)
    {
        if (CAN_GetRxMessage(&rx_msg) == HAL_OK)
        {
            if (rx_msg.id == (0x400U + (uint32_t)node_id) &&
                rx_msg.dlc >= 2U &&
                rx_msg.data[0] == block_num)
            {
                return (rx_msg.data[1] == 0x00U) ? DETECTOR_OK : DETECTOR_ERR_FAIL;
            }
        }
    }

    return DETECTOR_ERR_TIMEOUT;
}

/**
  * @brief  0x1A3 Complete OTA update
  */
Detector_Status_t Detector_OTAComplete(uint8_t node_id, uint8_t reboot,
                                        uint16_t crc, uint32_t total_len)
{
    uint8_t tx[8] = {
        node_id,
        reboot ? 1U : 0U,
        (uint8_t)(crc & 0xFFU),
        (uint8_t)((crc >> 8) & 0xFFU),
        (uint8_t)(total_len & 0xFFU),
        (uint8_t)((total_len >> 8) & 0xFFU),
        (uint8_t)((total_len >> 16) & 0xFFU),
        (uint8_t)((total_len >> 24) & 0xFFU)
    };
    uint8_t rx[64];
    uint8_t rx_len = 0;

    Detector_Status_t status = _send_and_wait(
        0x1A3U, tx, 8U, node_id, CMD_ECHO(0x1A3U),
        DETECTOR_TIMEOUT_OTA_MS, rx, sizeof(rx), &rx_len);

    if (status != DETECTOR_OK)
    {
        return status;
    }

    if (rx_len < 2U)
    {
        return DETECTOR_ERR_RESP;
    }

    return (rx[1] == 0x00U) ? DETECTOR_OK : DETECTOR_ERR_FAIL;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Utility                                                                     */
/* ─────────────────────────────────────────────────────────────────────────── */

const char *Detector_StatusString(Detector_Status_t status)
{
    switch (status)
    {
        case DETECTOR_OK:           return "OK";
        case DETECTOR_ERR_FAIL:     return "Device Fail";
        case DETECTOR_ERR_PARAM:    return "Invalid Parameter";
        case DETECTOR_ERR_TIMEOUT:  return "Timeout";
        case DETECTOR_ERR_BUSY:     return "Busy (OTA/Cal/SN-write)";
        case DETECTOR_ERR_STATE:    return "Wrong State";
        case DETECTOR_ERR_SEND:     return "CAN Send Failed";
        case DETECTOR_ERR_RESP:     return "Malformed Response";
        default:                    return "Unknown";
    }
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Internal Helpers                                                            */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  Core send-and-wait-for-response
  */
static Detector_Status_t _send_and_wait(uint32_t can_id,
                                         const uint8_t *tx_data, uint8_t tx_dlc,
                                         uint8_t node_id, uint8_t cmd_echo,
                                         uint32_t timeout_ms,
                                         uint8_t *rx_data, uint8_t rx_max,
                                         uint8_t *rx_len)
{
    if (tx_data == NULL || tx_dlc == 0U)
    {
        return DETECTOR_ERR_PARAM;
    }

    /* Build and send CAN frame */
    CAN_Msg_t tx_msg;
    tx_msg.id  = can_id;
    tx_msg.dlc = tx_dlc;
    memcpy(tx_msg.data, tx_data, tx_dlc);

    if (CAN_Send(&tx_msg) != HAL_OK)
    {
        return DETECTOR_ERR_SEND;
    }

    /* Wait for response */
    CAN_Msg_t rx_msg;
    if (!_wait_response(node_id, cmd_echo, timeout_ms, &rx_msg))
    {
        return DETECTOR_ERR_TIMEOUT;
    }

    /* Copy response data to caller */
    uint8_t copy_len = rx_msg.dlc;
    if (copy_len > rx_max)
    {
        copy_len = rx_max;
    }
    if (rx_data != NULL)
    {
        memcpy(rx_data, rx_msg.data, copy_len);
    }
    if (rx_len != NULL)
    {
        *rx_len = copy_len;
    }

    return DETECTOR_OK;
}

/**
  * @brief  Poll for a matching CAN response frame
  */
static int _wait_response(uint8_t node_id, uint8_t cmd_echo,
                          uint32_t timeout_ms, CAN_Msg_t *rx_out)
{
    if (timeout_ms < DETECTOR_MIN_TIMEOUT_MS)
    {
        timeout_ms = DETECTOR_MIN_TIMEOUT_MS;
    }

    uint32_t tick_start = HAL_GetTick();
    CAN_Msg_t rx;

    while ((HAL_GetTick() - tick_start) < timeout_ms)
    {
        if (CAN_GetRxMessage(&rx) == HAL_OK)
        {
            /* Match: CAN ID matches node_id (or broadcast), Byte0 matches cmd echo */
            if ((rx.id == (uint32_t)node_id || node_id == DETECTOR_BROADCAST) &&
                rx.dlc >= 1U &&
                rx.data[0] == cmd_echo)
            {
                if (rx_out != NULL)
                {
                    memcpy(rx_out, &rx, sizeof(CAN_Msg_t));
                }
                return 1;
            }
        }
        HAL_Delay(DETECTOR_POLL_INTERVAL_MS);
    }

    return 0;
}

/**
  * @brief  Convert a hex string to a byte
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
  * @brief  Print hex dump to log
  */
static void _print_hex(const char *prefix, const uint8_t *data, uint16_t len)
{
    char hex_str[256U];
    int  pos = 0;

    for (uint16_t i = 0U; i < len && pos < (int)(sizeof(hex_str) - 4); i++)
    {
        pos += snprintf(&hex_str[pos],
                        (size_t)(sizeof(hex_str) - (size_t)pos - 1U),
                        "%02X ", (unsigned)data[i]);
    }

    if (pos > 0)
    {
        hex_str[pos - 1] = '\0';
    }

    LOG_INFO("%s (%u bytes): %s", prefix, (unsigned)len, hex_str);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Debug CLI — "detector" command                                              */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  'detector' debug CLI command — interactive CAN protocol testing
  *
  *         Usage:
  *           detector sn <node_id>
  *           detector version <node_id>
  *           detector reset <node_id>
  *           detector led <node_id> <on/off> [period_ms]
  *           detector nodeid <node_id> <new_id>
  *           detector nodeid <node_id> reset
  *           detector control <node_id> <mode> <hold_ms> <threshold> [gate_ms]
  *           detector stop <node_id>
  *           detector switch <node_id> <sw1> <sw2> <sw3> <sw4> <sw5> <sw6>
  *           detector att <node_id> <att1> <att2> <att3> <att4> <att5> <att6> <att7>
  *           detector freq <node_id> <ch> <freq_khz> [lmx_pwr]
  *           detector band <node_id> <band_mhz> <mode> <mask_hex>
  *           detector temp <node_id> [det] [mcu]
  *           detector power <node_id> <freq_khz> <hold_ms> <mode> <thr> <gate>
  *           detector txpower <node_id> <ch> <freq_khz> <target_dbm100> [gps] [comp]
  *           detector flash <node_id>
  *           detector readflash <node_id> <addr_hex> <len>
  *           detector cal <node_id> enter|exit
  *           detector ota prepare <node_id> <major> <minor> <patch>
  *           detector writesn <node_id> <sn_string>
  */
static void _dbg_cmd_detector(int argc, char **argv)
{
    if (argc < 2)
    {
        LOG_INFO("Usage: detector <subcmd> [args...]");
        LOG_INFO("Sub-commands:");
        LOG_INFO("  sn <node_id>                    — Read serial number (0x101)");
        LOG_INFO("  version <node_id>               — Read firmware version (0x103)");
        LOG_INFO("  reset <node_id>                 — Reset MCU (0x104)");
        LOG_INFO("  led <node_id> <0|1> [period]   — LED control (0x105)");
        LOG_INFO("  nodeid <node_id> <new_id>      — Write Node ID (0x102)");
        LOG_INFO("  nodeid <node_id> reset          — Reset Node ID to default");
        LOG_INFO("  control <node> <mode> <hold> <thr> [gate] — Detector cmd (0x110)");
        LOG_INFO("  stop <node_id>                  — Stop detection (0x112)");
        LOG_INFO("  switch <node> <s1>..<s6>       — Switch control (0x111)");
        LOG_INFO("  att <node> <a1>..<a7>          — Attenuator set (0x113)");
        LOG_INFO("  freq <node> <ch> <khz> [pwr]   — VCO frequency (0x114)");
        LOG_INFO("  band <node> <mhz> <mode> <mask> — Band select (0x115)");
        LOG_INFO("  temp <node> [det] [mcu]         — Temperature (0x116)");
        LOG_INFO("  power <node> <khz> <hold> <mode> <thr> <gate> — Query (0x11B)");
        LOG_INFO("  txpower <node> <ch> <khz> <dbm100> [gps] [comp] — Tx (0x11C)");
        LOG_INFO("  flash <node>                    — Flash info (0x117)");
        LOG_INFO("  readflash <node> <addr> <len>  — Read flash (0x11A)");
        LOG_INFO("  cal <node> enter|exit          — Calibration mode (0x120/0x121)");
        LOG_INFO("  ota prepare <node> <maj> <min> <pat> — OTA prepare (0x1A0)");
        LOG_INFO("  writesn <node> <sn_string>      — Write SN (0x106)");
        LOG_INFO("  help                            — Show this help");
        return;
    }

    const char *cmd = argv[1];

    /* ── detector sn <node_id> ───────────────────────────────────────────── */
    if (strcmp(cmd, "sn") == 0)
    {
        if (argc < 3) { LOG_INFO("Usage: detector sn <node_id>"); return; }
        long id = strtol(argv[2], NULL, 0);
        if (id < 0 || id > 255) { LOG_INFO("Invalid node_id"); return; }

        Detector_SN_t sn;
        Detector_Status_t s = Detector_ReadSN((uint8_t)id, &sn);
        if (s == DETECTOR_OK)
        {
            LOG_INFO("Node %ld SN: '%s' (len=%u)", id, sn.sn, (unsigned)sn.sn_len);
        }
        else
        {
            LOG_ERROR("Node %ld SN query failed: %s", id, Detector_StatusString(s));
        }
        return;
    }

    /* ── detector version <node_id> ──────────────────────────────────────── */
    if (strcmp(cmd, "version") == 0)
    {
        if (argc < 3) { LOG_INFO("Usage: detector version <node_id>"); return; }
        long id = strtol(argv[2], NULL, 0);
        if (id < 0 || id > 255) { LOG_INFO("Invalid node_id"); return; }

        Detector_Version_t ver;
        Detector_Status_t s = Detector_ReadVersion((uint8_t)id, &ver);
        if (s == DETECTOR_OK)
        {
            LOG_INFO("Node %ld version: %u.%u.%u", id,
                     (unsigned)ver.major, (unsigned)ver.minor, (unsigned)ver.patch);
        }
        else
        {
            LOG_ERROR("Node %ld version query failed: %s", id, Detector_StatusString(s));
        }
        return;
    }

    /* ── detector reset <node_id> ────────────────────────────────────────── */
    if (strcmp(cmd, "reset") == 0)
    {
        if (argc < 3) { LOG_INFO("Usage: detector reset <node_id>"); return; }
        long id = strtol(argv[2], NULL, 0);
        if (id < 0 || id > 255) { LOG_INFO("Invalid node_id"); return; }

        Detector_Status_t s = Detector_Reset((uint8_t)id);
        LOG_INFO("Node %ld reset: %s", id, Detector_StatusString(s));
        return;
    }

    /* ── detector led <node_id> <0|1> [period_ms] ────────────────────────── */
    if (strcmp(cmd, "led") == 0)
    {
        if (argc < 4) { LOG_INFO("Usage: detector led <node_id> <0|1> [period_ms]"); return; }
        long id = strtol(argv[2], NULL, 0);
        long en = strtol(argv[3], NULL, 0);
        long period = (argc >= 5) ? strtol(argv[4], NULL, 0) : 1000L;

        Detector_Status_t s = Detector_LEDControl((uint8_t)id,
                                                   (uint8_t)(en ? 1 : 0),
                                                   (uint16_t)period);
        LOG_INFO("Node %ld LED %s (period=%ld): %s",
                 id, en ? "on" : "off", period, Detector_StatusString(s));
        return;
    }

    /* ── detector nodeid <node_id> <new_id|reset> ────────────────────────── */
    if (strcmp(cmd, "nodeid") == 0)
    {
        if (argc < 4) { LOG_INFO("Usage: detector nodeid <node_id> <new_id|reset>"); return; }
        long id  = strtol(argv[2], NULL, 0);

        uint8_t reset_flag = 0U;
        long    new_id     = 0;

        if (strcmp(argv[3], "reset") == 0)
        {
            reset_flag = 1U;
        }
        else
        {
            new_id = strtol(argv[3], NULL, 0);
        }

        Detector_NodeId_t result;
        Detector_Status_t s = Detector_WriteNodeId((uint8_t)id, reset_flag,
                                                    (uint8_t)new_id, &result);
        if (s == DETECTOR_OK)
        {
            LOG_INFO("Node %ld new ID: %u", id, (unsigned)result.node_id);
        }
        else
        {
            LOG_ERROR("Node %ld write ID failed: %s", id, Detector_StatusString(s));
        }
        return;
    }

    /* ── detector control <node> <mode> <hold_ms> <threshold> [gate_ms] ──── */
    if (strcmp(cmd, "control") == 0)
    {
        if (argc < 6) { LOG_INFO("Usage: detector control <node> <mode> <hold> <thr> [gate]"); return; }
        long id    = strtol(argv[2], NULL, 0);
        long mode  = strtol(argv[3], NULL, 0);
        long hold  = strtol(argv[4], NULL, 0);
        long thr   = strtol(argv[5], NULL, 0);
        long gate  = (argc >= 7) ? strtol(argv[6], NULL, 0) : 0L;

        Detector_ControlResult_t r;
        Detector_Status_t s = Detector_Control((uint8_t)id, (Detector_Mode_t)mode,
                                                (uint8_t)hold, (uint16_t)thr,
                                                (uint16_t)gate, &r);
        if (s == DETECTOR_OK)
        {
            LOG_INFO("Node %ld: H_avg=%d, V_avg=%d, mode=%u, state=%u",
                     id, r.h_adc_avg, r.v_adc_avg, (unsigned)r.mode, (unsigned)r.state);
        }
        else
        {
            LOG_ERROR("Node %ld control failed: %s", id, Detector_StatusString(s));
        }
        return;
    }

    /* ── detector stop <node_id> ─────────────────────────────────────────── */
    if (strcmp(cmd, "stop") == 0)
    {
        if (argc < 3) { LOG_INFO("Usage: detector stop <node_id>"); return; }
        long id = strtol(argv[2], NULL, 0);
        Detector_Status_t s = Detector_Stop((uint8_t)id);
        LOG_INFO("Node %ld stop: %s", id, Detector_StatusString(s));
        return;
    }

    /* ── detector switch <node> <s1>..<s6> ──────────────────────────────── */
    if (strcmp(cmd, "switch") == 0)
    {
        if (argc < 9) { LOG_INFO("Usage: detector switch <node> <s1> <s2> <s3> <s4> <s5> <s6>"); return; }
        long id = strtol(argv[2], NULL, 0);

        Detector_Status_t s = Detector_SwitchControl(
            (uint8_t)id,
            (uint8_t)strtol(argv[3], NULL, 0),
            (uint8_t)strtol(argv[4], NULL, 0),
            (uint8_t)strtol(argv[5], NULL, 0),
            (uint8_t)strtol(argv[6], NULL, 0),
            (uint8_t)strtol(argv[7], NULL, 0),
            (uint8_t)strtol(argv[8], NULL, 0));
        LOG_INFO("Node %ld switch: %s", id, Detector_StatusString(s));
        return;
    }

    /* ── detector att <node> <a1>..<a7> ────────────────────────────────── */
    if (strcmp(cmd, "att") == 0)
    {
        if (argc < 10) { LOG_INFO("Usage: detector att <node> <a1> <a2> <a3> <a4> <a5> <a6> <a7>"); return; }
        long id = strtol(argv[2], NULL, 0);

        Detector_Attenuator_t att;
        att.att1 = (uint8_t)strtol(argv[3], NULL, 0);
        att.att2 = (uint8_t)strtol(argv[4], NULL, 0);
        att.att3 = (uint8_t)strtol(argv[5], NULL, 0);
        att.att4 = (uint8_t)strtol(argv[6], NULL, 0);
        att.att5 = (uint8_t)strtol(argv[7], NULL, 0);
        att.att6 = (uint8_t)strtol(argv[8], NULL, 0);
        att.att7 = (uint8_t)strtol(argv[9], NULL, 0);

        Detector_Status_t s = Detector_SetAttenuator((uint8_t)id, &att);
        LOG_INFO("Node %ld attenuator: %s", id, Detector_StatusString(s));
        return;
    }

    /* ── detector freq <node> <ch> <freq_khz> [lmx_pwr] ──────────────────── */
    if (strcmp(cmd, "freq") == 0)
    {
        if (argc < 5) { LOG_INFO("Usage: detector freq <node> <ch> <freq_khz> [lmx_pwr]"); return; }
        long id      = strtol(argv[2], NULL, 0);
        long ch      = strtol(argv[3], NULL, 0);
        long freq    = strtol(argv[4], NULL, 0);
        long lmx_pwr = (argc >= 6) ? strtol(argv[5], NULL, 0) : 0L;

        Detector_FreqResult_t r;
        Detector_Status_t s = Detector_SetFrequency(
            (uint8_t)id, (Detector_Channel_t)ch,
            (uint32_t)freq, (uint8_t)lmx_pwr, &r);
        if (s == DETECTOR_OK)
        {
            LOG_INFO("Node %ld freq: ch=%u, actual=%lu Hz, lmx_pwr=%u",
                     id, (unsigned)r.channel,
                     (unsigned long)r.freq_hz, (unsigned)r.lmx_power);
        }
        else
        {
            LOG_ERROR("Node %ld freq failed: %s", id, Detector_StatusString(s));
        }
        return;
    }

    /* ── detector band <node> <band_mhz> <mode> <mask_hex> ───────────────── */
    if (strcmp(cmd, "band") == 0)
    {
        if (argc < 6) { LOG_INFO("Usage: detector band <node> <band_mhz> <mode> <mask_hex>"); return; }
        long id    = strtol(argv[2], NULL, 0);
        long mhz   = strtol(argv[3], NULL, 0);
        long mode  = strtol(argv[4], NULL, 0);
        uint64_t mask = (uint64_t)strtoull(argv[5], NULL, 0);

        Detector_BandResult_t r;
        Detector_Status_t s = Detector_SelectBand(
            (uint8_t)id, (uint8_t)mhz, (uint8_t)mode, mask, &r);
        if (s == DETECTOR_OK)
        {
            LOG_INFO("Node %ld band: mode=%u, V=%u, H=%u",
                     id, (unsigned)r.mode, (unsigned)r.v_selected,
                     (unsigned)r.h_selected);
        }
        else
        {
            LOG_ERROR("Node %ld band failed: %s", id, Detector_StatusString(s));
        }
        return;
    }

    /* ── detector temp <node> [det] [mcu] ────────────────────────────────── */
    if (strcmp(cmd, "temp") == 0)
    {
        if (argc < 3) { LOG_INFO("Usage: detector temp <node> [det] [mcu]"); return; }
        long id  = strtol(argv[2], NULL, 0);
        long det = (argc >= 4) ? strtol(argv[3], NULL, 0) : 1L;
        long mcu = (argc >= 5) ? strtol(argv[4], NULL, 0) : 1L;

        Detector_Temp_t t;
        Detector_Status_t s = Detector_ReadTemperature(
            (uint8_t)id, (uint8_t)det, (uint8_t)mcu, &t);
        if (s == DETECTOR_OK)
        {
            LOG_INFO("Node %ld temp: det=%d, mcu=%d, rsv=%d",
                     id, t.detector_temp, t.mcu_temp, t.reserved);
        }
        else
        {
            LOG_ERROR("Node %ld temp failed: %s", id, Detector_StatusString(s));
        }
        return;
    }

    /* ── detector power <node> <khz> <hold> <mode> <thr> <gate> ──────────── */
    if (strcmp(cmd, "power") == 0)
    {
        if (argc < 8) { LOG_INFO("Usage: detector power <node> <khz> <hold> <mode> <thr> <gate>"); return; }
        long id    = strtol(argv[2], NULL, 0);
        long freq  = strtol(argv[3], NULL, 0);
        long hold  = strtol(argv[4], NULL, 0);
        long mode  = strtol(argv[5], NULL, 0);
        long thr   = strtol(argv[6], NULL, 0);
        long gate  = strtol(argv[7], NULL, 0);

        Detector_Power_t p;
        Detector_Status_t s = Detector_QueryPower(
            (uint8_t)id, (uint32_t)freq, (uint8_t)hold,
            (Detector_Mode_t)mode, (uint16_t)thr, (uint16_t)gate, 0U, &p);
        if (s == DETECTOR_OK)
        {
            LOG_INFO("Node %ld power: H=%.2f dBm, V=%.2f dBm, mode=%u, state=%u",
                     id, p.h_power_dbm100 / 100.0f, p.v_power_dbm100 / 100.0f,
                     (unsigned)p.mode, (unsigned)p.state);
        }
        else
        {
            LOG_ERROR("Node %ld power query failed: %s", id, Detector_StatusString(s));
        }
        return;
    }

    /* ── detector txpower <node> <ch> <khz> <dbm100> [gps] [comp] ────────── */
    if (strcmp(cmd, "txpower") == 0)
    {
        if (argc < 6) { LOG_INFO("Usage: detector txpower <node> <ch> <khz> <dbm100> [gps] [comp]"); return; }
        long id    = strtol(argv[2], NULL, 0);
        long ch    = strtol(argv[3], NULL, 0);
        long freq  = strtol(argv[4], NULL, 0);
        long power = strtol(argv[5], NULL, 0);
        long gps   = (argc >= 7) ? strtol(argv[6], NULL, 0) : 0L;
        long comp  = (argc >= 8) ? strtol(argv[7], NULL, 0) : 0L;

        Detector_TxPowerResult_t r;
        Detector_Status_t s = Detector_SetTxPower(
            (uint8_t)id, (Detector_Channel_t)ch, (uint32_t)freq,
            (int16_t)power, (uint8_t)gps, (uint8_t)comp, &r);
        if (s == DETECTOR_OK)
        {
            LOG_INFO("Node %ld txpower: ch=%u, freq=%lu Hz, pred=%.2f dBm",
                     id, (unsigned)r.channel, (unsigned long)r.freq_hz,
                     r.predicted_power_dbm100 / 100.0f);
            LOG_INFO("  ATT: %u/%u/%u/%u/%u/%u/%u",
                     (unsigned)r.att1, (unsigned)r.att2, (unsigned)r.att3,
                     (unsigned)r.att4, (unsigned)r.att5, (unsigned)r.att6,
                     (unsigned)r.att7);
        }
        else
        {
            LOG_ERROR("Node %ld txpower failed: %s", id, Detector_StatusString(s));
        }
        return;
    }

    /* ── detector flash <node> ───────────────────────────────────────────── */
    if (strcmp(cmd, "flash") == 0)
    {
        if (argc < 3) { LOG_INFO("Usage: detector flash <node_id>"); return; }
        long id = strtol(argv[2], NULL, 0);

        Detector_FlashInfo_t info;
        Detector_Status_t s = Detector_GetFlashInfo((uint8_t)id, &info);
        if (s == DETECTOR_OK)
        {
            LOG_INFO("Node %ld Flash Info:", id);
            LOG_INFO("  JEDEC ID: 0x%06lX, Status: 0x%02X",
                     (unsigned long)info.jedec_id, (unsigned)info.status_reg1);
            LOG_INFO("  OTA image:   0x%06lX (%lu KB)",
                     (unsigned long)info.ota_img_addr,
                     (unsigned long)(info.ota_img_size / 1024UL));
            LOG_INFO("  H coarse:    0x%06lX (%lu KB)",
                     (unsigned long)info.cal_h_coarse_addr,
                     (unsigned long)(info.cal_h_coarse_size / 1024UL));
            LOG_INFO("  H fine:      0x%06lX (%lu KB)",
                     (unsigned long)info.cal_h_fine_addr,
                     (unsigned long)(info.cal_h_fine_size / 1024UL));
            LOG_INFO("  V coarse:    0x%06lX (%lu KB)",
                     (unsigned long)info.cal_v_coarse_addr,
                     (unsigned long)(info.cal_v_coarse_size / 1024UL));
            LOG_INFO("  V fine:      0x%06lX (%lu KB)",
                     (unsigned long)info.cal_v_fine_addr,
                     (unsigned long)(info.cal_v_fine_size / 1024UL));
            LOG_INFO("  Ext cal:     0x%06lX (%lu KB)",
                     (unsigned long)info.cal_ext_addr,
                     (unsigned long)(info.cal_ext_size / 1024UL));
            LOG_INFO("  Reserved:    0x%06lX (%lu KB)",
                     (unsigned long)info.reserved_addr,
                     (unsigned long)(info.reserved_size / 1024UL));
        }
        else
        {
            LOG_ERROR("Node %ld flash info failed: %s", id, Detector_StatusString(s));
        }
        return;
    }

    /* ── detector readflash <node> <addr_hex> <len> ──────────────────────── */
    if (strcmp(cmd, "readflash") == 0)
    {
        if (argc < 5) { LOG_INFO("Usage: detector readflash <node> <addr> <len>"); return; }
        long id   = strtol(argv[2], NULL, 0);
        long addr = strtol(argv[3], NULL, 0);
        long len  = strtol(argv[4], NULL, 0);
        if (len < 1 || len > 58) { LOG_INFO("Length must be 1-58"); return; }

        Detector_FlashRead_t r;
        Detector_Status_t s = Detector_ReadFlash((uint8_t)id,
                                                   (uint32_t)addr,
                                                   (uint8_t)len, &r);
        if (s == DETECTOR_OK)
        {
            LOG_INFO("Node %ld flash read @0x%06lX (%u bytes):",
                     id, (unsigned long)r.addr, (unsigned)r.data_len);
            _print_hex("Flash", r.data, r.data_len);
        }
        else
        {
            LOG_ERROR("Node %ld read flash failed: %s", id, Detector_StatusString(s));
        }
        return;
    }

    /* ── detector cal <node> enter|exit ──────────────────────────────────── */
    if (strcmp(cmd, "cal") == 0)
    {
        if (argc < 4) { LOG_INFO("Usage: detector cal <node> enter|exit"); return; }
        long id = strtol(argv[2], NULL, 0);

        Detector_Status_t s;
        if (strcmp(argv[3], "enter") == 0)
        {
            s = Detector_EnterCalibration((uint8_t)id);
            LOG_INFO("Node %ld enter cal: %s", id, Detector_StatusString(s));
        }
        else if (strcmp(argv[3], "exit") == 0)
        {
            s = Detector_ExitCalibration((uint8_t)id);
            LOG_INFO("Node %ld exit cal: %s", id, Detector_StatusString(s));
        }
        else
        {
            LOG_INFO("Usage: detector cal <node> enter|exit");
        }
        return;
    }

    /* ── detector ota prepare <node> <maj> <min> <pat> ───────────────────── */
    if (strcmp(cmd, "ota") == 0)
    {
        if (argc < 4) { LOG_INFO("Usage: detector ota prepare <node> <maj> <min> <pat>"); return; }

        if (strcmp(argv[3], "prepare") == 0)
        {
            if (argc < 7) { LOG_INFO("Usage: detector ota prepare <node> <maj> <min> <pat>"); return; }
            long id  = strtol(argv[2], NULL, 0);
            long maj = strtol(argv[4], NULL, 0);
            long min = strtol(argv[5], NULL, 0);
            long pat = strtol(argv[6], NULL, 0);

            uint8_t code = 0;
            Detector_Status_t s = Detector_OTAPrepare(
                (uint8_t)id, (uint8_t)maj, (uint8_t)min, (uint8_t)pat, &code);
            LOG_INFO("Node %ld OTA prepare: %s (code=0x%02X)",
                     id, Detector_StatusString(s), (unsigned)code);
        }
        else
        {
            LOG_INFO("Unknown ota sub-command. Use: detector ota prepare ...");
        }
        return;
    }

    /* ── detector writesn <node> <sn_string> ─────────────────────────────── */
    if (strcmp(cmd, "writesn") == 0)
    {
        if (argc < 4) { LOG_INFO("Usage: detector writesn <node_id> <sn_string>"); return; }
        long id = strtol(argv[2], NULL, 0);
        const char *sn_str = argv[3];
        uint8_t slen = (uint8_t)strlen(sn_str);
        if (slen < 1 || slen > DETECTOR_SN_MAX_LEN)
        {
            LOG_INFO("SN length must be 1-%u, got %u", (unsigned)DETECTOR_SN_MAX_LEN, (unsigned)slen);
            return;
        }

        uint8_t written = 0;
        Detector_Status_t s = Detector_WriteSN((uint8_t)id, sn_str, slen, &written);
        if (s == DETECTOR_OK)
        {
            LOG_INFO("Node %ld SN write OK: '%s' (%u bytes)", id, sn_str, (unsigned)written);
        }
        else
        {
            LOG_ERROR("Node %ld SN write failed: %s", id, Detector_StatusString(s));
        }
        return;
    }

    /* ── detector help ───────────────────────────────────────────────────── */
    if (strcmp(cmd, "help") == 0)
    {
        LOG_INFO("Available sub-commands:");
        LOG_INFO("  sn, version, reset, led, nodeid, control, stop, switch");
        LOG_INFO("  att, freq, band, temp, power, txpower, flash, readflash");
        LOG_INFO("  cal, ota, writesn");
        LOG_INFO("  Use 'detector' without args for full listing");
        return;
    }

    LOG_INFO("Unknown sub-command: '%s'. Type 'detector help'.", cmd);
}
