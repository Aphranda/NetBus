/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    detector_task.h
  * @brief   A1 Detector Board CAN protocol — command API for system control,
  *          detector operation, power management, flash access, calibration,
  *          and OTA update.
  *
  *          Protocol reference: Doc/V3.x A1检波板指令.docx
  *
  *          All CAN frames use 11-bit standard ID. Default DLC = 8 bytes
  *          (except OTA data blocks and CAN FD power commands).
  *
  *          Request frame:  Byte0 = target Node ID (0xFF = broadcast)
  *          Response frame: ID = nodeId, Byte0 = command echo (low byte of
  *                          command ID), Byte1 = result code (0 = success)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __DETECTOR_TASK_H__
#define __DETECTOR_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "app_task.h"

/* Exported constants --------------------------------------------------------*/

/** @brief Default response timeout (ms) for standard 8-byte commands */
#define DETECTOR_TIMEOUT_MS         500U

/** @brief Extended response timeout (ms) for flash erase/write commands */
#define DETECTOR_TIMEOUT_FLASH_MS   5000U

/** @brief Extended response timeout (ms) for OTA commands */
#define DETECTOR_TIMEOUT_OTA_MS     10000U

/** @brief Maximum SN string length (per protocol) */
#define DETECTOR_SN_MAX_LEN         61U

/** @brief Maximum version string length */
#define DETECTOR_VERSION_LEN        3U

/** @brief Maximum external flash read length (per single read command, CAN FD 64B) */
#define DETECTOR_FLASH_READ_MAX     58U

/* Exported types ------------------------------------------------------------*/

/**
  * @brief  Detector command result codes
  */
typedef enum {
    DETECTOR_OK           = 0x00U,  /*!< Success                                  */
    DETECTOR_ERR_FAIL     = 0x01U,  /*!< Generic failure (device-reported)        */
    DETECTOR_ERR_PARAM    = 0x02U,  /*!< Invalid parameter                        */
    DETECTOR_ERR_TIMEOUT  = 0x03U,  /*!< No response from device                  */
    DETECTOR_ERR_BUSY     = 0x04U,  /*!< Device busy (OTA/calibration/SN-write)   */
    DETECTOR_ERR_STATE    = 0x05U,  /*!< Wrong state for this command             */
    DETECTOR_ERR_SEND     = 0x06U,  /*!< CAN send failed                          */
    DETECTOR_ERR_RESP     = 0x07U,  /*!< Malformed response                       */
} Detector_Status_t;

/**
  * @brief  Detector operation mode (for 0x110 detector control)
  */
typedef enum {
    DETECTOR_MODE_TIME     = 0U,  /*!< Timed measurement                        */
    DETECTOR_MODE_TRIGGER  = 1U,  /*!< Trigger measurement                      */
    DETECTOR_MODE_PULSE    = 2U,  /*!< Pulse measurement                        */
} Detector_Mode_t;

/**
  * @brief  Detector channel selection (for 0x114 VCO / 0x11C Tx Power)
  */
typedef enum {
    DETECTOR_CH_OFF       = 0U,  /*!< Close / turn off                         */
    DETECTOR_CH_V         = 1U,  /*!< V channel                                */
    DETECTOR_CH_H         = 2U,  /*!< H channel                                */
} Detector_Channel_t;

/**
  * @brief  Detector measure state (Byte7 of 0x110/0x11B response)
  */
typedef enum {
    DETECTOR_MEASURE_DONE       = 0x00U,  /*!< Timed/pulse measure finished             */
    DETECTOR_MEASURE_ARMED      = 0x01U,  /*!< Trigger mode armed                       */
    DETECTOR_MEASURE_TIMEOUT    = 0x02U,  /*!< Trigger mode timed out (not triggered)   */
} Detector_MeasureState_t;

/* ── Response structures (one per command) ─────────────────────────────────── */

/** @brief 0x101 Read SN response */
typedef struct {
    uint8_t  sn[DETECTOR_SN_MAX_LEN + 1U];  /*!< Null-terminated SN string        */
    uint8_t  sn_len;                         /*!< SN character count               */
} Detector_SN_t;

/** @brief 0x102 Write Node ID response */
typedef struct {
    uint8_t  node_id;   /*!< Current Node ID after write (success) or original */
} Detector_NodeId_t;

