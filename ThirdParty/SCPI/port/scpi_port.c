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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scpi/scpi.h"
#include "scpi-def.h"
#include "usart.h"
#include "debug.h"


extern size_t usb_buffer_len;
extern char usb_buffer[225];

/* SCPI RX buffer (for USART1 interrupt collection) */
#define SCPI_RX_BUFFER_SIZE     256
static volatile uint8_t  scpi_rx_buffer[SCPI_RX_BUFFER_SIZE];
static volatile uint16_t scpi_rx_index = 0;
static volatile uint8_t  scpi_rx_ready = 0;  /* 完整命令行已就绪 */

#define SCPI_USE_SERIAL
//#define SCPI_USE_USBTMC

/**
 * @brief 将 USART1 接收到的字节送入 SCPI 行缓冲区
 * @param ch 接收到的字符
 * @note 由 HAL_UART_RxCpltCallback 在中断中调用
 */
void SCPI_RxPutChar(uint8_t ch)
{
    if (ch == '\r' || ch == '\n')
    {
        if (scpi_rx_index > 0)
        {
            /* 将 \r\n 也写入缓冲区，让 SCPI 词法分析器能检测到行终止符 */
            if (scpi_rx_index < (SCPI_RX_BUFFER_SIZE - 2))
            {
                scpi_rx_buffer[scpi_rx_index++] = '\r';
                scpi_rx_buffer[scpi_rx_index++] = '\n';
            }
            scpi_rx_buffer[scpi_rx_index] = '\0';
            scpi_rx_ready = 1;
            scpi_rx_index = 0;
        }
        return;
    }

    /* 退格处理 */
    if (ch == '\b' || ch == 0x7F)
    {
        if (scpi_rx_index > 0)
            scpi_rx_index--;
        return;
    }

    /* 普通字符 */
    if (scpi_rx_index < (SCPI_RX_BUFFER_SIZE - 1))
    {
        scpi_rx_buffer[scpi_rx_index++] = ch;
    }
}

/**
 * @brief 获取 SCPI 接收缓冲区指针
 */
uint8_t* SCPI_GetRxBuffer(void)
{
    return (uint8_t *)scpi_rx_buffer;
}

/**
 * @brief 检查是否有完整 SCPI 命令就绪
 */
uint8_t SCPI_IsRxReady(void)
{
    return scpi_rx_ready;
}

/**
 * @brief 清除就绪标志（在 SCPITask 处理完后调用）
 */
void SCPI_RxClearReady(void)
{
    scpi_rx_ready = 0;
}

/**
 * @brief 获取 SCPI 命令长度
 */
uint16_t SCPI_GetRxLength(void)
{
    return strlen((const char *)scpi_rx_buffer);
}

size_t SCPI_Write(scpi_t *context, const char *data, size_t len)
{
    (void)context;
#ifdef SCPI_USE_SERIAL
    /* 通过 Debug TX 环形缓冲区非阻塞发送，避免与 Debug IT 发送冲突 */
    Debug_Transmit((const uint8_t *)data, len);
#endif
#ifdef SCPI_USE_USBTMC
    if (len + usb_buffer_len < sizeof(usb_buffer))
    {
        memcpy(&(usb_buffer[usb_buffer_len]), data, len);
        usb_buffer_len += len;
    }
    else
    {
        return SCPI_RES_ERR; // buffer overflow!
    }
#endif
    return SCPI_RES_OK;
}

scpi_result_t SCPI_Flush(scpi_t *context)
{
    (void)context;
    return SCPI_RES_OK;
}

int SCPI_Error(scpi_t *context, int_fast16_t err)
{
    (void)context;
    if (err != 0)
    {
#ifdef SCPI_USE_SERIAL
        char err_buf[64];
        int n = snprintf(err_buf, sizeof(err_buf), "**ERROR: %d, \"%s\"\r\n",
                        (int16_t)err, SCPI_ErrorTranslate(err));
        if (n > 0)
            Debug_Transmit((uint8_t *)err_buf, n);
#endif
#ifdef SCPI_USE_USBTMC
        char _err[64];
        uint8_t _err_len = 0;
        memset(_err, 0, sizeof(_err));
        sprintf(_err, "**ERROR: %d, \"%s\"\r\n", (int16_t)err, SCPI_ErrorTranslate(err));
        _err_len = strlen(_err);
        if (_err_len + usb_buffer_len < sizeof(usb_buffer))
        {
            memcpy(&(usb_buffer[usb_buffer_len]), _err, _err_len);
            usb_buffer_len += _err_len;
        }
        else
        {
            return SCPI_RES_ERR; // buffer overflow!
        }
#endif
    }
    else
    {
        /* No more errors in the queue */
    }
    return SCPI_RES_OK;
}

scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val)
{
    (void)context;
#ifdef SCPI_USE_SERIAL
    char ctrl_buf[64];
    int n;
    if (SCPI_CTRL_SRQ == ctrl)
    {
        n = snprintf(ctrl_buf, sizeof(ctrl_buf), "**SRQ: 0x%X (%d)\r\n", val, val);
    }
    else
    {
        n = snprintf(ctrl_buf, sizeof(ctrl_buf), "**CTRL %02x: 0x%X (%d)\r\n", ctrl, val, val);
    }
    if (n > 0)
        Debug_Transmit((uint8_t *)ctrl_buf, n);
#endif
#ifdef SCPI_USE_USBTMC
    char _err[64];
    uint8_t _err_len = 0;
    memset(_err, 0, sizeof(_err));
    if (SCPI_CTRL_SRQ == ctrl)
    {
        sprintf(usb_buffer, "**SRQ: 0x%X (%d)\r\n", val, val);
    }
    else
    {
        sprintf(usb_buffer, "**CTRL %02x: 0x%X (%d)\r\n", ctrl, val, val);
    }
    _err_len = strlen(_err);
    if (_err_len + usb_buffer_len < sizeof(usb_buffer))
    {
        memcpy(&(usb_buffer[usb_buffer_len]), _err, _err_len);
        usb_buffer_len += _err_len;
    }
    else
    {
        return SCPI_RES_ERR; // buffer overflow!
    }
#endif
    return SCPI_RES_OK;
}

scpi_result_t SCPI_Reset(scpi_t *context)
{
    (void)context;
#ifdef SCPI_USE_SERIAL
    Debug_Transmit((uint8_t *)"**Reset\r\n", 9);
#endif
#ifdef SCPI_USE_USBTMC
    char _err[64];
    uint8_t _err_len = 0;
    memset(_err, 0, sizeof(_err));
    sprintf(usb_buffer, "**Reset\r\n");
    _err_len = strlen(_err);
    if (_err_len + usb_buffer_len < sizeof(usb_buffer))
    {
        memcpy(&(usb_buffer[usb_buffer_len]), _err, _err_len);
        usb_buffer_len += _err_len;
    }
    else
    {
        return SCPI_RES_ERR; // buffer overflow!
    }
#endif
    return SCPI_RES_OK;
}

/* END OF LICENSE */
