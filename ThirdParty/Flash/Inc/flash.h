/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    flash.h
  * @brief   STM32H7 internal Flash base library — low-level erase/program/read
  *
  *          Architecture:
  *            This module abstracts the STM32H7xx HAL FLASH driver for use
  *            by higher-level storage components. It handles Bank 2 operations
  *            with proper critical-section protection and error recovery.
  *
  *            ┌─────────────────────────────────────────────────────┐
  *            │  Storage Layer (storage_task.c)                     │
  *            │    Key-value, CRC, append-only log, RAM cache       │
  *            └──────────────┬──────────────────────────────────────┘
  *                           │
  *            ┌──────────────▼──────────────────────────────────────┐
  *            │  Flash Base Library (ThirdParty/Flash)              │
  *            │    Flash_EraseSector / Flash_Program / Flash_Read   │
  *            │    Bank 2 unlock/lock, error flag handling          │
  *            └──────────────┬──────────────────────────────────────┘
  *                           │
  *            ┌──────────────▼──────────────────────────────────────┐
  *            │  HAL FLASH Driver (stm32h7xx_hal_flash.c)           │
  *            │    HAL_FLASH_Program / HAL_FLASHEx_Erase            │
  *            └─────────────────────────────────────────────────────┘
  *
  *          Target:
  *            STM32H743ZITx — Bank 2, Sector 7 (0x081E0000, 128KB)
  *            Flash word size: 256-bit (32 bytes, 8 × uint32_t)
  *
  *          Important Notes:
  *            1. Flash must be unlocked before program/erase, locked after.
  *            2. Address must be 32-byte aligned for FLASHWORD programming.
  *            3. During erase (~2s for 128KB), CPU can run from Bank 1.
  *            4. taskENTER_CRITICAL is used around flash word writes (~100us each).
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __FLASH_H__
#define __FLASH_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "stm32h7xx_hal.h"

/* Exported constants --------------------------------------------------------*/

/** @defgroup Flash_Banks Flash memory banks
  * @{
  */
#define FLASH_BASE_BANK1                0x08000000U
#define FLASH_BASE_BANK2                0x08100000U

/* FLASH_BANK_SIZE and FLASH_SECTOR_SIZE are defined by CMSIS (stm32h743xx.h) */
#define FLASH_SECTOR_COUNT_PER_BANK     8U

/* User storage: Bank 2, Sector 7 */
#define FLASH_USER_SECTOR               FLASH_SECTOR_7
#define FLASH_USER_BANK                 FLASH_BANK_2
#define FLASH_USER_BASE_ADDR            0x081E0000U

/* Flash word programming */
#define FLASH_WORD_SIZE                 32U          /* 256-bit = 32 bytes */
#define FLASH_WORD_U32_COUNT            8U           /* 8 × uint32_t */
#define FLASH_PROGRAM_TIMEOUT_MS        2000U

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Public API                                                                 */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
  * @brief  Initialize the Flash base library
  * @note   Verifies that the target sector is not write-protected.
  *         Does NOT erase the sector.
  * @retval HAL_OK on success
  */
HAL_StatusTypeDef Flash_Init(void);

/**
  * @brief  Erase one or more flash sectors in the user bank (Bank 2)
  * @param  sector     Starting sector number (0-7 for the user bank)
  * @param  nb_sectors Number of consecutive sectors to erase (1-8)
  * @retval HAL_OK on success
  */
HAL_StatusTypeDef Flash_EraseSector(uint32_t sector, uint32_t nb_sectors);

/**
  * @brief  Program one 256-bit flashword (32 bytes) to flash
  * @note   Both addr and data must be 32-byte aligned.
  *         This is a blocking call (~100us). Flash must be unlocked.
  * @param  addr  Destination address in flash (must be 32-byte aligned)
  * @param  data  Pointer to 8 × uint32_t source data in SRAM
  * @retval HAL_OK on success
  */
HAL_StatusTypeDef Flash_ProgramWord(uint32_t addr, const uint32_t *data);

/**
  * @brief  Program multiple flashwords to flash
  * @param  addr       Destination start address (32-byte aligned)
  * @param  data       Pointer to source data in SRAM
  * @param  word_count Number of flashwords to program
  * @retval HAL_OK on success
  */
HAL_StatusTypeDef Flash_Program(uint32_t addr, const uint32_t *data,
                                uint32_t word_count);

/**
  * @brief  Read bytes from flash memory
  * @param  addr  Source address in flash (any alignment)
  * @param  buf   Destination buffer in SRAM
  * @param  len   Number of bytes to read
  */
void Flash_Read(uint32_t addr, uint8_t *buf, uint32_t len);

/**
  * @brief  Verify that programmed data matches expected data
  * @param  addr  Flash address to verify
  * @param  data  Expected data buffer in SRAM
  * @param  len   Length in bytes
  * @retval HAL_OK if data matches, HAL_ERROR on mismatch
  */
HAL_StatusTypeDef Flash_Verify(uint32_t addr, const uint8_t *data, uint32_t len);

/**
  * @brief  Unlock the user flash bank for program/erase operations
  * @retval HAL_OK on success
  */
HAL_StatusTypeDef Flash_Unlock(void);

/**
  * @brief  Lock the user flash bank after operations are complete
  * @retval HAL_OK on success
  */
HAL_StatusTypeDef Flash_Lock(void);

/**
  * @brief  Check if a flash address is 32-byte aligned for programming
  * @retval 1 if aligned, 0 otherwise
  */
static inline uint8_t Flash_IsAligned(uint32_t addr)
{
    return ((addr & 0x1FU) == 0U) ? 1U : 0U;
}

/**
  * @brief  Get the base address of a flash bank
  * @param  bank  FLASH_BANK_1 or FLASH_BANK_2
  * @retval Base address of the bank
  */
static inline uint32_t Flash_GetBankBase(uint32_t bank)
{
    return (bank == FLASH_BANK_2) ? FLASH_BASE_BANK2 : FLASH_BASE_BANK1;
}

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_H__ */