/** @brief 0x103 Read version response */
typedef struct {
    uint8_t  major;
    uint8_t  minor;
    uint8_t  patch;
} Detector_Version_t;

/** @brief 0x110 Detector control response */
typedef struct {
    int16_t  h_adc_avg;     /*!< H channel ADC average value                   */
    int16_t  v_adc_avg;     /*!< V channel ADC average value                   */
    uint8_t  mode;          /*!< Echo of requested mode                        */
    uint8_t  state;         /*!< Measure state (Detector_MeasureState_t)       */
} Detector_ControlResult_t;

/** @brief 0x113 Attenuator settings */
typedef struct {
    uint8_t  att1;          /*!< ATT1: 0-15                                     */
    uint8_t  att2;          /*!< ATT2: 0-15                                     */
    uint8_t  att3;          /*!< ATT3: 0-15                                     */
    uint8_t  att4;          /*!< ATT4: 0/7/8/15                                 */
    uint8_t  att5;          /*!< ATT5: 0/7/8/15                                 */
    uint8_t  att6;          /*!< ATT6: 0/1 (1.7G filter bypass)                 */
    uint8_t  att7;          /*!< ATT7: 0/7/8/15                                 */
} Detector_Attenuator_t;

/** @brief 0x114 VCO frequency setting response */
typedef struct {
    uint32_t  freq_hz;      /*!< Actual frequency set (Hz, hardware-returned)   */
    uint8_t   channel;      /*!< Channel (0/1/2)                                */
    uint8_t   lmx_power;    /*!< LMX output power register value (0-63)         */
} Detector_FreqResult_t;

/** @brief 0x115 Band select response */
typedef struct {
    uint8_t  mode;          /*!< Band mode (0 = single point, >0 = sweep)       */
    uint8_t  v_selected;    /*!< V channel selected status (0/1)                */
    uint8_t  h_selected;    /*!< H channel selected status (0/1)                */
} Detector_BandResult_t;

/** @brief 0x116 Temperature reading response */
typedef struct {
    int16_t  detector_temp;  /*!< Detector board temperature (signed, unit TBD)  */
    int16_t  mcu_temp;       /*!< MCU internal temperature                       */
    int16_t  reserved;       /*!< Reserved                                       */
} Detector_Temp_t;

/** @brief 0x117 External Flash info response */
typedef struct {
    uint32_t  jedec_id;         /*!< JEDEC ID (3 bytes, MSB-padded)            */
    uint8_t   status_reg1;      /*!< Status register 1                          */
    uint32_t  ota_img_addr;     /*!< OTA image partition start address          */
    uint32_t  ota_img_size;     /*!< OTA image partition size                   */
    uint32_t  cal_h_coarse_addr;/*!< H coarse calibration start address         */
    uint32_t  cal_h_coarse_size;/*!< H coarse calibration size                  */
    uint32_t  cal_h_fine_addr;  /*!< H fine calibration start address           */
    uint32_t  cal_h_fine_size;  /*!< H fine calibration size                    */
    uint32_t  cal_v_coarse_addr;/*!< V coarse calibration start address         */
    uint32_t  cal_v_coarse_size;/*!< V coarse calibration size                  */
    uint32_t  cal_ext_addr;     /*!< Extended calibration region start address  */
    uint32_t  cal_ext_size;     /*!< Extended calibration region total size     */
    uint32_t  reserved_addr;    /*!< Reserved/unused region start address       */
    uint32_t  reserved_size;    /*!< Reserved/unused region size                 */
    uint32_t  cal_v_fine_addr;  /*!< V fine calibration start address           */
    uint32_t  cal_v_fine_size;  /*!< V fine calibration size                    */
} Detector_FlashInfo_t;

/** @brief 0x118 / 0x119 Flash erase/write response */
typedef struct {
    uint32_t  addr;         /*!< Operation start address (echo)                 */
    uint32_t  length;       /*!< Operated length (echo, or actual for erase)    */
} Detector_FlashOpResult_t;

/** @brief 0x11A Flash read result */
typedef struct {
    uint32_t  addr;                              /*!< Read start address            */
    uint8_t   data[DETECTOR_FLASH_READ_MAX];     /*!< Read-back data                */
    uint8_t   data_len;                          /*!< Actual returned data length   */
} Detector_FlashRead_t;

