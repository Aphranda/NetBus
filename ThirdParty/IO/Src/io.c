/*
 * io.c
 *
 *  Created on: 2026-04-17
 *      Author: STM32 Project
 *
 *  硬件引脚配置（根据 .ioc 文件）:
 *    衰减器A: CTRL_A1(PC8), CTRL_A2(PC9), CTRL_A3(PA8), CTRL_A4(PA9)
 *    衰减器B: CTRL_B1(PD2), CTRL_B2(PC12), CTRL_B3(PC11), CTRL_B4(PC10)
 */

/* Includes ------------------------------------------------------------------*/
#include "io.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* 输出引脚映射表：与 main.h 中的引脚定义一致 */
static const struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} output_map[IO_OUTPUT_COUNT] = {
    {CTRL_A1_GPIO_Port, CTRL_A1_Pin},  // IO_OUTPUT_A1 - PC8
    {CTRL_A2_GPIO_Port, CTRL_A2_Pin},  // IO_OUTPUT_A2 - PC9
    {CTRL_A3_GPIO_Port, CTRL_A3_Pin},  // IO_OUTPUT_A3 - PA8
    {CTRL_A4_GPIO_Port, CTRL_A4_Pin},  // IO_OUTPUT_A4 - PA9
    {CTRL_B1_GPIO_Port, CTRL_B1_Pin},  // IO_OUTPUT_B1 - PD2
    {CTRL_B2_GPIO_Port, CTRL_B2_Pin},  // IO_OUTPUT_B2 - PC12
    {CTRL_B3_GPIO_Port, CTRL_B3_Pin},  // IO_OUTPUT_B3 - PC11
    {CTRL_B4_GPIO_Port, CTRL_B4_Pin},  // IO_OUTPUT_B4 - PC10
};

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static uint8_t attenuator_a_value = 0;  /**< 衰减器A当前值 (0-15) */
static uint8_t attenuator_b_value = 0;  /**< 衰减器B当前值 (0-15) */

/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化IO模块
 */
HAL_StatusTypeDef IO_Init(void)
{
    /* GPIO已在MX_GPIO_Init中初始化，此处仅初始化内部状态 */

    /* 初始化变量 */
    attenuator_a_value = 0;
    attenuator_b_value = 0;

    /* 读取当前引脚状态，同步到内部变量 */
    uint8_t output_mask = IO_ReadAllOutputs();
    attenuator_a_value = output_mask & 0x0F;           // bit0-3 = A1-A4
    attenuator_b_value = (output_mask >> 4) & 0x0F;    // bit4-7 = B1-B4

    return HAL_OK;
}

/**
 * @brief 去初始化IO模块
 */
void IO_DeInit(void)
{
    attenuator_a_value = 0;
    attenuator_b_value = 0;
}

/**
 * @brief 设置指定输出IO的状态
 */
void IO_SetOutput(IO_OutputID_t output, GPIO_PinState state)
{
    if (output >= IO_OUTPUT_COUNT)
        return;

    HAL_GPIO_WritePin(output_map[output].port, output_map[output].pin, state);

    /* 同步更新内部衰减器值 */
    if (output <= IO_OUTPUT_A4) {
        /* 衰减器A (bit0-3) */
        if (state == GPIO_PIN_SET)
            attenuator_a_value |= (1 << output);
        else
            attenuator_a_value &= ~(1 << output);
    } else {
        /* 衰减器B (bit4-7)，减去A偏移 */
        uint8_t b_idx = output - IO_OUTPUT_B1;
        if (state == GPIO_PIN_SET)
            attenuator_b_value |= (1 << b_idx);
        else
            attenuator_b_value &= ~(1 << b_idx);
    }
}

/**
 * @brief 设置所有输出IO的状态
 */
void IO_SetAllOutputs(const GPIO_PinState states[IO_OUTPUT_COUNT])
{
    for (int i = 0; i < IO_OUTPUT_COUNT; i++) {
        HAL_GPIO_WritePin(output_map[i].port, output_map[i].pin, states[i]);
    }

    /* 从状态数组更新内部衰减器值 */
    attenuator_a_value = 0;
    attenuator_b_value = 0;
    for (int i = 0; i < IO_OUTPUT_COUNT; i++) {
        if (states[i] == GPIO_PIN_SET) {
            if (i <= IO_OUTPUT_A4)
                attenuator_a_value |= (1 << i);
            else
                attenuator_b_value |= (1 << (i - IO_OUTPUT_B1));
        }
    }
}

/**
 * @brief 读取指定输出IO的状态
 */
GPIO_PinState IO_ReadOutput(IO_OutputID_t output)
{
    if (output >= IO_OUTPUT_COUNT)
        return GPIO_PIN_RESET;

    return HAL_GPIO_ReadPin(output_map[output].port, output_map[output].pin);
}

/**
 * @brief 读取所有输出IO的状态（位掩码形式）
 */
uint8_t IO_ReadAllOutputs(void)
{
    uint8_t mask = 0;
    for (int i = 0; i < IO_OUTPUT_COUNT; i++) {
        if (HAL_GPIO_ReadPin(output_map[i].port, output_map[i].pin) == GPIO_PIN_SET)
            mask |= (1 << i);
    }
    return mask;
}

/**
 * @brief 设置衰减器A的衰减值（4位，0-15）
 */
void IO_SetAttenuatorA(uint8_t value)
{
    value &= 0x0F;  // 确保只取低4位

    /* 设置A1-A4引脚 */
    for (int i = 0; i < 4; i++) {
        GPIO_PinState state = (value & (1 << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        HAL_GPIO_WritePin(output_map[IO_OUTPUT_A1 + i].port,
                          output_map[IO_OUTPUT_A1 + i].pin, state);
    }

    attenuator_a_value = value;
}

/**
 * @brief 设置衰减器B的衰减值（4位，0-15）
 */
void IO_SetAttenuatorB(uint8_t value)
{
    value &= 0x0F;  // 确保只取低4位

    /* 设置B1-B4引脚 */
    for (int i = 0; i < 4; i++) {
        GPIO_PinState state = (value & (1 << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        HAL_GPIO_WritePin(output_map[IO_OUTPUT_B1 + i].port,
                          output_map[IO_OUTPUT_B1 + i].pin, state);
    }

    attenuator_b_value = value;
}

/**
 * @brief 获取衰减器A当前衰减值
 */
uint8_t IO_GetAttenuatorA(void)
{
    return attenuator_a_value;
}

/**
 * @brief 获取衰减器B当前衰减值
 */
uint8_t IO_GetAttenuatorB(void)
{
    return attenuator_b_value;
}
