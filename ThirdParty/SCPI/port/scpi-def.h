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

#ifndef __SCPI_DEF_H_
#define __SCPI_DEF_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "scpi/scpi.h"

/* Flash 配置存储 */
#include "flash.h"

#define SCPI_INPUT_BUFFER_LENGTH 256
#define SCPI_ERROR_QUEUE_SIZE 17
#define SCPI_IDN1 "GTS"
#define SCPI_IDN2 "PPA_SP10T"
#define SCPI_IDN3 "20260417"
#define SCPI_IDN4 "V0.0.1"

extern const scpi_command_t scpi_commands[];
extern scpi_interface_t scpi_interface;
extern char scpi_input_buffer[];
extern scpi_error_t scpi_error_queue_data[];
extern scpi_t scpi_context;

size_t SCPI_Write(scpi_t * context, const char * data, size_t len);
int SCPI_Error(scpi_t * context, int_fast16_t err);
scpi_result_t SCPI_Control(scpi_t * context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val);
scpi_result_t SCPI_Reset(scpi_t * context);
scpi_result_t SCPI_Flush(scpi_t * context);

/* SCPI USART1 接收接口 */
void     SCPI_RxPutChar(uint8_t ch);
uint8_t* SCPI_GetRxBuffer(void);
uint8_t  SCPI_IsRxReady(void);
void     SCPI_RxClearReady(void);
uint16_t SCPI_GetRxLength(void);

/* ========================================================================== */
/*              Flash 配置 SCPI 回调                                           */
/* ========================================================================== */

scpi_result_t SCPI_SystemConfigureIdentity(scpi_t *context);
scpi_result_t SCPI_SystemConfigureIdentityQ(scpi_t *context);
scpi_result_t SCPI_SystemConfigureName(scpi_t *context);
scpi_result_t SCPI_SystemConfigureNameQ(scpi_t *context);
scpi_result_t SCPI_SystemConfigureSerial(scpi_t *context);
scpi_result_t SCPI_SystemConfigureSerialQ(scpi_t *context);
scpi_result_t SCPI_SystemConfigureSave(scpi_t *context);
scpi_result_t SCPI_SystemConfigureDefaults(scpi_t *context);
scpi_result_t SCPI_SystemConfigureStatusQ(scpi_t *context);

#ifdef __cplusplus
}
#endif
#endif /* __SCPI_DEF_H_ */