/** @brief 0x11B Power query response */
typedef struct {
    int16_t   h_power_dbm100;   /*!< H channel power (dBm * 100, int16)          */
    int16_t   v_power_dbm100;   /*!< V channel power (dBm * 100, int16)          */
    uint8_t   mode;             /*!< Mode echo                                    */
    uint8_t   state;            /*!< Measure state (Detector_MeasureState_t)      */
} Detector_Power_t;

/** @brief 0x11C Tx power setting response */
typedef struct {
    uint8_t   channel;          /*!< Channel (0/1/2)                              */
    uint32_t  freq_hz;          /*!< Actual frequency set                         */
    uint8_t   att1, att2, att3; /*!< ATT1-3 final values                          */
    uint8_t   att4, att5, att6, att7;  /*!< ATT4-7 final values                  */
    int16_t   predicted_power_dbm100;  /*!< Predicted power (dBm * 100)          */
} Detector_TxPowerResult_t;

/* ── OTA types ─────────────────────────────────────────────────────────────── */

/** @brief OTA status codes */
typedef enum {
    DETECTOR_OTA_OK            = 0x00U,
    DETECTOR_OTA_ERR_FAIL      = 0x01U,
    DETECTOR_OTA_ERR_SAME_VER  = 0x03U,  /*!< Same version, no need to update */
    DETECTOR_OTA_ERR_BUSY      = 0x04U,  /*!< Already in OTA state            */
} Detector_OTAStatus_t;

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/** @brief Debug CLI handler for "detector" command — registered by log_task */
void Detector_CLIHandler(int argc, char **argv);

/* ── System commands ──────────────────────────────────────────────────────── */

/** @brief 0x101 Read device serial number */
Detector_Status_t Detector_ReadSN(uint8_t node_id, Detector_SN_t *result);

/** @brief 0x102 Write Node ID (1-40, or 0 for flash reset to default=1) */
Detector_Status_t Detector_WriteNodeId(uint8_t node_id, uint8_t reset_flag,
                                        uint8_t new_id, Detector_NodeId_t *result);

/** @brief 0x103 Read firmware version (major.minor.patch) */
Detector_Status_t Detector_ReadVersion(uint8_t node_id, Detector_Version_t *result);

/** @brief 0x104 Reset MCU */
Detector_Status_t Detector_Reset(uint8_t node_id);

/** @brief 0x105 LED control */
Detector_Status_t Detector_LEDControl(uint8_t node_id, uint8_t enable,
                                       uint16_t period_ms);

/** @brief 0x106 Write SN string (1-61 printable ASCII, no trailing NUL needed) */
Detector_Status_t Detector_WriteSN(uint8_t node_id, const char *sn,
                                    uint8_t sn_len, uint8_t *written_len);

/** @brief 0x107 Enter SN write state */
Detector_Status_t Detector_EnterSNWrite(uint8_t node_id);

/** @brief 0x108 Exit SN write state */
Detector_Status_t Detector_ExitSNWrite(uint8_t node_id);

/* ── Detector operation commands ──────────────────────────────────────────── */

/**
  * @brief  0x110 Detector measurement control
  * @param  mode        0=timed, 1=trigger, 2=pulse
  * @param  hold_ms     Measurement hold time (ms, 8-bit)
  * @param  threshold   Detection threshold value
  * @param  gate_ms     Gate time (ms, for pulse mode)
  */
Detector_Status_t Detector_Control(uint8_t node_id, Detector_Mode_t mode,
                                    uint8_t hold_ms, uint16_t threshold,
                                    uint16_t gate_ms, Detector_ControlResult_t *result);

/** @brief 0x111 Switch control (SW1-SW6, each = 0/1, >=2 = keep current) */
Detector_Status_t Detector_SwitchControl(uint8_t node_id,
                                          uint8_t sw1, uint8_t sw2,
                                          uint8_t sw3, uint8_t sw4,
                                          uint8_t sw5, uint8_t sw6);

/** @brief 0x112 Stop current detection measurement */
Detector_Status_t Detector_Stop(uint8_t node_id);

