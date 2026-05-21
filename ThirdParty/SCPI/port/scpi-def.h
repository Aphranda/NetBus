/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    scpi-def.h
  * @brief   NetBus SCPI command definitions and port layer declarations
  *
  *          Architecture:
  *            - SCPI parser (ThirdParty/SCPI) runs as primary command interface
  *              on UART7, sharing the Log module's DMA RX pipeline.
  *            - When DIAGnostic:DEBUg ON is issued, unrecognized commands fall
  *              through to the legacy debug CLI parser.
  *            - TX output uses Log_Print() via the SCPI_Write() callback.
  *
  *          SCPI command tree:
  *            IEEE 488.2  → *IDN?, *RST, *CLS, *STB?, *WAI, *OPC?
  *            SYSTem      → ATTenuator, COMMunicate:CAN, DETector, ERRor
  *            SENSe       → DETector:CONTrol, STOP, TEMPerature, POWer, BAND
  *            SOURce      → DETector:FREQuency, POWer
  *            ROUTe       → SWITch (RFSW Modbus), DETector:SWITch
  *            STATus      → OPERation, QUEStionable
  *            DIAGnostic  → DEBUg, ECHO
  *
  *          Reference:
  *            Doc/SCPI_Commands.md — Full SCPI command reference
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 NetBus Project
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __SCPI_DEF_H_
#define __SCPI_DEF_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "scpi/scpi.h"

/* SCPI configuration */
#define SCPI_INPUT_BUFFER_LENGTH    256U
#define SCPI_ERROR_QUEUE_SIZE       17U
#define SCPI_IDN1                   "NetBus"
#define SCPI_IDN2                   "PPA-NB100"
#define SCPI_IDN3                   "00000000"
#define SCPI_IDN4                   "v1.0.0"

extern const scpi_command_t scpi_commands[];
extern scpi_interface_t     scpi_interface;
extern char                 scpi_input_buffer[];
extern scpi_error_t         scpi_error_queue_data[];
extern scpi_t               scpi_context;

/* ── Port layer (scpi_port.c) ─────────────────────────────────────────────── */

size_t        SCPI_Write(scpi_t *context, const char *data, size_t len);
int           SCPI_Error(scpi_t *context, int_fast16_t err);
scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val);
scpi_result_t SCPI_Reset(scpi_t *context);
scpi_result_t SCPI_Flush(scpi_t *context);

/* ── SCPI lifecycle ───────────────────────────────────────────────────────── */

/**
  * @brief  Initialize the SCPI subsystem — call once after Log_Init()
  */
void SCPI_SystemInit(void);

/**
  * @brief  Try to parse a command line as SCPI
  * @param  line  Null-terminated command string (without line terminators)
  * @retval TRUE if SCPI recognized and handled the command
  * @retval FALSE if the command was not recognized
  */
scpi_bool_t SCPI_TryParse(const char *line);

/* ── IEEE 488.2 common commands ───────────────────────────────────────────── */

scpi_result_t SCPI_CoreIdnQ(scpi_t *context);
scpi_result_t SCPI_CoreRst(scpi_t *context);
scpi_result_t SCPI_CoreCls(scpi_t *context);
scpi_result_t SCPI_CoreStbQ(scpi_t *context);
scpi_result_t SCPI_CoreWai(scpi_t *context);
scpi_result_t SCPI_CoreOpcQ(scpi_t *context);

/* ── SYSTem subsystem ─────────────────────────────────────────────────────── */

/* ATTenuator */
scpi_result_t SCPI_SystemAttA(scpi_t *context);
scpi_result_t SCPI_SystemAttAQ(scpi_t *context);
scpi_result_t SCPI_SystemAttB(scpi_t *context);
scpi_result_t SCPI_SystemAttBQ(scpi_t *context);

/* COMMunicate:CAN */
scpi_result_t SCPI_SystemCommCanSend(scpi_t *context);
scpi_result_t SCPI_SystemCommCanScanQ(scpi_t *context);

/* DETector node commands */
scpi_result_t SCPI_SystemDetSnQ(scpi_t *context);
scpi_result_t SCPI_SystemDetVersionQ(scpi_t *context);
scpi_result_t SCPI_SystemDetReset(scpi_t *context);
scpi_result_t SCPI_SystemDetLed(scpi_t *context);
scpi_result_t SCPI_SystemDetAddress(scpi_t *context);
scpi_result_t SCPI_SystemDetFlashInfoQ(scpi_t *context);
scpi_result_t SCPI_SystemDetFlashDataQ(scpi_t *context);

/* ERRor */
scpi_result_t SCPI_SystemErrorNextQ(scpi_t *context);
scpi_result_t SCPI_SystemErrorCountQ(scpi_t *context);

/* ── SENSe subsystem ─────────────────────────────────────────────────────── */

scpi_result_t SCPI_SenseDetControl(scpi_t *context);
scpi_result_t SCPI_SenseDetStop(scpi_t *context);
scpi_result_t SCPI_SenseDetTempQ(scpi_t *context);
scpi_result_t SCPI_SenseDetPowerQ(scpi_t *context);
scpi_result_t SCPI_SenseDetBand(scpi_t *context);

/* ── SOURce subsystem ─────────────────────────────────────────────────────── */

scpi_result_t SCPI_SourceDetFreq(scpi_t *context);
scpi_result_t SCPI_SourceDetPower(scpi_t *context);

/* ── ROUTe subsystem ──────────────────────────────────────────────────────── */

/* ROUTe:DETector:SWITch */
scpi_result_t SCPI_RouteDetSwitch(scpi_t *context);

/* ROUTe:SWITch:<addr> (RFSW via Modbus) */
scpi_result_t SCPI_RouteSwitchChannelQ(scpi_t *context);
scpi_result_t SCPI_RouteSwitchChannel(scpi_t *context);
scpi_result_t SCPI_RouteSwitchModeQ(scpi_t *context);
scpi_result_t SCPI_RouteSwitchMode(scpi_t *context);
scpi_result_t SCPI_RouteSwitchIdentityQ(scpi_t *context);
scpi_result_t SCPI_RouteSwitchOutputQ(scpi_t *context);
scpi_result_t SCPI_RouteSwitchOutput(scpi_t *context);
scpi_result_t SCPI_RouteSwitchInputQ(scpi_t *context);
scpi_result_t SCPI_RouteSwitchConditionQ(scpi_t *context);
scpi_result_t SCPI_RouteSwitchAddress(scpi_t *context);

/* ── STATus subsystem ─────────────────────────────────────────────────────── */

scpi_result_t SCPI_StatusOperationEventQ(scpi_t *context);
scpi_result_t SCPI_StatusQuestionableEventQ(scpi_t *context);

/* ── DIAGnostic subsystem ─────────────────────────────────────────────────── */

scpi_result_t SCPI_DiagDebug(scpi_t *context);
scpi_result_t SCPI_DiagDebugQ(scpi_t *context);
scpi_result_t SCPI_DiagEcho(scpi_t *context);
scpi_result_t SCPI_DiagEchoQ(scpi_t *context);

#ifdef __cplusplus
}
#endif
#endif /* __SCPI_DEF_H_ */
