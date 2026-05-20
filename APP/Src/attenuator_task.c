/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    attenuator_task.c
  * @brief   Attenuator task — owns the "att" debug CLI command
  *
  *          The "att" command was previously a built-in in ThirdParty/Log.
  *          Extracted here so the Log layer stays generic and IO-dependent
  *          commands live at the application layer.
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
#include "attenuator_task.h"
#include "log.h"
#include "io.h"
#include <string.h>
#include <stdlib.h>

/* Private function prototypes -----------------------------------------------*/

static App_Status_t _attenuator_task_init(void);
static App_Status_t _attenuator_task_process(void);
static void         _attenuator_task_on_error(App_Status_t err);
static void         _dbg_cmd_att(int argc, char **argv);

/* Exported variables --------------------------------------------------------*/

const App_Module_t g_attenuator_task_module = {
    .name     = "Attenuator",
    .init     = _attenuator_task_init,
    .process  = _attenuator_task_process,
    .on_error = _attenuator_task_on_error,
};

/* Private functions ---------------------------------------------------------*/

static App_Status_t _attenuator_task_init(void)
{
    if (Log_RegisterDbgCmd("att", _dbg_cmd_att) != HAL_OK)
    {
        LOG_ERROR("Attenuator: failed to register 'att' debug command");
        return APP_ERROR;
    }

    LOG_INFO("Attenuator: initialized, type 'att' for usage");
    return APP_OK;
}

static App_Status_t _attenuator_task_process(void)
{
    return APP_OK;
}

static void _attenuator_task_on_error(App_Status_t err)
{
    LOG_ERROR("Attenuator: error (status=%d)", (int)err);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Debug CLI — "att" command                                                   */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
  * @brief  'att' command — control attenuators
  *
  *         Usage:
  *           att a <0-15>   — Set Attenuator A value
  *           att b <0-15>   — Set Attenuator B value
  *           att a get      — Get Attenuator A value
  *           att a ?        — Alias for att a get
  *           att b get      — Get Attenuator B value
  *           att b ?        — Alias for att b get
  *           att get        — Get both attenuator values
  *           att ?          — Alias for att get
  */
static void _dbg_cmd_att(int argc, char **argv)
{
    if (argc < 2)
    {
        LOG_INFO("Usage:");
        LOG_INFO("  att a <0-15>    Set Attenuator A");
        LOG_INFO("  att b <0-15>    Set Attenuator B");
        LOG_INFO("  att a get       Get Attenuator A");
        LOG_INFO("  att b get       Get Attenuator B");
        LOG_INFO("  att get/?       Get both");
        return;
    }

    /* ── Selector: a / b ────────────────────────────────────────────────── */
    int is_a = (strcmp(argv[1], "a") == 0);
    int is_b = (strcmp(argv[1], "b") == 0);

    /* ── att get / att ? ────────────────────────────────────────────────── */
    if (strcmp(argv[1], "get") == 0 || strcmp(argv[1], "?") == 0)
    {
        uint8_t val_a = IO_GetAttenuatorA();
        uint8_t val_b = IO_GetAttenuatorB();
        LOG_INFO("Attenuator A = %u  (0x%X)", (unsigned)val_a, (unsigned)val_a);
        LOG_INFO("Attenuator B = %u  (0x%X)", (unsigned)val_b, (unsigned)val_b);
        return;
    }

    /* ── att a get / att a ? / att b get / att b ? ──────────────────────── */
    if ((is_a || is_b) && argc >= 3)
    {
        if (strcmp(argv[2], "get") == 0 || strcmp(argv[2], "?") == 0)
        {
            if (is_a)
            {
                uint8_t val = IO_GetAttenuatorA();
                LOG_INFO("Attenuator A = %u  (0x%X)", (unsigned)val, (unsigned)val);
            }
            else
            {
                uint8_t val = IO_GetAttenuatorB();
                LOG_INFO("Attenuator B = %u  (0x%X)", (unsigned)val, (unsigned)val);
            }
            return;
        }
    }

    /* ── att a <val> / att b <val> ──────────────────────────────────────── */
    if (argc < 3)
    {
        LOG_INFO("Error: missing value. Usage: att %s <0-15>", argv[1]);
        return;
    }

    /* Validate that argv[2] is a pure numeric string using strtol end pointer */
    char *endptr = NULL;
    long value = strtol(argv[2], &endptr, 0);

    if (endptr == argv[2] || *endptr != '\0')
    {
        LOG_INFO("Error: invalid numeric value '%s'", argv[2]);
        return;
    }

    if (value < 0 || value > 15)
    {
        LOG_INFO("Error: value out of range (0-15): %ld", value);
        return;
    }

    if (is_a)
    {
        IO_SetAttenuatorA((uint8_t)value);
        LOG_INFO("Attenuator A set to %u", (unsigned)value);
    }
    else if (is_b)
    {
        IO_SetAttenuatorB((uint8_t)value);
        LOG_INFO("Attenuator B set to %u", (unsigned)value);
    }
    else
    {
        LOG_INFO("Error: unknown attenuator '%s'. Use 'a' or 'b'.", argv[1]);
    }
}
