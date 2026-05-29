/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    storage_task.c
  * @brief   Non-volatile flash storage module implementation
  *
  *          Architecture:
  *            - Based on ThirdParty/Flash base library (flash.c/h)
  *            - Append-only log in Bank 2 Sector 7 (0x081E0000, 128KB)
  *            - RAM cache with dirty flag; SCPI MEMory commands drive flash I/O
  *            - Records are 64 bytes (2 flashwords); CRC16-CCITT protects value
  *            - Header is CRC-protected for power-loss resilience
  *            - process() is a no-op -- all operations are SCPI-initiated
  *
  *          Layer diagram:
  *            SCPI layer (scpi-def.c)
  *              → Storage API (storage_task.c)
  *                → Flash base library (ThirdParty/Flash)
  *                  → HAL FLASH Driver
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "storage_task.h"
#include "flash.h"
#include "log.h"
#include <string.h>

/* Private typedef -----------------------------------------------------------*/

typedef struct {
    char            key[STORAGE_MAX_KEY_LEN];
    Storage_Type_t  type;
    uint8_t         value[STORAGE_MAX_VALUE_LEN];
    uint16_t        data_len;
    uint8_t         dirty;
    uint8_t         valid;
} Storage_CacheEntry_t;

/* Private variables ---------------------------------------------------------*/

static Storage_CacheEntry_t  g_cache[STORAGE_MAX_PARAMS];
static uint32_t              g_write_offset   = STORAGE_FLASH_ADDR + STORAGE_HEADER_SIZE;
static uint32_t              g_write_counter  = 0U;
static uint32_t              g_record_count   = 0U;
static uint8_t               g_initialized    = 0U;

/* Private function prototypes -----------------------------------------------*/

static App_Status_t _storage_task_init(void);
static App_Status_t _storage_task_process(void);
static void         _storage_task_on_error(App_Status_t err);

/* Format detection and scanning */
static uint8_t       _is_formatted(void);
static App_Status_t  _scan_flash(void);
static App_Status_t  _read_header(Storage_Header_t *hdr);

/* Flash write helpers */
static App_Status_t  _write_header(void);
static App_Status_t  _write_record(uint32_t addr, const Storage_Record_t *rec);

/* RAM cache */
static int16_t       _cache_find(const char *key);
static int16_t       _cache_find_free(void);
static void          _cache_clear(void);

/* CRC */
static uint16_t      _crc16_ccitt(const uint8_t *data, uint16_t len);

/* Exported variables --------------------------------------------------------*/

const App_Module_t g_storage_task_module = {
    .name     = "Storage",
    .init     = _storage_task_init,
    .process  = _storage_task_process,
    .on_error = _storage_task_on_error,
};

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Module Interface                                                           */
/* ═══════════════════════════════════════════════════════════════════════════ */

static App_Status_t _storage_task_init(void)
{
    memset(g_cache, 0, sizeof(g_cache));

    /* Do NOT read flash during boot — unprogrammed sectors can cause
       ECC bus faults on STM32H7. Storage starts in "not formatted" state.
       Flash is first accessed on explicit Storage_Load() or Storage_Commit(). */
    g_write_offset  = STORAGE_FLASH_ADDR + STORAGE_HEADER_SIZE;
    g_write_counter = 0U;
    g_record_count  = 0U;
    g_initialized   = 1U;

    LOG_INFO("Storage: initialized (deferred flash access)");

    return APP_OK;
}

static App_Status_t _storage_task_process(void)
{
    return APP_OK;
}