/** @brief 0x113 Attenuator manual setting */
Detector_Status_t Detector_SetAttenuator(uint8_t node_id,
                                          const Detector_Attenuator_t *att);

/** @brief 0x114 VCO frequency setting */
Detector_Status_t Detector_SetFrequency(uint8_t node_id,
                                         Detector_Channel_t channel,
                                         uint32_t freq_khz,
                                         uint8_t lmx_power,
                                         Detector_FreqResult_t *result);

/** @brief 0x115 Band selection */
Detector_Status_t Detector_SelectBand(uint8_t node_id, uint8_t band_mhz,
                                       uint8_t band_mode,
                                       uint64_t bitmask,
                                       Detector_BandResult_t *result);

/** @brief 0x116 Read detector and MCU temperature */
Detector_Status_t Detector_ReadTemperature(uint8_t node_id,
                                            uint8_t read_detector,
                                            uint8_t read_mcu,
                                            Detector_Temp_t *result);

/* ── Power commands ───────────────────────────────────────────────────────── */

/** @brief 0x11B Query detection power */
Detector_Status_t Detector_QueryPower(uint8_t node_id, uint32_t freq_khz,
                                       uint8_t hold_ms, Detector_Mode_t mode,
                                       uint16_t threshold, uint16_t gate_ms,
                                       uint8_t compensate,
                                       Detector_Power_t *result);

/** @brief 0x11C Set transmit power (closed-loop with calibration) */
Detector_Status_t Detector_SetTxPower(uint8_t node_id,
                                       Detector_Channel_t channel,
                                       uint32_t freq_khz,
                                       int16_t target_power_dbm100,
                                       uint8_t gps_flag,
                                       uint8_t compensate,
                                       Detector_TxPowerResult_t *result);

/* ── Flash commands ───────────────────────────────────────────────────────── */

/** @brief 0x117 Query external flash layout */
Detector_Status_t Detector_GetFlashInfo(uint8_t node_id,
                                         Detector_FlashInfo_t *result);

/** @brief 0x118 Erase external flash sector (4KB-aligned) */
Detector_Status_t Detector_EraseFlash(uint8_t node_id, uint32_t addr,
                                       uint32_t len,
                                       Detector_FlashOpResult_t *result);

/** @brief 0x119 Write external flash data */
Detector_Status_t Detector_WriteFlash(uint8_t node_id, uint32_t addr,
                                       const uint8_t *data, uint16_t data_len,
                                       Detector_FlashOpResult_t *result);

/** @brief 0x11A Read external flash data (1-58 bytes per call) */
Detector_Status_t Detector_ReadFlash(uint8_t node_id, uint32_t addr,
                                      uint8_t read_len,
                                      Detector_FlashRead_t *result);

/* ── Calibration commands ─────────────────────────────────────────────────── */

/** @brief 0x120 Enter calibration data write mode */
Detector_Status_t Detector_EnterCalibration(uint8_t node_id);

/** @brief 0x121 Exit calibration data write mode */
Detector_Status_t Detector_ExitCalibration(uint8_t node_id);

/* ── OTA commands ─────────────────────────────────────────────────────────── */

/** @brief 0x1A0 Prepare OTA update (enter OTA mode) */
Detector_Status_t Detector_OTAPrepare(uint8_t node_id, uint8_t ver_major,
                                       uint8_t ver_minor, uint8_t ver_patch,
                                       uint8_t *status_code);

/** @brief 0x1A1 Begin OTA page write (prepare a flash page) */
Detector_Status_t Detector_OTABegin(uint8_t node_id, uint16_t page_num);

/** @brief Send OTA data block (0x300 + blockNum) */
Detector_Status_t Detector_OTAData(uint8_t node_id, uint8_t block_num,
                                    const uint8_t *data, uint8_t data_len);

/** @brief 0x1A3 Complete OTA (validate CRC, reboot if success) */
Detector_Status_t Detector_OTAComplete(uint8_t node_id, uint8_t reboot,
                                        uint16_t crc, uint32_t total_len);

/* ── Utility ──────────────────────────────────────────────────────────────── */

/** @brief Convert Detector_Status_t to a human-readable string */
const char *Detector_StatusString(Detector_Status_t status);

#ifdef __cplusplus
}
#endif

#endif /* __DETECTOR_TASK_H__ */
