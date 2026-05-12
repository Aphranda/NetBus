/*
 * io.h
 *
 *  Created on: 2026-04-17
 *      Author: STM32 Project
 *
 *  硬件引脚配置（根据 .ioc 文件）:
 *    衰减器A: CTRL_A1(PC8), CTRL_A2(PC9), CTRL_A3(PA8), CTRL_A4(PA9)
 *    衰减器B: CTRL_B1(PD2), CTRL_B2(PC12), CTRL_B3(PC11), CTRL_B4(PC10)
 */

#ifndef MODULES_IO_INC_IO_H_
#define MODULES_IO_INC_IO_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported types ------------------------------------------------------------*/

/**
 * @brief 输出IO编号枚举（8个控制输出引脚）
 */
typedef enum {
    IO_OUTPUT_A1 = 0,  /**< 衰减器A bit0 - CTRL_A1 (PC8) */
    IO_OUTPUT_A2,      /**< 衰减器A bit1 - CTRL_A2 (PC9) */
    IO_OUTPUT_A3,      /**< 衰减器A bit2 - CTRL_A3 (PA8) */
    IO_OUTPUT_A4,      /**< 衰减器A bit3 - CTRL_A4 (PA9) */
    IO_OUTPUT_B1,      /**< 衰减器B bit0 - CTRL_B1 (PD2) */
    IO_OUTPUT_B2,      /**< 衰减器B bit1 - CTRL_B2 (PC12) */
    IO_OUTPUT_B3,      /**< 衰减器B bit2 - CTRL_B3 (PC11) */
    IO_OUTPUT_B4,      /**< 衰减器B bit3 - CTRL_B4 (PC10) */
    IO_OUTPUT_COUNT    /**< 输出引脚总数：8 */
} IO_OutputID_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief 初始化IO模块
 * @note GPIO已在MX_GPIO_Init中初始化，本函数仅做状态初始化
 * @retval HAL状态
 */
HAL_StatusTypeDef IO_Init(void);

/**
 * @brief 去初始化IO模块
 */
void IO_DeInit(void);

/**
 * @brief 设置指定输出IO的状态
 * @param output 输出IO编号（IO_OutputID_t）
 * @param state GPIO_PIN_SET 或 GPIO_PIN_RESET
 */
void IO_SetOutput(IO_OutputID_t output, GPIO_PinState state);

/**
 * @brief 设置所有输出IO的状态
 * @param states 8位状态数组，索引与IO_OutputID_t对应
 */
void IO_SetAllOutputs(const GPIO_PinState states[IO_OUTPUT_COUNT]);

/**
 * @brief 读取指定输出IO的状态
 * @param output 输出IO编号
 * @retval GPIO_PIN_SET 或 GPIO_PIN_RESET
 */
GPIO_PinState IO_ReadOutput(IO_OutputID_t output);

/**
 * @brief 读取所有输出IO的状态（位掩码形式）
 * @retval 8位掩码，bit0-3=A1-A4, bit4-7=B1-B4
 */
uint8_t IO_ReadAllOutputs(void);

/**
 * @brief 设置衰减器A的衰减值（4位，0-15）
 * @param value 衰减值 (0-15)，对应A1-A4引脚状态
 *        bit0→A1, bit1→A2, bit2→A3, bit3→A4
 */
void IO_SetAttenuatorA(uint8_t value);

/**
 * @brief 设置衰减器B的衰减值（4位，0-15）
 * @param value 衰减值 (0-15)，对应B1-B4引脚状态
 *        bit0→B1, bit1→B2, bit2→B3, bit3→B4
 */
void IO_SetAttenuatorB(uint8_t value);

/**
 * @brief 获取衰减器A当前衰减值
 * @retval 4位衰减值 (0-15)
 */
uint8_t IO_GetAttenuatorA(void);

/**
 * @brief 获取衰减器B当前衰减值
 * @retval 4位衰减值 (0-15)
 */
uint8_t IO_GetAttenuatorB(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULES_IO_INC_IO_H_ */