static void _storage_task_on_error(App_Status_t err)
{
    LOG_ERROR("Storage: error %d", (int)err);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Format Detection & Scanning                                                */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
  * @brief  Check if the storage sector has a valid format header
  */
static uint8_t _is_formatted(void)
{
    Storage_Header_t hdr;
    if (_read_header(&hdr) != APP_OK)
        return 0U;

    uint16_t computed = _crc16_ccitt((const uint8_t *)&hdr, 14U);
    return (computed == hdr.header_crc16) ? 1U : 0U;
}

/**
  * @brief  Read and validate the sector header (32 bytes from flash)
  */
static App_Status_t _read_header(Storage_Header_t *hdr)
{
    if (hdr == NULL) return APP_INVALID;

    /* Direct memory-mapped read -- 32 bytes, 32-byte aligned */
    const uint32_t *src = (const uint32_t *)STORAGE_FLASH_ADDR;
    uint32_t *dst = (uint32_t *)hdr;
    for (uint8_t i = 0U; i < 8U; i++)
        dst[i] = src[i];

    if (memcmp(hdr->magic, "NBSFv1", 6U) != 0)
        return APP_ERROR;

    return APP_OK;
}

/**
  * @brief  Scan all flash records and populate the RAM cache
  * @note   Stops at first record with status == 0xFF (erased) or CRC mismatch.
  *         Flash is memory-mapped so reads are direct pointer dereferences.
  */
static App_Status_t _scan_flash(void)
{
    _cache_clear();

    uint32_t addr = STORAGE_FLASH_ADDR + STORAGE_RECORDS_START_OFFSET;
    uint32_t records_scanned = 0U;
    uint32_t active_count    = 0U;

    while (addr < (STORAGE_FLASH_ADDR + STORAGE_SECTOR_SIZE - STORAGE_RECORD_SIZE))
    {
        uint8_t status = *(volatile uint8_t *)(addr + 0x10U);

        if (status == STORAGE_STATUS_ERASED)
            break;

        records_scanned++;

        if (status == STORAGE_STATUS_ACTIVE)
        {
            const Storage_Record_t *rec = (const Storage_Record_t *)addr;

            uint16_t computed = _crc16_ccitt(rec->value_data, STORAGE_MAX_VALUE_LEN);
            if (computed != rec->crc16)
            {
                LOG_WARN("Storage: CRC mismatch at 0x%08lX, stopping scan",
                         (unsigned long)addr);
                break;
            }

            int16_t slot = _cache_find_free();
            if (slot < 0)
            {
                LOG_WARN("Storage: cache full at record %lu, stopping scan",
                         (unsigned long)records_scanned);
                break;
            }

            memcpy(g_cache[slot].key, rec->key, STORAGE_MAX_KEY_LEN);
            g_cache[slot].key[STORAGE_MAX_KEY_LEN - 1] = '\0';
            g_cache[slot].type     = (Storage_Type_t)rec->data_type;
            g_cache[slot].data_len = (rec->data_len <= STORAGE_MAX_VALUE_LEN)
                                     ? rec->data_len : STORAGE_MAX_VALUE_LEN;
            memcpy(g_cache[slot].value, rec->value_data, g_cache[slot].data_len);
            g_cache[slot].dirty = 0U;
            g_cache[slot].valid = 1U;
            active_count++;
        }
        /* status == STORAGE_STATUS_DELETED: tombstone, skip */

        addr += STORAGE_RECORD_SIZE;
    }

    g_write_offset = addr;
    g_record_count = active_count;

    LOG_INFO("Storage: scanned %lu records, %lu active, next @ 0x%08lX",
             (unsigned long)records_scanned, (unsigned long)active_count,
             (unsigned long)g_write_offset);

    return APP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Flash Write Helpers (uses Flash base library)                              */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
  * @brief  Write sector header with CRC16 protection (1 flashword = 32 bytes)
  */
static App_Status_t _write_header(void)
{
    Storage_Header_t hdr;
    memset(&hdr, 0, sizeof(hdr));

    memcpy(hdr.magic, "NBSFv1", 6U);
    hdr.magic[6] = '\0';
    hdr.magic[7] = '\0';
    hdr.write_counter = g_write_counter + 1U;
    hdr.record_count  = g_record_count;
    hdr.header_crc16  = _crc16_ccitt((const uint8_t *)&hdr, 14U);

    HAL_StatusTypeDef status = Flash_ProgramWord(STORAGE_FLASH_ADDR,
                                                  (const uint32_t *)&hdr);
    if (status != HAL_OK)
    {
        LOG_ERROR("Storage: header write failed (HAL %d)", (int)status);
        return APP_ERROR;
    }

    g_write_counter++;
    return APP_OK;
}

/**
  * @brief  Write a 64-byte record as two flashwords
  */
static App_Status_t _write_record(uint32_t addr, const Storage_Record_t *rec)
{
    /* 2 flashwords × 32 bytes = 64 bytes */
    HAL_StatusTypeDef status = Flash_Program(addr, (const uint32_t *)rec, 2U);
    if (status != HAL_OK)
    {
        LOG_ERROR("Storage: record write failed at 0x%08lX (HAL %d)",
                  (unsigned long)addr, (int)status);
        return APP_ERROR;
    }
    return APP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  RAM Cache                                                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */

static int16_t _cache_find(const char *key)
{
    if (key == NULL) return -1;
    for (int16_t i = 0; i < (int16_t)STORAGE_MAX_PARAMS; i++)
    {
        if (g_cache[i].valid && strcmp(g_cache[i].key, key) == 0)
            return i;
    }
    return -1;
}

static int16_t _cache_find_free(void)
{
    for (int16_t i = 0; i < (int16_t)STORAGE_MAX_PARAMS; i++)
    {
        if (!g_cache[i].valid)
            return i;
    }
    return -1;
}

static void _cache_clear(void)
{
    for (uint16_t i = 0U; i < STORAGE_MAX_PARAMS; i++)
    {
        g_cache[i].valid = 0U;
        g_cache[i].dirty = 0U;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  CRC16-CCITT                                                                */
/* ═══════════════════════════════════════════════════════════════════════════ */

static uint16_t _crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    for (uint16_t i = 0U; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0U; j < 8U; j++)
        {
            if (crc & 0x8000U)
                crc = (crc << 1) ^ 0x1021U;
            else
                crc = crc << 1;
        }
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Public API                                                                 */
/* ═══════════════════════════════════════════════════════════════════════════ */

App_Status_t Storage_Init(void)
{
    return _storage_task_init();
}

App_Status_t Storage_Commit(void)
{
    if (!g_initialized)
    {
        LOG_ERROR("Storage: not initialized");
        return APP_ERROR;
    }

    uint32_t dirty_count = 0U;
    for (uint16_t i = 0U; i < STORAGE_MAX_PARAMS; i++)
    {
        if (g_cache[i].valid && g_cache[i].dirty)
            dirty_count++;
    }

    if (dirty_count == 0U)
        return APP_OK;

    uint32_t needed = dirty_count * STORAGE_RECORD_SIZE + STORAGE_HEADER_SIZE;
    if (Storage_FreeSpace() < needed)
    {
        LOG_ERROR("Storage: insufficient space (need %lu, have %lu)",
                  (unsigned long)needed, (unsigned long)Storage_FreeSpace());
        return APP_ERROR;
    }

    LOG_INFO("Storage: committing %lu dirty records", (unsigned long)dirty_count);

    for (uint16_t i = 0U; i < STORAGE_MAX_PARAMS; i++)
    {
        if (!g_cache[i].valid || !g_cache[i].dirty)
            continue;

        Storage_Record_t rec;
        memset(&rec, 0, sizeof(rec));

        strncpy(rec.key, g_cache[i].key, STORAGE_MAX_KEY_LEN - 1);
        rec.key[STORAGE_MAX_KEY_LEN - 1] = '\0';
        rec.status    = STORAGE_STATUS_ACTIVE;
        rec.data_type = (uint8_t)g_cache[i].type;
        rec.data_len  = g_cache[i].data_len;
        memcpy(rec.value_data, g_cache[i].value, g_cache[i].data_len);
        rec.crc16 = _crc16_ccitt(rec.value_data, STORAGE_MAX_VALUE_LEN);

        if (_write_record(g_write_offset, &rec) != APP_OK)
            return APP_ERROR;

        g_write_offset += STORAGE_RECORD_SIZE;
        g_record_count++;
        g_cache[i].dirty = 0U;
    }

    if (_write_header() != APP_OK)
        return APP_ERROR;

    LOG_INFO("Storage: commit complete (%lu records, %lu writes)",
             (unsigned long)g_record_count, (unsigned long)g_write_counter);

    return APP_OK;
}

App_Status_t Storage_Load(void)
{
    if (!g_initialized)
    {
        LOG_ERROR("Storage: not initialized");
        return APP_ERROR;
    }

    if (!_is_formatted())
    {
        LOG_WARN("Storage: flash not formatted, nothing to load");
        return APP_ERROR;
    }

    _cache_clear();
    g_write_offset = STORAGE_FLASH_ADDR + STORAGE_HEADER_SIZE;
    g_record_count = 0U;

    Storage_Header_t hdr;
    if (_read_header(&hdr) == APP_OK)
        g_write_counter = hdr.write_counter;

    _scan_flash();

    LOG_INFO("Storage: reloaded %lu records from flash", (unsigned long)g_record_count);
    return APP_OK;
}

App_Status_t Storage_Initialize(void)
{
    HAL_StatusTypeDef hal_st = Flash_EraseSector(STORAGE_FLASH_SECTOR, 1U);
    if (hal_st != HAL_OK)
    {
        LOG_ERROR("Storage: sector erase failed (HAL %d)", (int)hal_st);
        return APP_ERROR;
    }

    _cache_clear();
    g_write_offset  = STORAGE_FLASH_ADDR + STORAGE_HEADER_SIZE;
    g_write_counter = 0U;
    g_record_count  = 0U;
    g_initialized   = 1U;

    if (_write_header() != APP_OK)
        return APP_ERROR;

    LOG_INFO("Storage: sector erased, factory reset complete");
    return APP_OK;
}

App_Status_t Storage_Set(const char *key, Storage_Type_t type,
                         const void *value, uint16_t data_len)
{
    if (!g_initialized) return APP_ERROR;
    if (key == NULL || value == NULL) return APP_INVALID;
    if (data_len > STORAGE_MAX_VALUE_LEN) return APP_INVALID;

    size_t key_len = strlen(key);
    if (key_len >= STORAGE_MAX_KEY_LEN) return APP_INVALID;

    int16_t slot = _cache_find(key);
    if (slot < 0)
    {
        slot = _cache_find_free();
        if (slot < 0)
        {
            LOG_ERROR("Storage: cache full (%u entries)", (unsigned)STORAGE_MAX_PARAMS);
            return APP_ERROR;
        }
        strncpy(g_cache[slot].key, key, STORAGE_MAX_KEY_LEN - 1);
        g_cache[slot].key[STORAGE_MAX_KEY_LEN - 1] = '\0';
        g_cache[slot].valid = 1U;
    }

    g_cache[slot].type     = type;
    g_cache[slot].data_len = data_len;
    memcpy(g_cache[slot].value, value, data_len);
    g_cache[slot].dirty = 1U;

    return APP_OK;
}

App_Status_t Storage_Get(const char *key, Storage_Type_t *type,
                         void *value, uint16_t *data_len)
{
    if (!g_initialized) return APP_ERROR;
    if (key == NULL || value == NULL) return APP_INVALID;

    int16_t slot = _cache_find(key);
    if (slot < 0) return APP_ERROR;

    if (type != NULL)      *type     = g_cache[slot].type;
    if (data_len != NULL)  *data_len = g_cache[slot].data_len;
    memcpy(value, g_cache[slot].value, g_cache[slot].data_len);

    return APP_OK;
}

App_Status_t Storage_Delete(const char *key)
{
    if (!g_initialized) return APP_ERROR;
    if (key == NULL) return APP_INVALID;

    int16_t slot = _cache_find(key);
    if (slot < 0) return APP_ERROR;

    g_cache[slot].valid = 0U;
    g_cache[slot].dirty = 0U;
    return APP_OK;
}

App_Status_t Storage_List(char *buffer, size_t buf_size)
{
    if (!g_initialized) return APP_ERROR;
    if (buffer == NULL || buf_size == 0U) return APP_INVALID;

    buffer[0] = '\0';
    size_t pos = 0U;
    uint8_t first = 1U;

    for (uint16_t i = 0U; i < STORAGE_MAX_PARAMS; i++)
    {
        if (!g_cache[i].valid) continue;

        size_t key_len = strlen(g_cache[i].key);
        size_t needed  = key_len + 2U;

        if (pos + needed >= buf_size) break;

        if (!first) buffer[pos++] = ',';
        first = 0U;

        memcpy(&buffer[pos], g_cache[i].key, key_len);
        pos += key_len;
    }
    buffer[pos] = '\0';

    return APP_OK;
}

uint32_t Storage_FreeSpace(void)
{
    uint32_t used = g_write_offset - STORAGE_FLASH_ADDR;
    if (used >= STORAGE_SECTOR_SIZE) return 0U;
    return STORAGE_SECTOR_SIZE - used;
}

uint32_t Storage_GetWriteCount(void)
{
    return g_write_counter;
}

uint32_t Storage_GetRecordCount(void)
{
    return g_record_count;
}
