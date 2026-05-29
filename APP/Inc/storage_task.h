/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    storage_task.h
  * @brief   Non-volatile flash storage module -- key-value parameter store
  *
  *          Responsibilities:
  *            - Provide persistent key-value storage in Bank 2 Sector 7
  *            - Append-only log structure with CRC16 integrity
  *            - Power-loss-safe header with double CRC protection
  *            - RAM cache with dirty tracking; SCPI-driven flash commit
  *
  *          Usage:
  *            1. Register module via App_RegisterModule(&g_storage_task_module)
  *            2. Use SCPI MEMory commands to read/write parameters
  *            3. Call Storage_Set/Get API from other modules as needed
  *
  *          Flash Layout (0x081E0000, 128KB):
  *            [Header 32B] [Record 64B] [Record 64B] ... [Record 64B]
  *            Records are appended; deletes write tombstones.
  *            Max ~2046 records per sector.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __STORAGE_TASK_H__
#define __STORAGE_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "app_task.h"

/* Exported constants --------------------------------------------------------*/

#define STORAGE_FLASH_ADDR            0x081E0000U   /* Bank 2, Sector 7       */
#define STORAGE_FLASH_SECTOR          7U            /* Sector index (7)       */
#define STORAGE_SECTOR_SIZE           0x20000U      /* 128KB                  */
#define STORAGE_MAX_PARAMS            200U          /* Max RAM-cached entries */
#define STORAGE_MAX_KEY_LEN           16U           /* Null-terminated key    */
#define STORAGE_MAX_VALUE_LEN         32U           /* Max string/blob value  */
#define STORAGE_RECORD_SIZE           64U           /* 32-byte aligned        */
#define STORAGE_HEADER_SIZE           32U           /* Offset before records  */
#define STORAGE_RECORDS_START_OFFSET  0x0020U       /* First record at +0x20  */
#define STORAGE_MAGIC_LEN             8U

/* Record status values (written to flash -- must match erased 0xFF) */
#define STORAGE_STATUS_ERASED         0xFFU
#define STORAGE_STATUS_ACTIVE         0x01U
#define STORAGE_STATUS_DELETED        0x00U

/* Exported types ------------------------------------------------------------*/

typedef enum {
    STORAGE_TYPE_U32   = 0,
    STORAGE_TYPE_I32   = 1,
    STORAGE_TYPE_FLOAT = 2,
    STORAGE_TYPE_STR   = 3,
    STORAGE_TYPE_BLOB  = 4,
} Storage_Type_t;

/**
  * @brief  On-flash record layout -- 64 bytes, 32-byte aligned for flashword writes
  */
typedef struct __attribute__((packed, aligned(32))) {
    char     key[STORAGE_MAX_KEY_LEN];      /* 0x00: null-terminated key    */
    uint8_t  status;                         /* 0x10: see STORAGE_STATUS_*  */
    uint8_t  data_type;                      /* 0x11: Storage_Type_t         */
    uint16_t crc16;                          /* 0x12: CRC16 of value_data    */
    uint16_t data_len;                       /* 0x14: valid bytes in value   */
    uint16_t reserved1;                      /* 0x16: padding                */
    uint8_t  value_data[STORAGE_MAX_VALUE_LEN]; /* 0x18: payload             */
    uint8_t  reserved2[8];                   /* 0x38: padding to 64B         */
} Storage_Record_t;

/* Compile-time check: record must be exactly 64 bytes */
typedef char _storage_record_size_check[sizeof(Storage_Record_t) == 64U ? 1 : -1];

/**
  * @brief  Sector header -- first 32 bytes of the flash sector
  */
typedef struct __attribute__((packed, aligned(32))) {
    char     magic[STORAGE_MAGIC_LEN];       /* "NBSFv1\0\0"                */
    uint32_t write_counter;                  /* incremented each commit      */
    uint32_t record_count;                   /* active records in flash      */
    uint16_t header_crc16;                   /* CRC over bytes 0..13         */
    uint8_t  reserved[14];                   /* padding to 32 bytes          */
} Storage_Header_t;

/* Exported variables --------------------------------------------------------*/

extern const App_Module_t g_storage_task_module;

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize storage -- detect flash state, load cache
  * @retval APP_OK on success
  */
App_Status_t Storage_Init(void);

/**
  * @brief  Commit all dirty cache entries to flash (append-only)
  * @note   Blocks ~N*200us while writing records. Call from SCPI context.
  * @retval APP_OK on success
  */
App_Status_t Storage_Commit(void);

/**
  * @brief  Reload cache from flash (discard unsaved dirty entries)
  * @retval APP_OK on success
  */
App_Status_t Storage_Load(void);

/**
  * @brief  Erase storage sector (factory reset)
  * @note   Blocks ~2s while erasing 128KB. Prompts confirmation needed.
  * @retval APP_OK on success
  */
App_Status_t Storage_Initialize(void);

/**
  * @brief  Set a parameter value in RAM cache (does NOT write flash)
  * @param  key       Null-terminated key name (max 15 chars)
  * @param  type      Data type (U32, I32, FLOAT, STR, BLOB)
  * @param  value     Pointer to value data
  * @param  data_len  Size in bytes (max 32)
  * @retval APP_OK on success
  */
App_Status_t Storage_Set(const char *key, Storage_Type_t type,
                         const void *value, uint16_t data_len);

/**
  * @brief  Get a parameter value from RAM cache
  * @param  key       Null-terminated key name
  * @param  type      Output: data type
  * @param  value     Output: buffer for value (caller-provided, min 32 bytes)
  * @param  data_len  Output: actual data length
  * @retval APP_OK on success, APP_ERROR if not found
  */
App_Status_t Storage_Get(const char *key, Storage_Type_t *type,
                         void *value, uint16_t *data_len);

/**
  * @brief  Delete a parameter (marks for tombstone on next commit)
  * @param  key  Null-terminated key name
  * @retval APP_OK on success, APP_ERROR if not found
  */
App_Status_t Storage_Delete(const char *key);

/**
  * @brief  List all active keys as comma-separated string
  * @param  buffer    Output buffer
  * @param  buf_size  Buffer capacity
  * @retval APP_OK on success
  */
App_Status_t Storage_List(char *buffer, size_t buf_size);

/**
  * @brief  Query remaining free bytes in the flash sector
  * @retval Free bytes available for new records
  */
uint32_t Storage_FreeSpace(void);

/**
  * @brief  Get write counter from flash header
  * @retval Write counter value
  */
uint32_t Storage_GetWriteCount(void);

/**
  * @brief  Get number of active records
  * @retval Active record count
  */
uint32_t Storage_GetRecordCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __STORAGE_TASK_H__ */
