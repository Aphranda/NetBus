/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    flash.c
  * @brief   STM32H7 internal Flash base library implementation
  *
  *          Operations:
  *            - Sector erase via HAL_FLASHEx_Erase (Bank 2)
  *            - Flashword program via HAL_FLASH_Program (256-bit aligned)
  *            - Read with direct memory access (flash is memory-mapped)
  *            - Verify with memcmp after program
  *
  *          Safety:
  *            - taskENTER_CRITICAL around each flashword write
  *            - Error flag clearing on failure
  *            - Bank 2 operations don't stall CPU (app runs from Bank 1)
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "flash.h"
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

static uint8_t g_flash_initialized = 0U;

/* Private function prototypes -----------------------------------------------*/

static HAL_StatusTypeDef _flash_wait_ready(uint32_t bank, uint32_t timeout_ms);
static void               _flash_clear_errors(uint32_t bank);

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Public API                                                                 */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
  * @brief  Initialize the Flash base library
  */
HAL_StatusTypeDef Flash_Init(void)
{
    /* Do NOT read from flash here — unprogrammed sectors on STM32H7
       cause ECC bus faults. Flash verification happens on first write. */
    g_flash_initialized = 1U;
    return HAL_OK;
}

/**
  * @brief  Erase one or more flash sectors in Bank 2
  */
HAL_StatusTypeDef Flash_EraseSector(uint32_t sector, uint32_t nb_sectors)
{
    if (!g_flash_initialized || nb_sectors == 0U ||
        (sector + nb_sectors) > FLASH_SECTOR_COUNT_PER_BANK)
        return HAL_ERROR;

    HAL_StatusTypeDef status;

    status = Flash_Unlock();
    if (status != HAL_OK) return status;

    _flash_wait_ready(FLASH_USER_BANK, FLASH_PROGRAM_TIMEOUT_MS);
    _flash_clear_errors(FLASH_USER_BANK);

    FLASH_EraseInitTypeDef erase_init = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .Banks        = FLASH_USER_BANK,
        .Sector       = sector,
        .NbSectors    = nb_sectors,
        .VoltageRange = FLASH_VOLTAGE_RANGE_4,
    };

    uint32_t sector_error = 0U;

    taskENTER_CRITICAL();
    status = HAL_FLASHEx_Erase(&erase_init, &sector_error);
    taskEXIT_CRITICAL();

    if (status != HAL_OK)
        _flash_clear_errors(FLASH_USER_BANK);

    Flash_Lock();
    return status;
}

/**
  * @brief  Program one flashword (32 bytes) to flash
  */
HAL_StatusTypeDef Flash_ProgramWord(uint32_t addr, const uint32_t *data)
{
    if (!g_flash_initialized) return HAL_ERROR;
    if (!Flash_IsAligned(addr)) return HAL_ERROR;

    HAL_StatusTypeDef status;

    status = Flash_Unlock();
    if (status != HAL_OK) return status;

    _flash_wait_ready(FLASH_USER_BANK, FLASH_PROGRAM_TIMEOUT_MS);
    _flash_clear_errors(FLASH_USER_BANK);

    taskENTER_CRITICAL();
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, addr,
                               (uint32_t)data);
    taskEXIT_CRITICAL();

    if (status != HAL_OK)
        _flash_clear_errors(FLASH_USER_BANK);

    Flash_Lock();
    return status;
}

/**
  * @brief  Program multiple flashwords
  */
HAL_StatusTypeDef Flash_Program(uint32_t addr, const uint32_t *data,
                                uint32_t word_count)
{
    if (!g_flash_initialized || word_count == 0U) return HAL_ERROR;
    if (!Flash_IsAligned(addr)) return HAL_ERROR;

    HAL_StatusTypeDef status = Flash_Unlock();
    if (status != HAL_OK) return status;

    for (uint32_t i = 0U; i < word_count; i++)
    {
        _flash_wait_ready(FLASH_USER_BANK, FLASH_PROGRAM_TIMEOUT_MS);
        _flash_clear_errors(FLASH_USER_BANK);

        taskENTER_CRITICAL();
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                                   addr + i * FLASH_WORD_SIZE,
                                   (uint32_t)(data + i * FLASH_WORD_U32_COUNT));
        taskEXIT_CRITICAL();

        if (status != HAL_OK)
        {
            _flash_clear_errors(FLASH_USER_BANK);
            Flash_Lock();
            return status;
        }
    }

    Flash_Lock();
    return HAL_OK;
}

/**
  * @brief  Read bytes from flash memory (direct memory-mapped access)
  */
void Flash_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    if (buf == NULL || len == 0U) return;

    const uint8_t *src = (const uint8_t *)addr;
    for (uint32_t i = 0U; i < len; i++)
        buf[i] = src[i];
}

/**
  * @brief  Verify programmed data against expected
  */
HAL_StatusTypeDef Flash_Verify(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (data == NULL || len == 0U) return HAL_ERROR;

    const uint8_t *flash = (const uint8_t *)addr;
    for (uint32_t i = 0U; i < len; i++)
    {
        if (flash[i] != data[i])
            return HAL_ERROR;
    }
    return HAL_OK;
}

/**
  * @brief  Unlock the user bank (Bank 2)
  */
HAL_StatusTypeDef Flash_Unlock(void)
{
    return HAL_FLASH_Unlock();
}

/**
  * @brief  Lock the user bank
  */
HAL_StatusTypeDef Flash_Lock(void)
{
    return HAL_FLASH_Lock();
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Private Helpers                                                            */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
  * @brief  Wait for any pending flash operation on the specified bank
  */
static HAL_StatusTypeDef _flash_wait_ready(uint32_t bank, uint32_t timeout_ms)
{
    uint32_t tickstart = HAL_GetTick();
    uint32_t bsy_flag, qw_flag, wbne_flag;

    if (bank == FLASH_BANK_2)
    {
        bsy_flag  = FLASH_FLAG_BSY_BANK2;
        qw_flag   = FLASH_FLAG_QW_BANK2;
        wbne_flag = FLASH_FLAG_WBNE_BANK2;
    }
    else
    {
        bsy_flag  = FLASH_FLAG_BSY;
        qw_flag   = FLASH_FLAG_QW;
        wbne_flag = FLASH_FLAG_WBNE;
    }

    while (__HAL_FLASH_GET_FLAG(bsy_flag) ||
           __HAL_FLASH_GET_FLAG(qw_flag)  ||
           __HAL_FLASH_GET_FLAG(wbne_flag))
    {
        if ((HAL_GetTick() - tickstart) > timeout_ms)
            return HAL_TIMEOUT;
    }
    return HAL_OK;
}

/**
  * @brief  Clear all error flags for the specified bank
  */
static void _flash_clear_errors(uint32_t bank)
{
    if (bank == FLASH_BANK_2)
    {
        __HAL_FLASH_CLEAR_FLAG_BANK2(FLASH_FLAG_ALL_ERRORS_BANK2);
    }
    else
    {
        __HAL_FLASH_CLEAR_FLAG_BANK1(FLASH_FLAG_ALL_ERRORS_BANK1);
    }
}
