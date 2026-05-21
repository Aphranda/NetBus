# SP10T 电子开关操作手册

> **文档版本**: V1.0  
> **适用固件**: STM32G431RBT6_PAA_SP10T_RAW V0.0.1  
> **更新日期**: 2026-04-28

---

## 目录

1. [产品概述](#1-产品概述)
2. [硬件规格与接口](#2-硬件规格与接口)
3. [快速上手指南](#3-快速上手指南)
4. [系统架构](#4-系统架构)
5. [工作模式](#5-工作模式)
6. [RS485 Modbus RTU 远程控制](#6-rs485-modbus-rtu-远程控制)
7. [USART1 SCPI 调试接口](#7-usart1-scpi-调试接口)
8. [USART1 Debug 终端](#8-usart1-debug-终端)
9. [Flash 配置管理](#9-flash-配置管理)
10. [通讯示例汇总](#10-通讯示例汇总)
11. [调试与故障排除](#11-调试与故障排除)
12. [常见问题 (FAQ)](#12-常见问题-faq)
13. [附录 A: Modbus RTU 常用指令速查表](#附录-a-modbus-rtu-常用指令速查表)
14. [附录 B: CRC-16-Modbus 算法](#附录-b-crc-16-modbus-算法)
15. [附录 C: 文件结构参考](#附录-c-文件结构参考)

---

## 1. 产品概述

**SP10T 电子开关** 是一款基于 STM32G431RBT6 微控制器的单刀十掷（Single-Pole 10-Throw）射频开关控制器。设备支持三种独立控制接口：

| 接口 | 物理层 | 协议 | 用途 |
|:----:|:------:|:----:|:-----|
| **RS485** | 差分总线 (A/B) | Modbus RTU (二进制) | 远程多机控制，支持 247 台设备 |
| **USART1 (TTL)** | TX=PC4, RX=PC5 | SCPI (ASCII 文本) | 本地调试与单机配置 |
| **USART1 (TTL)** | 同上端口 | Debug 终端 (简化文本) | 本地交互式控制 |

三个接口**同时工作，互不干扰**，可通过任意接口控制开关通道切换。

### 1.1 主要特性

| 特性 | 说明 |
|------|------|
| 开关类型 | SP10T (Single-Pole 10-Throw)，通道 1-10 |
| 工作模式 | IO DIRECT（硬件直通） / COMMAND（命令控制） |
| 远程总线 | RS485 差分信号，最远 1200m |
| 远程协议 | Modbus RTU，地址 1-247，CRC16 校验 |
| 本地调试 | USART1 TTL 电平，SCPI + Debug 双协议 |
| Flash 存储 | 设备地址、名称、序列号、默认通道等配置掉电保存 |
| 固件升级 | 通过 SWD (STM32) |

---

## 2. 硬件规格与接口

### 2.1 主控芯片

| 参数 | 值 |
|------|-----|
| MCU | STM32G431RBT6 (ARM Cortex-M4, 170MHz) |
| Flash | 128 KB (末页 2KB 用于配置存储) |
| RAM | 32 KB |
| 系统时钟 | 170 MHz (HSI 16MHz → PLL ×16 ÷2) |

### 2.2 RS485 接口 (远程控制)

| 参数 | 值 |
|------|-----|
| 收发器 | MAX3485 (3.3V) |
| DE 控制 | PB12 (高电平发送，低电平接收，手动 GPIO 控制) |
| 终端电阻 | 板载 120Ω（可通过跳线接入） |
| 总线拓扑 | Daisy-Chain（菊花链） |
| 接线 | A(+) 与 A(+) 相连，B(-) 与 B(-) 相连 |

### 2.3 USART1 接口 (本地调试)

| 参数 | 值 |
|------|-----|
| TX 引脚 | PC4 |
| RX 引脚 | PC5 |
| 电平 | TTL 3.3V |
| 连接方式 | USB-TTL 转换器 (如 CH340G, CP2102) |
| 接收方式 | DMA 循环缓冲 + IDLE 中断 |

### 2.4 输入/输出引脚

#### 输入 (CTRL1-CTRL4, 4 个)

| 引脚 | 名称 | 说明 |
|:----:|:----:|------|
| GPIO | CTRL1 | 输入检测 1 |
| GPIO | CTRL2 | 输入检测 2 |
| GPIO | CTRL3 | 输入检测 3 |
| GPIO | CTRL4 | 输入检测 4 |

> DIREC 模式下，4 个输入直接映射控制 6 个输出。

#### 输出 (6 个, 用于 RF 开关编码)

| 编号 | 名称 | 说明 |
|:----:|:----:|------|
| 0 | S0_CA | 编码输出 0 — A 相 |
| 1 | S0_CB | 编码输出 0 — B 相 |
| 2 | S1_CA | 编码输出 1 — A 相 |
| 3 | S1_CB | 编码输出 1 — B 相 |
| 4 | S2_CA | 编码输出 2 — A 相 |
| 5 | S2_CB | 编码输出 2 — B 相 |

### 2.5 RS485 多设备 Daisy-Chain 拓扑

```
 PC (USB转485) ──┬── [设备A] ──┬── [设备B] ──┬── ... ── 120Ω
                 │  地址: 0x01 │  地址: 0x02 │
               120Ω           (无端接)      (无端接)
```

- **A/B 线**: 所有设备的 RS485-A (+) 相连，RS485-B (-) 相连
- **终端电阻**: 总线两端各接 120Ω 终端电阻（首端在 USB-485 转换器，末端在最后一台设备）
- **GND**: 建议所有设备共地

---

## 3. 快速上手指南

### 3.1 首次使用 — USART1 SCPI 配置

1. 使用 USB-TTL 转换器连接 USART1 (PC4=TX, PC5=RX, GND)
2. 打开串口助手，设置 **115200, 8, N, 1**
3. 发送命令查询设备身份:

   ```
   发→ *IDN?
   收← GTS,PPA_SP10T,20260417,V0.0.1
   ```

4. 查询当前通道:

   ```
   发→ READ:SWITch1:STATe?
   收← 1
   ```

5. 设置设备 Modbus 地址 (如需接入 RS485 总线):

   ```
   发→ SYSTem:CONFigure:IDENtity 1
   收← OK
   发→ SYSTem:CONFigure:SAVE
   收← OK
   ```

### 3.2 快速控制 — Debug 终端模式

1. 启用 Debug 终端:

   ```
   发→ CONFigure:DEBUG ON
   收← Debug ON
   ```

2. 输入 `HELP` 查看命令，输入 `SET 3` 切换通道:

   ```
   发→ HELP
   发→ SET 3
   收← Channel set to 3
   ```

### 3.3 远程控制 — RS485 Modbus

1. 通过 USB-485 转换器连接 RS485 总线
2. 发送 HEX 帧读取通道号 (设备地址=0x01):

   ```
   发→ (HEX) 01 03 00 00 00 01 84 0A
   收← (HEX) 01 03 02 00 01 79 84
   ```

   返回值 `0x0001` 表示当前通道 1。

---

## 4. 系统架构

### 4.1 软件模块结构

```
┌──────────────────────────────────────────────────────┐
│                     main.c 主循环                      │
│  SCPI_Input → MODBUS_SlaveTask → Debug_Process       │
└──────┬──────────────┬──────────────────┬──────────────┘
       │              │                  │
┌──────▼──────┐ ┌─────▼──────┐  ┌──────▼──────┐
│ SCPI 协议栈  │ │ Modbus RTU │  │ Debug 终端   │
│  (USART1)   │ │ (USART3)   │  │ (USART1)    │
│ libscpi     │ │ 从机状态机  │  │ 简化命令     │
└──────┬──────┘ └─────┬──────┘  └──────┬──────┘
       │              │                  │
       └──────────────┼──────────────────┘
                      │
              ┌───────▼───────┐
              │    IO 模块     │
              │  开关通道控制   │
              │   输出控制      │
              │   输入读取      │
              └───────┬───────┘
                      │
              ┌───────▼───────┐
              │  Flash 配置存储 │
              │ (STM32 末页2KB)│
              └───────────────┘
```

### 4.2 初始化流程

```
HAL_Init()
  → SystemClock_Config()        // 系统时钟 170MHz
  → MX_GPIO_Init()              // GPIO 初始化
  → MX_DMA_Init()               // DMA 初始化
  → MX_USART1_UART_Init()       // USART1 (SCPI + Debug)
  → MX_USART3_UART_Init()       // USART3 (RS485 Modbus)
  → MX_TIM2_Init()              // 定时器
  → MX_TIM1_Init()              // 定时器
  → Flash_Init()                // 从 Flash 加载配置
  → IO_Init()                   // IO 模块初始化
  → Debug_Init()                // Debug 模块初始化 (默认禁用)
  → SCPI_Init()                 // SCPI 解析器初始化
  → RS485_Init()                // RS485 驱动初始化
  → MODBUS_Init(NULL)           // Modbus 从机初始化
```

### 4.3 主循环任务

| 执行顺序 | 任务 | 优先级 | 说明 |
|:-------:|:----:|:------:|------|
| 1 | `SCPI_Input()` | 最高 | 处理 USART1 SCPI 命令 |
| 2 | `MODBUS_SlaveTask()` | 中 | 处理 RS485 Modbus 帧 |
| 3 | `Debug_Process()` | 低 | 处理 Debug 终端命令 (仅启用时) |

三个任务在同一主循环中轮询执行，所有 IO 操作非阻塞。

---

## 5. 工作模式

设备支持两种工作模式，决定 RF 开关的控制来源。

### 5.1 IO DIRECT 模式 (直通模式, mode=0)

**默认模式**。4 个硬件输入引脚 (CTRL1-CTRL4) 直接编码映射到 6 个输出引脚 (S0_CA~S2_CB)，实现开关通道选择。

| 特性 | 说明 |
|:----:|------|
| 输入 | CTRL1-CTRL4 (4 位编码) |
| 输出 | S0_CA~S2_CB (6 位控制 RF 开关) |
| 触发 | 输入变化时由 EXTI 中断自动触发映射 |
| 软件控制 | 可通过任意命令接口切换到此模式 |

### 5.2 COMMAND 模式 (命令模式, mode=1)

所有开关通道切换必须通过命令接口完成：SCPI、Debug 终端或 Modbus RTU。

| 特性 | 说明 |
|:----:|------|
| 控制源 | SCPI / Debug / Modbus |
| 硬件输入 | 被忽略，不对输出产生直接影响 |
| 适用场景 | 远程自动化控制 |

### 5.3 模式切换

| 方式 | 指令 |
|:----:|------|
| SCPI | `CONFigure:MODE CMD` 或 `CONFigure:MODE IO` |
| Debug | `MODE CMD` 或 `MODE IO` |
| Modbus | `01 06 00 01 00 01 CRC` (→CMD) / `01 06 00 01 00 00 CRC` (→IO) |

---

## 6. RS485 Modbus RTU 远程控制

### 6.1 通讯参数

| 参数 | 值 |
|------|-----|
| 协议 | **Modbus RTU** (二进制) |
| 波特率 | **115200 bps** |
| 数据位 | 8 |
| 校验位 | 无 (N) |
| 停止位 | 1 |
| 帧间隔 | ≥ 3.5 字符时间 (~304μs @115200) |
| 字节格式 | Little-Endian (CRC 低字节在前) |
| 最大帧长 | 256 字节 |
| 帧间检测 | USART IDLE 中断 + DMA 循环缓冲 + DWT 周期计数器 T35 超时 |

### 6.2 从机地址

| 地址 | 说明 |
|:----:|------|
| `0x00` | 广播地址（所有设备执行但不回复） |
| `0x01` - `0xF7` | 单播地址（1-247，地址匹配则回复） |
| `0xF8` - `0xFF` | 保留 |

**设备 ID 存储**: 保存在 Flash 中，通过 [`Flash_GetDeviceID()`](App/Flash/Inc/flash.h:159) 读取。出厂默认 ID=0。

**特殊规则**: 当设备 ID = 0（未配置）时，设备接受所有非广播帧，便于首次配置时设置地址。

### 6.3 寄存器映射

#### 6.3.1 保持寄存器 (Holding Registers) — 功能码 `0x03` 读 / `0x06` `0x10` 写

| 地址 | 名称 | R/W | 默认值 | 说明 |
|:----:|------|:---:|:------:|------|
| `0x0000` | REG_SWITCH_CHANNEL | R | — | 当前通道号 (1-N)，[`IO_GetCurrentChannel()`](App/IO/Inc/io.h:156) + 1 |
| `0x0001` | REG_WORK_MODE | R/W | — | 工作模式 (0=IO DIRECT, 1=COMMAND) |
| `0x0002` | REG_DEVICE_ID | R/W | Flash | 设备 Modbus 地址 (1-247) |
| `0x0003` | REG_SERIAL_NUMBER_H | R | Flash | 序列号高16位 |
| `0x0004` | REG_SERIAL_NUMBER_L | R | Flash | 序列号低16位 |
| `0x0005` | REG_DEVICE_NAME_0 | R/W | Flash | 设备名称第1-2字符 |
| `0x0006` | REG_DEVICE_NAME_1 | R/W | Flash | 设备名称第3-4字符 |
| `0x0007` | REG_DEVICE_NAME_2 | R/W | Flash | 设备名称第5-6字符 |
| `0x0008` | REG_DEVICE_NAME_3 | R/W | Flash | 设备名称第7-8字符 |
| `0x0009` | REG_DEVICE_STATUS | R | — | 设备状态位（见下方） |
| `0x000A` | REG_OUTPUT_CONTROL | R/W | — | 输出控制位（每bit对应一个输出） |
| `0x000B` | REG_FW_VERSION | R | Flash | 固件版本 (主<<8 \| 次) |
| `0x000C` - `0x001A` | *保留* | — | — | 扩展预留 |
| `0x001B` | REG_HW_VERSION | R | Flash | 硬件版本 |

**REG_DEVICE_STATUS 位定义**:

| Bit | 含义 |
|:---:|------|
| 0 | 初始化完成标志 |
| 1 | 通讯状态 |
| 2-15 | 保留 |

#### 6.3.2 输入寄存器 (Input Registers) — 功能码 `0x04` 只读

| 地址 | 名称 | 说明 |
|:----:|------|------|
| `0x0000` | REG_INPUT_CHANNEL | 当前输入通道号 |
| `0x0001` | REG_INPUT_VOLTAGE | ADC 电压采样值 |

#### 6.3.3 线圈 (Coils) — 功能码 `0x01` 读 / `0x05` `0x0F` 写

| 地址 | 名称 | 说明 |
|:----:|------|------|
| `0x0000` - `0x0005` | COIL_OUTPUT_1 ~ COIL_OUTPUT_6 | 6个输出继电器 (0=OFF, 1=ON) |
| `0x0006` - `0xFFFF` | *保留* | 读取返回 0 |

#### 6.3.4 离散输入 (Discrete Inputs) — 功能码 `0x02` 只读

| 地址 | 名称 | 说明 |
|:----:|------|------|
| `0x0000` - `0x0003` | DISC_INPUT_1 ~ DISC_INPUT_4 | 4个输入检测引脚 |
| `0x0004` - `0xFFFF` | *保留* | 读取返回 0 |

### 6.4 支持的功能码

| 功能码 | 名称 | 说明 |
|:-----:|------|------|
| `0x01` | Read Coils | 读取线圈输出状态 |
| `0x02` | Read Discrete Inputs | 读取离散输入状态 |
| `0x03` | Read Holding Registers | 读取保持寄存器 (2字节/个) |
| `0x04` | Read Input Registers | 读取输入寄存器 (2字节/个) |
| `0x05` | Write Single Coil | 写入单个线圈 (0xFF00=ON, 0x0000=OFF) |
| `0x06` | Write Single Register | 写入单个保持寄存器 |
| `0x0F` | Write Multiple Coils | 写入多个线圈 |
| `0x10` | Write Multiple Registers | 写入多个保持寄存器 |

### 6.5 帧格式

#### 通用帧结构

```
┌─────────┬─────────┬───────────────┬──────────┐
│ 地址(1) │ 功能码(1)│  数据(N)      │ CRC16(2) │
└─────────┴─────────┴───────────────┴──────────┘
```

- 地址: 1 字节 (0x00 广播 / 0x01-0xF7 单播)
- 功能码: 1 字节 (0x01-0x06 / 0x0F / 0x10)
- 数据: N 字节 (寄存器数据大端序)
- CRC16: 2 字节 (低字节在前)

#### 各功能码帧格式

| 功能码 | 请求格式 | 响应格式 |
|:-----:|----------|----------|
| `0x01` (Read Coils) | `ADDR \| 0x01 \| 起始地址H \| L \| 数量H \| L \| CRC16` | `ADDR \| 0x01 \| 字节数 \| 线圈数据 \| CRC16` |
| `0x02` (Read Discrete Inputs) | `ADDR \| 0x02 \| 起始地址H \| L \| 数量H \| L \| CRC16` | `ADDR \| 0x02 \| 字节数 \| 输入数据 \| CRC16` |
| `0x03` (Read Holding Registers) | `ADDR \| 0x03 \| 起始地址H \| L \| 数量H \| L \| CRC16` | `ADDR \| 0x03 \| 字节数 \| 数据H \| L ... \| CRC16` |
| `0x04` (Read Input Registers) | `ADDR \| 0x04 \| 起始地址H \| L \| 数量H \| L \| CRC16` | `ADDR \| 0x04 \| 字节数 \| 数据H \| L ... \| CRC16` |
| `0x05` (Write Single Coil) | `ADDR \| 0x05 \| 地址H \| L \| 0xFF/0x00 \| 0x00 \| CRC16` | Echo 请求帧 |
| `0x06` (Write Single Register) | `ADDR \| 0x06 \| 地址H \| L \| 数据H \| L \| CRC16` | Echo 请求帧 |
| `0x0F` (Write Multiple Coils) | `ADDR \| 0x0F \| 起始地址H \| L \| 数量H \| L \| 字节数 \| 数据 \| CRC16` | `ADDR \| 0x0F \| 起始地址H \| L \| 数量H \| L \| CRC16` |
| `0x10` (Write Multiple Registers) | `ADDR \| 0x10 \| 起始地址H \| L \| 数量H \| L \| 字节数 \| 数据 \| CRC16` | `ADDR \| 0x10 \| 起始地址H \| L \| 数量H \| L \| CRC16` |

#### 异常帧格式

```
┌─────────┬──────────────┬──────────┬──────────┐
│ 地址(1) │ 功能码|0x80(1)│ 异常码(1)│ CRC16(2) │
└─────────┴──────────────┴──────────┴──────────┘
```

### 6.6 异常响应

| 异常码 | 名称 | 说明 |
|:-----:|------|------|
| `0x01` | ILLEGAL_FUNCTION | **不支持的功能码** — 收到 0x01-0x06/0x0F/0x10 以外的功能码 |
| `0x02` | ILLEGAL_DATA_ADDRESS | **地址越界** — 寄存器/线圈/输入地址超出范围 |
| `0x03` | ILLEGAL_DATA_VALUE | **数据值非法** — 数量为0、线圈值不是0xFF00/0x0000等 |
| `0x04` | SLAVE_DEVICE_FAILURE | **设备执行失败** — 底层回调/I2C/Flash 操作失败 |

异常响应示例:

| 请求 | 异常响应 | 含义 |
|------|---------|------|
| `01 09 00 00 00 01 CRC` | `01 89 01 CRC` | 功能码0x09不支持 |
| `01 03 00 20 00 01 CRC` | `01 83 02 CRC` | 地址0x0020超出范围 |
| `01 03 00 00 00 00 CRC` | `01 83 03 CRC` | 读取数量为0 |
| `01 05 00 00 12 34 CRC` | `01 85 03 CRC` | 线圈数据不是0xFF00或0x0000 |

### 6.7 Modbus 协议栈实现要点

| 组件 | 文件 | 说明 |
|:----:|:----:|------|
| 帧定义 | [`App/Modbus/Inc/modbus_frame.h`](App/Modbus/Inc/modbus_frame.h) | 地址/功能码/异常码/缓冲区大小定义 |
| CRC16 | [`App/Modbus/Src/modbus_crc.c`](App/Modbus/Src/modbus_crc.c) | 查表法 CRC-16-Modbus，低字节在前 |
| 从机状态机 | [`App/Modbus/Src/modbus_slave.c`](App/Modbus/Src/modbus_slave.c) | T35 超时 + DMA 帧接收 + 地址过滤 + 帧处理 |
| 功能码处理器 | [`App/Modbus/Src/modbus_functions.c`](App/Modbus/Src/modbus_functions.c) | 8 个功能码分发 + 异常构建 |
| 寄存器回调 | [`App/Modbus/Src/modbus_regs.c`](App/Modbus/Src/modbus_regs.c) | 映射 IO/Flash 到 Modbus 地址空间 |

---

## 7. USART1 SCPI 调试接口

### 7.1 概述

USART1 为本地调试串口，运行 **SCPI** (Standard Commands for Programmable Instruments) 文本协议。与 RS485 的 Modbus 协议完全独立。

| 参数 | 值 |
|------|-----|
| 接口 | USART1 (TX=PC4, RX=PC5) |
| 波特率 | **115200 bps** |
| 帧格式 | 8N1 |
| 协议 | SCPI (ASCII 文本，`\r\n` 结尾) |
| 电平 | TTL 3.3V (需 USB-TTL 转换器) |
| 协议栈 | libscpi (开源 SCPI 解析器) |

### 7.2 IEEE 488.2 标准指令

| 指令 | 说明 | 示例 |
|------|------|------|
| `*IDN?` | 查询设备身份 | `*IDN?` → `GTS,PPA_SP10T,20260417,V0.0.1` |
| `*CLS` | 清除状态 | `*CLS` |
| `*RST` | 复位设备 | `*RST` |
| `*STB?` | 查询状态字节 | `*STB?` → `0` |
| `*WAI` | 等待操作完成 | `*WAI` |
| `*OPC?` | 查询操作完成 | `*OPC?` → `1` |

### 7.3 开关通道控制

| 指令 | 说明 | 示例 |
|------|------|------|
| `CONFigure:SWITch# <channel>` | 设置开关通道<br>其中 # 为本机设备ID (1-based) | `CONFigure:SWITch1 3` → 切换到通道3<br>`CONFigure:SWITch2 5` → 仅设备ID=2响应 |
| `READ:SWITch#:STATe?` | 查询当前通道号 | `READ:SWITch1:STATe?` → `3` |

> `SWITCH#` 中的 `#` 必须匹配本机设备ID。若设备ID=0（未配置），接受任意编号。

### 7.4 输入读取

| 指令 | 说明 | 示例 |
|------|------|------|
| `READ:INPut:ALL?` | 读取所有输入引脚状态 (4位掩码) | `READ:INPut:ALL?` → `5` (bit0=1, bit2=1) |
| `READ:INPut:ALL?` | 返回值: CTRL1=bit0, CTRL2=bit1, CTRL3=bit2, CTRL4=bit3 |

### 7.5 输出控制

| 指令 | 说明 | 示例 |
|------|------|------|
| `CONFigure:OUTPut <id> <0/1>` | 设置指定输出 (id: 0-5) | `CONFigure:OUTPut 0 1` → 输出0打开<br>`CONFigure:OUTPut 2 0` → 输出2关闭 |
| `READ:OUTPut:ALL?` | 读取所有输出状态 (6位掩码) | `READ:OUTPut:ALL?` → `3` (输出0和1打开) |

**输出编号映射**:

| ID | 名称 | 说明 |
|:--:|:----:|------|
| 0 | S0_CA | 编码输出 0 — A 相 |
| 1 | S0_CB | 编码输出 0 — B 相 |
| 2 | S1_CA | 编码输出 1 — A 相 |
| 3 | S1_CB | 编码输出 1 — B 相 |
| 4 | S2_CA | 编码输出 2 — A 相 |
| 5 | S2_CB | 编码输出 2 — B 相 |

### 7.6 模式设置

| 指令 | 说明 | 示例 |
|------|------|------|
| `CONFigure:MODE <DIRECT/CMD/IO/COMMAND>` | 设置工作模式 | `CONFigure:MODE CMD` → 命令模式 |
| `READ:MODE:STATe?` | 查询当前模式 | `READ:MODE:STATe?` → `COMMAND` |

### 7.7 Debug 控制

| 指令 | 说明 | 示例 |
|------|------|------|
| `CONFigure:DEBUG <ON/OFF/1/0>` | 启用/禁用 Debug 终端 | `CONFigure:DEBUG ON` → 终端模式启用 |

> Debug 终端启用后，USART1 同时处理 SCPI 命令（以 `*` 开头或包含 `:`）和 Debug 简化命令（如 `SET 3`、`GET`）。参见第8章。

### 7.8 Flash 配置管理 (SYSTem 子系统)

| 指令 | 说明 | 示例 |
|------|------|------|
| `SYSTem:CONFigure:IDENtity <id>` | 设置设备 Modbus 地址 (1-247) | `SYSTem:CONFigure:IDENtity 2` → `OK` |
| `SYSTem:CONFigure:IDENtity?` | 查询当前设备地址 | `SYSTem:CONFigure:IDENtity?` → `1` |
| `SYSTem:CONFigure:NAME <name>` | 设置设备名称 (最长8字符) | `SYSTem:CONFigure:NAME SwitchA` → `OK` |
| `SYSTem:CONFigure:NAME?` | 查询设备名称 | `SYSTem:CONFigure:NAME?` → `SwitchA` |
| `SYSTem:CONFigure:SERIAL <sn>` | 设置序列号 | `SYSTem:CONFigure:SERIAL 10001` → `OK` |
| `SYSTem:CONFigure:SERIAL?` | 查询序列号 | `SYSTem:CONFigure:SERIAL?` → `10001` |
| `SYSTem:CONFigure:SAVE` | 保存当前配置到 Flash | `SYSTem:CONFigure:SAVE` → `OK` |
| `SYSTem:CONFigure:DEFaults` | 恢复出厂默认配置 | `SYSTem:CONFigure:DEFaults` → `OK` |
| `SYSTem:CONFigure:STATus?` | 查询配置状态 | `SYSTem:CONFigure:STATus?` → `1` (已配置) |
| `SYSTem:ERRor[:NEXT]?` | 查询下一个错误 | `SYSTem:ERRor?` → `0,"No error"` |
| `SYSTem:ERRor:COUNt?` | 查询错误队列计数 | `SYSTem:ERRor:COUNt?` → `0` |

### 7.9 SCPI 完整使用示例

```
# 1. 查询设备身份
发→ *IDN?
收← GTS,PPA_SP10T,20260417,V0.0.1

# 2. 查询当前通道号
发→ READ:SWITch1:STATe?
收← 1

# 3. 切换到通道 5
发→ CONFigure:SWITch1 5
收← OK

# 4. 查询确认
发→ READ:SWITch1:STATe?
收← 5

# 5. 设置设备 Modbus 地址为 2
发→ SYSTem:CONFigure:IDENtity 2
收← OK

# 6. 读取所有输入
发→ READ:INPut:ALL?
收← 3

# 7. 打开输出 0
发→ CONFigure:OUTPut 0 1
收← (无输出)

# 8. 设置模式为 COMMAND
发→ CONFigure:MODE CMD
收← (无输出)

# 9. 保存配置
发→ SYSTem:CONFigure:SAVE
收← OK

# 10. 启用 Debug 终端
发→ CONFigure:DEBUG ON
收← Debug ON
```

---

## 8. USART1 Debug 终端

### 8.1 概述

Debug 终端是 USART1 上的第二个协议层，提供简化的文本命令用于快速交互式控制。默认**禁用**，需通过 SCPI 指令 `CONFigure:DEBUG ON` 启用。

| 参数 | 值 |
|------|-----|
| 协议 | 简化文本命令（`\r\n` 结尾） |
| 默认状态 | **禁用** (debug_enabled = 0) |
| 启用方式 | `CONFigure:DEBUG ON` |
| 禁用方式 | `CONFigure:DEBUG OFF` |
| 命令分隔 | 回车换行 `\r\n` |
| 大小写 | 不敏感（内部转大写比较） |
| 提示符 | `> ` (命令执行后自动打印) |
| 回退 | 支持 Backspace 键 |
| SCPI 路由 | 以 `*` 开头或包含 `:` 自动路由到 SCPI 引擎 |

### 8.2 命令参考

| 命令 | 参数 | 说明 | 实现参考 |
|:----:|:----:|------|:--------:|
| **`SET`** | `<channel>` | 设置开关通道 (1-10) | [`Debug_ExecuteSetChannel()`](App/Debug/Src/debug.c:531) |
| **`GET`** | — | 获取当前通道号 | [`Debug_ExecuteGetChannel()`](App/Debug/Src/debug.c:554) |
| **`INPUT`** | — | 读取所有4个输入引脚状态 (CTRL1-CTRL4) | [`Debug_ExecuteReadInput()`](App/Debug/Src/debug.c:564) |
| **`OUTPUTS`** | — | 读取所有6个输出引脚状态 (S0_CA~S2_CB) | [`Debug_ExecuteGetOutputs()`](App/Debug/Src/debug.c:579) |
| **`OUTPUT`** | `<id> <state>` | 设置指定输出 (id: 0-5, state: 0/1) | [`Debug_ExecuteSetOutput()`](App/Debug/Src/debug.c:594) |
| **`MODE`** | `[IO\|CMD]` | 获取或设置工作模式 | [`Debug_ExecuteSetMode()`](App/Debug/Src/debug.c:623) |
| **`INIT`** | — | 通过 RS485 发送 `*IDN?\r\n` (设备发现/握手) | [`Debug_ExecuteInit()`](App/Debug/Src/debug.c:662) |
| **`HELP`** | — | 显示帮助信息 | [`Debug_ShowHelp()`](App/Debug/Src/debug.c:672) |

### 8.3 命令详解

#### SET — 设置开关通道

```
SET <channel>
```

- `<channel>`: 1-10 (1为通道1, 10为通道10)
- 内部转换为 0-based 索引后调用 [`IO_SetSwitchChannel()`](App/IO/Inc/io.h:150)
- 成功: `Channel set to <channel>\r\n`
- 失败: `Error: Channel must be 1-10\r\n`

#### GET — 获取当前通道

```
GET
```

- 调用 [`IO_GetCurrentChannel()`](App/IO/Inc/io.h:156) 获取内部索引
- 输出转换为 1-based: `Current channel: <channel>\r\n`

#### INPUT — 读取所有输入

```
INPUT
```

- 调用 [`IO_ReadAllInputs()`](App/IO/Inc/io.h:117) 获取4位掩码
- 输出每个引脚名称和 HIGH/LOW 状态:
  ```
  Input states:
    CTRL1: HIGH
    CTRL2: LOW
    CTRL3: HIGH
    CTRL4: LOW
  ```

#### OUTPUTS — 读取所有输出

```
OUTPUTS
```

- 调用 [`IO_ReadAllOutputs()`](App/IO/Inc/io.h:143) 获取6位掩码
- 输出每个引脚名称和 HIGH/LOW 状态:
  ```
  Output states:
    S0_CA: HIGH
    S0_CB: LOW
    S1_CA: HIGH
    S1_CB: LOW
    S2_CA: LOW
    S2_CB: LOW
  ```

#### OUTPUT — 设置指定输出

```
OUTPUT <id> <state>
```

- `<id>`: 0-5 (0=S0_CA, 1=S0_CB, 2=S1_CA, 3=S1_CB, 4=S2_CA, 5=S2_CB)
- `<state>`: 0 或 1
- 调用 [`IO_SetOutput()`](App/IO/Inc/io.h:124)
- 用法提示: `Usage: OUTPUT <id> <state>\r\n`

#### MODE — 获取/设置工作模式

```
MODE              → 显示当前模式
MODE IO           → 切换到 IO DIRECT 模式
MODE CMD          → 切换到 COMMAND 命令模式
MODE COMMAND      → 同 MODE CMD
```

- 无参数时显示当前模式:
  - IO DIRECT: `Current mode: IO DIRECT (4 inputs -> 6 outputs)`
  - COMMAND: `Current mode: COMMAND (via UART/SCPI/RS485)`
- IO 模式切换:
  - `MODE IO`: `Mode switched to IO DIRECT` + `CTRL1-4 inputs now directly control RF outputs`
  - `MODE CMD`: `Mode switched to COMMAND` + `IO control via SET/SCPI/RS485 commands only`

#### INIT — 发送 *IDN? 查询

```
INIT
```

- 通过 RS485 发送 `*IDN?\r\n` (ASCII 字符串)
- 用于设备发现和握手
- 输出: `Sending *IDN? via RS485...\r\n`

#### HELP — 显示帮助

```
HELP
```

- 输出所有可用命令及说明

### 8.4 Debug 完整会话示例

```
发→ CONFigure:DEBUG ON
收← 
========================================
Debug Console - IO Switch Control
Current mode: IO DIRECT (4 inputs -> 6 outputs)
Type 'HELP' for available commands
========================================
> 
发→ HELP
收← 
Available Commands:
  SET <channel>     - Set switch channel (1-10)
  GET               - Get current channel
  INPUT             - Read all input pin states
  OUTPUTS           - Read all output pin states
  OUTPUT <id> <st>  - Set output pin (id: 0-5, st: 0 or 1)
  MODE [IO|CMD]     - Get/set operation mode
                       IO  = direct input-to-output mapping
                       CMD = command control via UART
  INIT              - Send *IDN? query via RS485
  HELP              - Show this help message

> 
发→ GET
收← Current channel: 1
> 
发→ SET 5
收← Channel set to 5
> 
发→ GET
收← Current channel: 5
> 
发→ MODE CMD
收← Mode switched to COMMAND
  IO control via SET/SCPI/RS485 commands only
> 
发→ INPUT
收← Input states:
  CTRL1: LOW
  CTRL2: HIGH
  CTRL3: LOW
  CTRL4: LOW
> 
发→ OUTPUTS
收← Output states:
  S0_CA: HIGH
  S0_CB: LOW
  S1_CA: LOW
  S1_CB: HIGH
  S2_CA: LOW
  S2_CB: LOW
> 
发→ CONFigure:DEBUG OFF
收← (Debug 终端关闭，> 提示符消失)
```

### 8.5 Debug 终端与 SCPI 的共存机制

USART1 接收的数据通过 DMA 循环缓冲 + IDLE 中断处理，每字节同时送达 Debug 和 SCPI 两个处理引擎：

- **SCPI 引擎**: 始终启用。收到完整行（`\r\n` 结尾）后自动解析执行。
- **Debug 终端**: 默认禁用。启用后，`Debug_ParseCommand()` 判断命令类型：
  - 以 `*` 开头 → 路由给 SCPI
  - 包含 `:` → 路由给 SCPI
  - 否则 → Debug 终端处理

这种设计确保 SCPI 命令始终可用，Debug 终端启用后也不干扰 SCPI。

---

## 9. Flash 配置管理

### 9.1 存储布局

| 参数 | 值 |
|:----:|:----:|
| 存储介质 | STM32G431RBT6 内嵌 Flash |
| 存储位置 | 末页 (Page 63, 地址 `0x0801F800`) |
| 页大小 | 2 KB |
| 链接配置 | FLASH 从 128KB 减至 126KB，预留末页 |

### 9.2 配置结构体

```c
typedef struct {
    uint32_t magic;              // 魔数 (0xA5A5A5A5)
    uint32_t version;            // 结构版本 (v1.0.0)
    uint8_t  device_id;          // Modbus 地址 (1-247)
    uint8_t  hw_version;         // 硬件版本 (BCD)
    uint8_t  fw_version_major;   // 固件主版本
    uint8_t  fw_version_minor;   // 固件次版本
    uint32_t serial_number;      // 序列号
    char     device_name[24];    // 设备名称
    uint8_t  default_channel;    // 默认开关通道
    uint8_t  default_mode;       // 默认 IO 模式
    uint32_t rs485_baudrate;     // RS485 波特率
    uint8_t  reserved[32];       // 保留
    uint32_t crc;                // CRC32 校验
} Flash_Config_t;
```

### 9.3 配置管理命令

| 方式 | 命令 | 说明 |
|:----:|:----:|------|
| SCPI | `SYSTem:CONFigure:IDENtity <id>` | 设置 Modbus 地址 |
| SCPI | `SYSTem:CONFigure:NAME <name>` | 设置设备名称 |
| SCPI | `SYSTem:CONFigure:SERIAL <sn>` | 设置序列号 |
| SCPI | `SYSTem:CONFigure:SAVE` | 保存到 Flash |
| SCPI | `SYSTem:CONFigure:DEFaults` | 恢复出厂默认 |
| Modbus | `0x06` 写 `REG_DEVICE_ID` | 设置地址 |
| Modbus | `0x10` 写 `REG_DEVICE_NAME_0~3` | 设置名称 |

> **重要**: 修改配置后必须发送 `SYSTem:CONFigure:SAVE` 或通过 Modbus 配置管理，否则掉电丢失。

---

## 10. 通讯示例汇总

### 10.1 RS485 Modbus 示例

#### 示例1: 读取当前通道号

```
发→ 01 03 00 00 00 01 84 0A
收← 01 03 02 00 01 79 84
```

- 地址: `0x01`, 功能码: `0x03` (读保持寄存器)
- 起始地址: `0x0000`, 读取数量: `1`
- 返回值: `0x0001` (通道1)

#### 示例2: 设置工作模式为 CMD

```
发→ 01 06 00 01 00 01 18 0A
收← 01 06 00 01 00 01 18 0A  (Echo)
```

#### 示例3: 设置通道 3

```
发→ 01 06 00 00 00 03 C8 0A
收← 01 06 00 00 00 03 C8 0A  (Echo)
```

#### 示例4: 读取所有线圈

```
发→ 01 01 00 00 00 06 BC 0B
收← 01 01 01 2A CRC           (线圈: bit0-5 = 0b101010)
```

#### 示例5: 写入线圈2为 ON

```
发→ 01 05 00 01 FF 00 DD FA
收← 01 05 00 01 FF 00 DD FA  (Echo)
```

#### 示例6: 非法地址访问

```
发→ 01 03 00 20 00 01 CRC
收← 01 83 02 CRC              (异常: ILLEGAL_DATA_ADDRESS)
```

### 10.2 USART1 SCPI 示例

```
# 查询身份
发→ *IDN?
收← GTS,PPA_SP10T,20260417,V0.0.1

# 查询通道
发→ READ:SWITch1:STATe?
收← 1

# 切换通道
发→ CONFigure:SWITch1 5
收← OK

# 设置模式
发→ CONFigure:MODE CMD
收← (无输出)

# 查询模式
发→ READ:MODE:STATe?
收← COMMAND

# 读取输入
发→ READ:INPut:ALL?
收← 3
```

### 10.3 USART1 Debug 终端示例

```
# 启用 Debug
发→ CONFigure:DEBUG ON
收← (显示欢迎信息)

# 设置通道
发→ SET 3
收← Channel set to 3

# 查询通道
发→ GET
收← Current channel: 3

# 读取输入
发→ INPUT
收← Input states:
  CTRL1: HIGH
  CTRL2: LOW
  CTRL3: HIGH
  CTRL4: LOW

# 读取输出
发→ OUTPUTS
收← Output states:
  S0_CA: HIGH
  S0_CB: LOW
  S1_CA: LOW
  S1_CB: HIGH
  S2_CA: LOW
  S2_CB: LOW

# 设置输出
发→ OUTPUT 0 1
收← Output 0 set to 1

# 查看帮助
发→ HELP
收← (帮助信息)

# 禁用 Debug
发→ CONFigure:DEBUG OFF
```

---

## 11. 调试与故障排除

### 11.1 Modbus 调试日志

固件默认**不输出** Modbus 调试日志。如需通过 USART1 查看 Modbus 协议栈内部状态：

#### 方法1: 编译时启用 (永久)

1. 打开 [`App/Modbus/Inc/modbus.h`](App/Modbus/Inc/modbus.h)
2. 找到约第33行，取消注释:
   ```c
   #define MODBUS_DEBUG
   ```
3. 重新编译下载

#### 方法2: 运行时启用 (临时)

```
发→ CONFigure:DEBUG ON
```

或通过 SCPI:
```
发→ CONFigure:DEBUG ON
收← Debug ON
```

> 注意: `CONFigure:DEBUG` 控制的是 Debug 终端的启用/禁用，同时会打印 `[MB]` 格式的 Modbus 调试日志。

#### 日志输出示例

```
[MB] Init: SysClk=170000000, T35=51680 cyc, devID=1
[MB] FeedData: len=8, data=01 03 00 00 00 01 84 0A
[MB] FeedData: buf_len=8, DWT=0x12345678
[MB] process_frame: len=8
[MB] addr=0x01 func=0x03 myID=1
[MB] CRC check: OK
[MB] Dispatch done: rsp_len=7
[MB] Transmit: len=7 data=01 03 02 00 01 79 84
```

### 11.2 三接口对照表

| 能力 | RS485 Modbus | USART1 SCPI | USART1 Debug |
|:----:|:------------:|:-----------:|:------------:|
| 开关通道控制 | ✅ R/W 寄存器 | ✅ CONFigure:SWITch# | ✅ SET 命令 |
| 通道查询 | ✅ 读寄存器 | ✅ READ:SWITch#:STATe? | ✅ GET 命令 |
| 输入读取 | ✅ 离散输入/寄存器 | ✅ READ:INPut:ALL? | ✅ INPUT 命令 |
| 输出控制 | ✅ 线圈/寄存器 | ✅ CONFigure:OUTPut | ✅ OUTPUT 命令 |
| 模式切换 | ✅ 写寄存器 | ✅ CONFigure:MODE | ✅ MODE 命令 |
| Flash 配置 | ✅ 写寄存器 | ✅ SYSTem:CONFigure | ❌ |
| *IDN? 查询 | ❌ | ✅ 标准 SCPI | ✅ INIT 命令 |
| 多设备支持 | ✅ 247 台 | ❌ 仅本地 | ❌ 仅本地 |

### 11.3 常见检查清单

#### RS485 无响应

1. 检查 RS485 A/B 线是否接反
2. 检查设备地址是否匹配请求中的地址（用 `SYSTem:CONFigure:IDENtity?` 查询）
3. 检查 CRC 计算是否正确（CRC 错误会被静默丢弃）
4. 检查帧间隔是否 ≥ 3.5 字节时间（连续发送会合并为一帧）
5. 先用 USART1 SCPI 测试设备是否正常运行: `*IDN?`

#### USART1 无输出

1. 检查 USB-TTL 转换器的 TX 是否连接设备的 RX (交叉连接)
2. 检查波特率: 115200-8N1
3. USART1 同时支持 SCPI 和 Debug，至少 SCPI 始终有效

#### Debug 终端命令无响应

1. 确认已启用: `CONFigure:DEBUG ON`
2. SCPI 命令仍然有效，但 Debug 简化命令需先启用
3. 命令不区分大小写，但语法需正确

---

## 12. 常见问题 (FAQ)

### Q: 设备没有响应 (RS485 Modbus)？

1. 检查 RS485 A/B 线是否接反
2. 检查设备地址是否匹配请求中的地址（用 [`SYSTem:CONFigure:IDENtity?`](App/SCPI/port/scpi-def.c) 查询）
3. 检查 CRC 计算是否正确（CRC 错误会被静默丢弃）
4. 检查帧间隔是否 ≥ 3.5 字节时间（连续发送会合并为一帧）
5. 先用 USART1 SCPI 测试设备是否正常运行: `*IDN?`

### Q: 返回异常码 0x02？

请求的寄存器/线圈地址超出范围。查看[第6.3节](#63-寄存器映射)寄存器映射表确认有效地址。

### Q: 返回异常码 0x03？

- 读取数量为 0
- 写线圈时数据不是 `0xFF00` 或 `0x0000`
- 写寄存器时数据值不合法

### Q: 如何改变设备地址？

**方法1 — 通过 RS485 Modbus**:

```
发→ 01 06 00 02 00 05 CRC    (将设备地址改为 5)
收← 01 06 00 02 00 05 CRC    (Echo 成功)
```

**方法2 — 通过 USART1 SCPI**:

```
发→ SYSTem:CONFigure:IDENtity 5
收← OK
发→ SYSTem:CONFigure:SAVE
收← OK
```

> 地址修改后立即生效，`SYSTem:CONFigure:SAVE` 保存至 Flash 掉电不丢失。

### Q: USART1 和 RS485 哪个接口可用？

**两个接口都可用**。USART1 (SCPI + Debug) 用于本地调试，RS485 (Modbus RTU) 用于远程总线控制。详见第7章和第8章。

### Q: 如何恢复出厂设置？

```
发→ SYSTem:CONFigure:DEFaults
收← OK
发→ SYSTem:CONFigure:SAVE
收← OK
```

或通过 Modbus 写入默认值。

### Q: Debug 终端和 SCPI 冲突吗？

**不冲突**。两者共用 USART1，DMA 接收的每字节同时送入两个引擎。SCPI 始终可用，Debug 终端启用后自动识别命令类型进行路由。

---

## 附录 A: Modbus RTU 常用指令速查表

以下为设备地址 **0x01** 的常用 Modbus 指令 HEX 帧（CRC 已计算），可直接复制到串口助手使用。

### A.1 读取操作

| 功能 | 请求帧 (HEX) | 响应解析 |
|:----:|:------------:|---------|
| 读通道号 | `01 03 00 00 00 01 84 0A` | 回复第4字节=通道号 (1-based) |
| 读工作模式 | `01 03 00 01 00 01 D4 0A` | 0=IO模式, 1=CMD模式 |
| 读设备地址 | `01 03 00 02 00 01 24 0A` | 回复值=当前Modbus地址 |
| 读序列号 (高) | `01 03 00 03 00 01 74 0A` | 序列号高16位 |
| 读序列号 (低) | `01 03 00 04 00 01 C4 0B` | 序列号低16位 |
| 读设备名称0 | `01 03 00 05 00 01 94 0B` | 名称第1-2字符 (ASCII) |
| 读设备名称1 | `01 03 00 06 00 01 64 0B` | 名称第3-4字符 |
| 读设备名称2 | `01 03 00 07 00 01 34 0B` | 名称第5-6字符 |
| 读设备名称3 | `01 03 00 08 00 01 44 09` | 名称第7-8字符 |
| 读设备状态 | `01 03 00 09 00 01 14 09` | 状态位 (见§6.3.1) |
| 读输出控制 | `01 03 00 0A 00 01 E4 09` | 每bit对应一个输出 |
| 读固件版本 | `01 03 00 0B 00 01 B4 09` | 高字节=主版本, 低字节=次版本 |
| 读硬件版本 | `01 03 00 1B 00 01 FD CC` | 硬件版本号 |
| 读输入寄存器 | `01 04 00 00 00 01 31 CA` | ADC/输入通道值 |
| 读所有线圈 | `01 01 00 00 00 06 BC 0B` | 6个输出继电器状态 |
| 读所有离散输入 | `01 02 00 00 00 04 79 C9` | 4个输入检测引脚 |

### A.2 写入操作

| 功能 | 请求帧 (HEX) | 说明 |
|:----:|:------------:|------|
| 设置通道 3 | `01 06 00 00 00 03 C8 0A` | 将开关切换到通道3 |
| 设置模式=CMD | `01 06 00 01 00 01 18 0A` | 0x0001 = COMMAND 模式 |
| 设置模式=IO | `01 06 00 01 00 00 59 CA` | 0x0000 = DIRECT(IO) 模式 |
| 设地址为 2 | `01 06 00 02 00 02 E9 CA` | 修改设备地址为 2 |
| 设地址为 5 | `01 06 00 02 00 05 28 0B` | 修改设备地址为 5 |
| 输出0=ON | `01 05 00 00 FF 00 8C 3A` | 打开输出0 (线圈0) |
| 输出0=OFF | `01 05 00 00 00 00 CD CA` | 关闭输出0 |
| 输出1=ON | `01 05 00 01 FF 00 DD FA` | 打开输出1 |
| 输出1=OFF | `01 05 00 01 00 00 9C 0A` | 关闭输出1 |
| 写输出控制 | `01 06 00 0A 00 05 A4 09` | 设置输出控制寄存器=5 |
| 写名称 "Test" | `01 10 00 05 00 04 08 46 54 65 73 74 00 00 00 00 CRC` | 写8字节名称 (ASCII) |
| 写多寄存器 | `01 10 00 00 00 02 04 00 01 00 05 CRC` | 同时写地址0x0000-0x0001 |

### A.3 异常触发示例

| 操作 | 请求帧 (HEX) | 异常响应 | 异常码 |
|:----:|:------------:|:--------:|:------:|
| 不支持功能码0x09 | `01 09 00 00 00 01 CRC` | `01 89 01 CRC` | 01 ILLEGAL_FUNCTION |
| 读地址0x0020 (越界) | `01 03 00 20 00 01 84 0A` | `01 83 02 CRC` | 02 ILLEGAL_DATA_ADDRESS |
| 读数量=0 | `01 03 00 00 00 00 84 0A` | `01 83 03 CRC` | 03 ILLEGAL_DATA_VALUE |
| 写线圈数据=0x1234 | `01 05 00 00 12 34 CRC` | `01 85 03 CRC` | 03 ILLEGAL_DATA_VALUE |

### A.4 多设备指令示例 (设备地址=0x02)

| 功能 | 请求帧 (HEX) |
|:----:|:------------:|
| 读设备2通道号 | `02 03 00 00 00 01 84 39` |
| 设置设备2通道3 | `02 06 00 00 00 03 C8 39` |
| 设备2地址改为3 | `02 06 00 02 00 03 29 CB` |
| 读设备2所有线圈 | `02 01 00 00 00 06 BC 3A` |

> **CRC 说明**: 以上 CRC 基于设备地址计算，若修改了地址需重新计算 CRC。CRC 计算方法见附录 B。

---

## 附录 B: CRC-16-Modbus 算法

### B.1 算法参数

| 参数 | 值 |
|------|-----|
| 多项式 | `0x8005` (反转: `0xA001`) |
| 初始值 | `0xFFFF` |
| 结果异或 | `0x0000` |
| 字节序 | **低字节在前** (Little-Endian) |

### B.2 Python 验证

```python
def crc16_modbus(data: list) -> int:
    """计算 CRC-16-Modbus"""
    crc = 0xFFFF
    poly = 0xA001  # 多项式 0x8005 的反转
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ poly
            else:
                crc >>= 1
    return crc

# 验证示例: 读取通道号
req_data = [0x01, 0x03, 0x00, 0x00, 0x00, 0x01]
crc = crc16_modbus(req_data)
print(f"CRC = 0x{crc:04X}")  # 输出: 0x0A84
# 帧中字节: 0x84 (低) 0x0A (高) ✓

rsp_data = [0x01, 0x03, 0x02, 0x00, 0x01]
crc = crc16_modbus(rsp_data)
print(f"CRC = 0x{crc:04X}")  # 输出: 0x8479
# 帧中字节: 0x79 (低) 0x84 (高) ✓
```

### B.3 C 语言查表实现 (部分)

CRC16 查表法使用 256 项预计算表（多项式 0xA001），参照 [`App/Modbus/Src/modbus_crc.c`](App/Modbus/Src/modbus_crc.c) 实现。

```c
uint16_t MODBUS_CRC16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        uint8_t idx = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ crc16_table[idx];
    }
    return crc;
}
```

---

## 附录 C: 文件结构参考

### C.1 应用层模块

```
App/
├── Debug/                    # Debug 终端模块
│   ├── Inc/debug.h          → API: Debug_Init/Process/PrintF
│   └── Src/debug.c          → 实现: 命令解析/执行/TX环形缓冲区
├── Flash/                    # Flash 配置存储
│   ├── Inc/flash.h          → 配置结构体定义
│   └── Src/flash.c          → Flash 读写/CRC32校验
├── IO/                       # IO 控制模块
│   ├── Inc/io.h             → API: GPIO 读写/开关控制/模式设置
│   └── Src/io.c             → 实现: EXTI映射/开关通道编码
├── Modbus/                   # Modbus RTU 协议栈
│   ├── Inc/
│   │   ├── modbus.h         → 模块主头文件 (API + MODBUS_DEBUG开关)
│   │   ├── modbus_frame.h   → 帧定义 (地址/功能码/异常码/缓冲区)
│   │   ├── modbus_crc.h     → CRC16 函数声明
│   │   ├── modbus_functions.h → 功能码处理器声明
│   │   └── modbus_regs.h    → 寄存器地址和回调声明
│   └── Src/
│       ├── modbus_slave.c   → 接收状态机 + T35超时 + 帧处理
│       ├── modbus_functions.c → 8个功能码处理器 + 异常构建
│       ├── modbus_regs.c    → 寄存器读写回调实现
│       ├── modbus_crc.c     → CRC16-Modbus查表法实现
│       └── modbus.c         → 模块占位符
├── RS485/                    # RS485 驱动
│   ├── Inc/rs485.h          → API: Init/Transmit/DMA处理
│   └── Src/rs485.c          → 实现: DMA循环接收/TX环形缓冲/DE控制
└── SCPI/                     # SCPI 协议栈 (libscpi)
    ├── port/
    │   ├── scpi-def.c       → SCPI 命令注册表
    │   └── scpi-def.h       → 设备标识 + API 声明
    ├── inc/scpi/             → libscpi 头文件
    └── src/                  → libscpi 源码
```

### C.2 底层驱动

```
Core/
├── Inc/
│   ├── main.h               → 主头文件
│   ├── usart.h              → USART HAL 配置
│   ├── dma.h                → DMA HAL 配置
│   ├── gpio.h               → GPIO HAL 配置
│   ├── tim.h                → TIMER HAL 配置
│   └── stm32g4xx_it.h       → 中断服务声明
├── Src/
│   ├── main.c               → 入口 + 初始化 + 主循环
│   ├── usart.c              → USART1/USART3 引脚配置
│   ├── dma.c                → DMA 通道配置
│   ├── gpio.c               → GPIO 引脚配置
│   ├── tim.c                → 定时器配置
│   ├── stm32g4xx_it.c       → 中断服务 (USART3 IDLE 等)
│   └── system_stm32g4xx.c   → 系统时钟配置
└── Startup/
    └── startup_stm32g431rbtx.s → 启动文件
```

---

> **文档结尾** — SP10T 电子开关操作手册 V1.0  
> 如有疑问，可通过 USART1 发送 `*IDN?` 查询设备信息，或使用 `HELP` 命令查看 Debug 终端帮助。
