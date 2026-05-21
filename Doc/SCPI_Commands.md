# NetBus SCPI 指令表 (IEEE 488.2 / SCPI-1999)

## 概述

本文档定义了 NetBus 控制板的 SCPI (Standard Commands for Programmable Instruments) 指令集，遵循 **IEEE 488.2** 和 **SCPI-1999** 协议规范。

### 传输层

| 层级 | 实现 |
|------|------|
| 物理层 | UART7 (PF6-RX, PF7-TX) @ 115200 8N1 |
| 链路层 | 纯文本行协议，`<LF>` / `<CR><LF>` 行终止 |
| 应用层 | SCPI ASCII 指令集 |

### 语法约定

| 约定 | 含义 | 示例 |
|------|------|------|
| `COMMand` | 长格式（大写部分为缩写） | `ATTenuator` 可缩写为 `ATT` |
| `[ ]` | 可选关键字/参数 | `[PERiod]` 可省略 |
| `< >` | 必选参数 | `<value>` 必须提供 |
| `|` | 互斥选项 | `ON|OFF` 二选一 |
| `?` | 查询后缀 | `SN?` 读取序列号 |
| `(@<list>)` | 通道列表 | `(@1,2,3)` 节点 1,2,3 |

---

## 一、IEEE 488.2 公用命令

所有兼容 IEEE 488.2 的仪器必须实现的命令。

### *IDN? — 标识查询

```scpi
*IDN?
```

**响应格式：**
```
<制造商>,<型号>,<序列号>,<固件版本>
```

**示例响应：**
```
NetBus,PPA-NB100,PINPRFB00300000038,v1.0.0
```

| 字段 | 说明 |
|------|------|
| 制造商 | 固定 `NetBus` |
| 型号 | `PPA-NB100` |
| 序列号 | 主机唯一序列号 |
| 固件版本 | 语义版本 `v<major>.<minor>.<patch>` |

### *RST — 复位

```scpi
*RST
```

将所有子系统恢复为出厂默认状态：
- 衰减器 A/B → 0
- 停止所有检波操作
- RF 开关通道 → 1
- 清除状态寄存器 (STB, ESR)
- 取消所有 OTA/校准/SN 写状态

### *TST? — 自检

```scpi
*TST?
```

执行上电自检 (POST)，返回结果码：

| 返回值 | 含义 |
|--------|------|
| 0 | 全部通过 |
| 1 | CAN 总线故障 |
| 2 | RS485 故障 |
| 4 | 外部 Flash 故障 |
| 8 | EEPROM 校验错误 |

### *CLS — 清除状态

```scpi
*CLS
```

清除状态字节寄存器 (STB)、事件状态寄存器 (ESR) 和错误队列。

### *ESE / *ESE? — 事件状态使能

```scpi
*ESE <mask>
*ESE?
```

设置/查询标准事件状态使能寄存器。

### *ESR? — 事件状态寄存器

```scpi
*ESR?
```

查询并清除标准事件状态寄存器。

| 位 | 权重 | 含义 |
|----|------|------|
| 0 | 1 | 操作完成 (OPC) |
| 2 | 4 | 查询错误 (QYE) |
| 3 | 8 | 设备相关错误 (DDE) |
| 4 | 16 | 执行错误 (EXE) |
| 5 | 32 | 命令错误 (CME) |
| 7 | 128 | 上电 (PON) |

### *SRE / *SRE? — 服务请求使能

```scpi
*SRE <mask>
*SRE?
```

设置/查询服务请求使能寄存器。

### *STB? — 状态字节

```scpi
*STB?
```

查询状态字节寄存器。

| 位 | 权重 | 含义 |
|----|------|------|
| 3 | 8 | 可疑状态摘要 |
| 4 | 16 | 消息可用 (MAV) |
| 5 | 32 | 事件状态位摘要 (ESB) |
| 6 | 64 | 主状态摘要 (MSS) |
| 7 | 128 | 操作状态摘要 |

### *OPC / *OPC? — 操作完成

```scpi
*OPC
*OPC?
```

`*OPC` 在完成所有待处理操作后将 ESR 的 OPC 位置 1。
`*OPC?` 阻塞至全部操作完成，返回 `1`。

### *WAI — 等待继续

```scpi
*WAI
```

等待所有待处理操作完成后才执行下一条命令。

---

## 二、系统子系统 — SYSTem

### 树形结构

```
SYSTem
├── HELP?                         帮助信息
├── VERSion?                      固件版本
├── SN?                           主机序列号
├── ERRor[:NEXT]?                 错误队列
├── ATTenuator                    本地衰减器 (DAC)
│   ├── A <value>                 0-15
│   ├── A?                        [MIN|MAX]
│   ├── B <value>                 0-15
│   └── B?                        [MIN|MAX]
├── COMMunicate
│   ├── CAN
│   │   ├── SEND <id>,<data>      CAN 帧发送
│   │   └── SCAN? [start],[end]   CAN 总线扫描
│   └── MODBus
│       └── SEND <hex bytes>      原生 Modbus RTU 帧
└── DETector
    └── [<node>]                  检波板管理 (CAN A1)
        ├── SN?                   序列号
        ├── VERSion?              固件版本
        ├── RESet                 复位 MCU
        ├── LED <state>[,<period>] LED 控制
        ├── ADDRess <new>|RESet   修改/复位 Node ID
        ├── FLASh
        │   ├── INFO?             Flash 布局信息
        │   └── DATA? <addr>,<len> 读取 Flash 数据
        ├── CALibration ENTer|EXIT 校准模式
        ├── OTA:PREPare <maj>,<min>,<pat> OTA 准备
        └── SNWRite <sn_string>   写入序列号
```

### SYSTem:HELP?

```scpi
SYSTem:HELP?
```

返回所有顶层命令的简短描述列表。等效于 Debug CLI 的 `help`。

### SYSTem:VERSion?

```scpi
SYSTem:VERSion?
```

返回 SCPI 协议版本号（字符串）。

**示例响应：** `1999.0`

### SYSTem:SN?

```scpi
SYSTem:SN?
```

返回主控板序列号。

**示例响应：** `"PINPRFB00300000038"`

### SYSTem:ERRor[:NEXT]?

```scpi
SYSTem:ERRor?
SYSTem:ERRor:NEXT?
```

从错误队列中取出最早的错误。

**响应格式：** `<code>,"<description>"`

| 代码 | 描述 |
|------|------|
| 0 | "No error" |
| -100 | "Command error" |
| -200 | "Execution error" |
| -220 | "Parameter error" |
| -222 | "Data out of range" |
| -310 | "System error" |
| -410 | "Query INTERRUPTED" |

### SYSTem:ATTenuator:A / B

```scpi
SYSTem:ATTenuator:A <value>
SYSTem:ATTenuator:A? [MIN|MAX]
SYSTem:ATTenuator:B <value>
SYSTem:ATTenuator:B? [MIN|MAX]
```

控制主控板两路 4 位数字衰减器。

| 参数 | 范围 | 说明 |
|------|------|------|
| `<value>` | 0–15 | 衰减值 (NR1) |
| MIN | — | 查询最小值 (0) |
| MAX | — | 查询最大值 (15) |

**示例：**
```scpi
SYST:ATT:A 5          → 衰减器A = 5
SYST:ATT:B?           → 10
SYST:ATT:A? MAX       → 15
```

**硬件对应：**

| 衰减器A | 引脚 |
|---------|------|
| bit0 | PC8 |
| bit1 | PC9 |
| bit2 | PA8 |
| bit3 | PA9 |

| 衰减器B | 引脚 |
|---------|------|
| bit0 | PD2 |
| bit1 | PC12 |
| bit2 | PC11 |
| bit3 | PC10 |

> **等效 Debug CLI:** `att a <v>`, `att b <v>`, `att a ?`, `att b ?`

### SYSTem:COMMunicate:CAN:SEND

```scpi
SYSTem:COMMunicate:CAN:SEND <id>,<data_byte1>[,<data_byte2>...]
```

通过 FDCAN1 发送 CAN 帧。数据长度 ≤8 字节自动使用 Classic CAN，>8 字节使用 CAN FD (with BRS)。

| 参数 | 范围 | 说明 |
|------|------|------|
| `<id>` | 0x000–0x7FF | CAN 标识符 |
| `<data_byteN>` | 0x00–0xFF | 数据字节 |

发送后自动等待 500ms 并收集所有响应帧。

**示例：**
```scpi
SYST:COMM:CAN:SEND 0x101,0x01          → 查询节点1的SN
SYST:COMM:CAN:SEND 0x123,0x01,0x02,0x03  → 3字节数据帧
```

**响应 (Query):**
```scpi
SYST:COMM:CAN:SEND? 0x101,0x01
# 返回: <resp_count>,<id1>,<dlc1>,<hex_data1>,...
```

> **等效 Debug CLI:** `can send <id> <hex...>`

### SYSTem:COMMunicate:CAN:SCAN?

```scpi
SYSTem:COMMunicate:CAN:SCAN? [<start>[,<end>]]
```

扫描 CAN 总线上的活动节点 (发送 0x101 SN 查询)。

| 参数 | 默认值 | 范围 |
|------|--------|------|
| `<start>` | 1 | 1–255 |
| `<end>` | 40 | 1–255 |

**响应格式：**
```
<node_count>,<node1>,<sn1_len>,<sn1_string>,<node2>,...
```

**示例：**
```scpi
SYST:COMM:CAN:SCAN?
→ 1,1,18,"PINPRFB00300000038"

SYST:COMM:CAN:SCAN? 1,10
→ 2,1,18,"PINPRFB00300000038",3,18,"PINPRFB00300000042"
```

> **等效 Debug CLI:** `can scan [start] [end]`

### SYSTem:COMMunicate:MODBus:SEND

```scpi
SYSTem:COMMunicate:MODBus:SEND <addr>,<fc>,<data_byte1>[,<data_byte2>...]
```

通过 RS485 (UART8) 发送原始 Modbus RTU 帧。CRC16 由固件自动追加。发送后自动等待响应。

| 参数 | 范围 | 说明 |
|------|------|------|
| `<addr>` | 1–247 | 从站地址 |
| `<fc>` | 1–255 | 功能码 |
| `<data_byteN>` | 0x00–0xFF | 数据字节 |

**示例：**
```scpi
SYST:COMM:MODB:SEND 1,3,0x00,0x00,0x00,0x01  → FC=03 读寄存器0x0000
SYST:COMM:MODB:SEND 1,6,0x00,0x00,0x00,0x05  → FC=06 写通道5
```

**响应 (Query):**
```scpi
SYST:COMM:MODB:SEND? 1,3,0x00,0x00,0x00,0x01
→ #21701 03 02 00 01 79 84\n
```
（`#2` 后的 `17` 表示17字节十六进制数据）

> **等效 Debug CLI:** `modbus <hex bytes...>`

---

## 三、检测子系统 — SENSe

```
SENSe
└── DETector
    └── <node>                     节点 ID (1–255)
        ├── CONTrol <mode>,<hold>,<thr>[,<gate>]  检波控制
        ├── STOP                                      停止检波
        ├── TEMPerature? [<det>[,<mcu>]]              温度读取
        ├── POWer? <khz>,<hold>,<mode>,<thr>,<gate>   功率查询
        ├── BAND <mhz>,<mode>,<mask>                  频段选择
        └── ATTenuator <a1>,<a2>,<a3>,<a4>,<a5>,<a6>,<a7>  衰减器链
```

> `<node>` 使用通道列表语法 `(@<node>)` 或数字参数。0xFF (255) = 广播。

### SENSe:DETector:<node>:CONTrol

```scpi
SENSe:DETector<node>:CONTrol <mode>,<hold>,<thr>[,<gate>]
```

触发检波测量 (CAN 0x110)。同步等待完成，返回 H/V 通道 ADC 均值。

| 参数 | 范围 | 说明 |
|------|------|------|
| `<mode>` | 0–2 | 0=定时, 1=触发, 2=脉冲 |
| `<hold>` | 0–255 | 保持时间 (ms) |
| `<thr>` | 0–65535 | 阈值 (16bit) |
| `<gate>` | 0–65535 | 门限时间 (ms, 16bit), 默认 0 |

**查询响应格式：**
```
<hadc>,<vadc>,<mode_echo>,<state>
```

| 字段 | 说明 |
|------|------|
| `<hadc>` | H 通道 ADC 均值 (int16) |
| `<vadc>` | V 通道 ADC 均值 (int16) |
| `<state>` | 0=完成, 1=触发已就绪, 2=触发超时 |

**示例：**
```scpi
SENS:DET1:CONT 0,100,500,1000
→ 2048,-512,0,0
```

> **等效 Debug CLI:** `detector control <node> <mode> <hold> <thr> [gate]`
> **CAN 协议:** 0x110

### SENSe:DETector:<node>:STOP

```scpi
SENSe:DETector<node>:STOP
```

停止当前节点的检波操作 (CAN 0x112)。

> **等效 Debug CLI:** `detector stop <node_id>`

### SENSe:DETector:<node>:TEMPerature?

```scpi
SENSe:DETector<node>:TEMPerature? [<det>[,<mcu>]]
```

读取检波板温度传感器 (CAN 0x116)。

| 参数 | 范围 | 说明 |
|------|------|------|
| `<det>` | 0|1 | 读取检波板温度 (默认1) |
| `<mcu>` | 0|1 | 读取 MCU 温度 (默认1) |

**响应格式：** `<det_temp>,<mcu_temp>,<rsv>`
- 温度值为原始 ADC 读数 (int16)，单位：需校准

**示例：**
```scpi
SENS:DET1:TEMP? 1,1
→ -28664,29193,0
```

> **等效 Debug CLI:** `detector temp <node_id> [det] [mcu]`

### SENSe:DETector:<node>:POWer?

```scpi
SENSe:DETector<node>:POWer? <khz>,<hold>,<mode>,<thr>,<gate>
```

检波功率查询 (CAN 0x11B, CAN FD 12字节帧)。执行一次检波并返回 dBm 功率值。

| 参数 | 范围 | 说明 |
|------|------|------|
| `<khz>` | 0–16777215 | 频率 (kHz, 24bit) |
| `<hold>` | 0–255 | 保持时间 (ms) |
| `<mode>` | 0–2 | 测量模式 |
| `<thr>` | 0–65535 | 阈值 |
| `<gate>` | 0–65535 | 门限时间 (ms) |

**响应格式：** `<h_pwr_dbm100>,<v_pwr_dbm100>,<mode_echo>,<state>`
- 功率单位为 dBm × 100 (int16)

**示例：**
```scpi
SENS:DET1:POW? 2450000,100,0,500,1000
→ -4523,-3890,0,0    (即 -45.23 dBm, -38.90 dBm)
```

> **等效 Debug CLI:** `detector power <node> <khz> <hold> <mode> <thr> <gate>`

### SENSe:DETector:<node>:BAND

```scpi
SENSe:DETector<node>:BAND <mhz>,<mode>,<mask>
```

设置检波频段选择 (CAN 0x115)，通过 40bit 位掩码使能 20 组 H/V 通道。

| 参数 | 范围 | 说明 |
|------|------|------|
| `<mhz>` | 0–255 | 频段基准频率 (MHz) |
| `<mode>` | 0|1 | 0=单频点, 1=扫频 |
| `<mask>` | 0–0xFFFFFFFFFF | 40bit 位掩码 (十进制或十六进制) |

**位掩码编码：** bit0=1H, bit1=1V, bit2=2H, bit3=2V, ..., bit38=20H, bit39=20V

**查询响应：** `<mode_echo>,<v_selected>,<h_selected>`

**示例：**
```scpi
SENS:DET1:BAND 100,0,#H3FF   → 使能 1H,1V,2H,2V,3H,3V (10bit)
```

> **等效 Debug CLI:** `detector band <node> <mhz> <mode> <mask>`

### SENSe:DETector:<node>:ATTenuator

```scpi
SENSe:DETector<node>:ATTenuator <a1>,<a2>,<a3>,<a4>,<a5>,<a6>,<a7>
```

手动设置检波板 7 级衰减器链 (CAN 0x113)。

| 参数 | 范围 | 说明 |
|------|------|------|
| `<a1>,<a2>,<a3>` | 0–15 | ATT1/2/3 (≥16保持当前) |
| `<a4>,<a5>,<a7>` | 0/7/8/15 | ATT4/5/7 (≥16保持当前) |
| `<a6>` | 0|1 | 1.7G 滤波器旁路 (≥2保持当前) |

**示例：**
```scpi
SENS:DET1:ATT 0,7,8,15,0,1,0
```

> **等效 Debug CLI:** `detector att <node> <a1>..<a7>`

---

## 四、信号源子系统 — SOURce

```
SOURce
└── DETector
    └── <node>                     节点 ID (1–255, 255=广播)
        ├── FREQuency <ch>,<khz>[,<pwr>]    VCO 频率设置
        └── POWer <ch>,<khz>,<dbm100>[,<gps>][,<comp>]  发射功率
```

### SOURce:DETector:<node>:FREQuency

```scpi
SOURce:DETector<node>:FREQuency <ch>,<khz>[,<pwr>]
```

设置检波板 VCO 频率 (CAN 0x114)。

| 参数 | 范围 | 说明 |
|------|------|------|
| `<ch>` | 0–2 | 0=关, 1=V, 2=H |
| `<khz>` | 0–4294967295 | 基准频率 (kHz, 32bit) |
| `<pwr>` | 0–63 | LMX 输出功率 (可选, 默认最小) |

**查询响应：** `<ch_echo>,<actual_khz>,<pwr_echo>`

**示例：**
```scpi
SOUR:DET1:FREQ 1,2450000,31    → V通道, 2450.000 MHz, LMX=31
SOUR:DET255:FREQ 2,1000000     → 广播: H通道, 1000.000 MHz
```

> **等效 Debug CLI:** `detector freq <node> <ch> <khz> [pwr]`

### SOURce:DETector:<node>:POWer

```scpi
SOURce:DETector<node>:POWer <ch>,<khz>,<dbm100>[,<gps>][,<comp>]
```

闭环发射功率设置 (CAN 0x11C, CAN FD 12字节帧)。频率 → 查校准表 → 计算衰减值 → 下发 ATT。

| 参数 | 范围 | 说明 |
|------|------|------|
| `<ch>` | 0–2 | 通道: 0=关, 1=V, 2=H |
| `<khz>` | 0–4294967295 | 目标频率 (kHz) |
| `<dbm100>` | -32768–32767 | 目标功率 (dBm × 100) |
| `<gps>` | 0|1 | GPS 标志 (默认0), 1时 ATT6 强制为1 |
| `<comp>` | 0|1 | 线损补偿 (默认0) |

**查询响应：** `<ch_echo>,<actual_khz>,<att1>..<att7>,<predicted_dbm100>`

**示例：**
```scpi
SOUR:DET1:POW 1,2450000,-1000,0,1    → V通道, 2450MHz, -10.00dBm, 补偿线损
```

> **等效 Debug CLI:** `detector txpower <node> <ch> <khz> <dbm100> [gps] [comp]`

---

## 五、路由子系统 — ROUTe

```
ROUTe
├── DETector
│   └── <node>
│       └── SWITch <s1>,<s2>,<s3>,<s4>,<s5>,<s6>  开关控制 (0x111)
└── SWITch
    └── <addr>                     Modbus 地址 (1–247)
        ├── CHANnel?               读取通道 (FC=03, REG 0x0000)
        ├── CHANnel <ch>           设置通道 (FC=06, REG 0x0000)
        ├── MODE?                  读取工作模式 (REG 0x0001)
        ├── MODE IO|CMD            设置工作模式
        ├── IDENtity?              完整设备信息 (FC=03 x10)
        ├── OUTPut?                读取全部6路输出线圈 (FC=01)
        ├── OUTPut <id>,<state>    设置单路输出线圈 (FC=05)
        ├── INPut?                 读取4路离散输入 (FC=02)
        ├── ADDRess <new_id>       修改设备 Modbus 地址 (REG 0x0020)
        └── CONDition?             设备状态寄存器 (REG 0x0020)
```

### ROUTe:DETector:<node>:SWITch

```scpi
ROUTe:DETector<node>:SWITch <s1>,<s2>,<s3>,<s4>,<s5>,<s6>
```

控制检波板 6 路开关 (CAN 0x111)。

| 参数 | 范围 | 说明 |
|------|------|------|
| `<s1>..<s6>` | 0|1 | SW1–SW6, ≥2=保持当前 |

**示例：**
```scpi
ROUT:DET1:SW 1,0,1,0,1,0
```

> **等效 Debug CLI:** `detector switch <node> <s1>..<s6>`

### ROUTe:SWITch:<addr>:CHANnel

```scpi
ROUTe:SWITch<addr>:CHANnel?
ROUTe:SWITch<addr>:CHANnel <ch>
```

读取/设置 SP10T RF 开关的当前通道 (Modbus RTU over RS485)。

| 参数 | 范围 | 说明 |
|------|------|------|
| `<addr>` | 1–247 | 从站 Modbus 地址 |
| `<ch>` | 1–10 | 通道号 |

**示例：**
```scpi
ROUT:SWIT1:CHAN?        → 1
ROUT:SWIT1:CHAN 5       → 切换到通道 5
```

> **等效 Debug CLI:** `rfsw get <addr>`, `rfsw set <addr> <ch>`

### ROUTe:SWITch:<addr>:MODE

```scpi
ROUTe:SWITch<addr>:MODE?
ROUTe:SWITch<addr>:MODE <mode>
```

读取/设置 RF 开关的工作模式。

| `<mode>` | 含义 |
|----------|------|
| `IO` | IO DIRECT — 硬件引脚直控 |
| `CMD` | COMMAND — Modbus 命令控制 |

**示例：**
```scpi
ROUT:SWIT1:MODE?        → 0    (IO DIRECT)
ROUT:SWIT1:MODE CMD     → 切换为命令模式
```

> **等效 Debug CLI:** `rfsw mode <addr>`, `rfsw mode <addr> io|cmd`

### ROUTe:SWITch:<addr>:IDENtity?

```scpi
ROUTe:SWITch<addr>:IDENtity?
```

一次性读取 RF 开关的完整身份信息 (FC=03, REG 0x0000–0x0009)。

**响应格式 (多行/块数据)：**
```
"<name>",<serial>,<modbus_id>,<fw_version>,<hw_version>,<status_hex>,<output_ctrl_hex>,<channel>,<mode>
```

**示例：**
```scpi
ROUT:SWIT1:IDEN?
→ "PPA-SP10",0,1,"v0.0",16,#H5400,#H0000,1,"IO DIRECT (0)"
```

> **等效 Debug CLI:** `rfsw info <addr>`

### ROUTe:SWITch:<addr>:OUTPut

```scpi
ROUTe:SWITch<addr>:OUTPut?
ROUTe:SWITch<addr>:OUTPut <id>,<state>
```

查询全部 6 路/设置单路输出线圈状态 (FC=01/FC=05)。

| 参数 | 范围 | 说明 |
|------|------|------|
| `<id>` | 0–5 | 线圈编号 (0=S0_CA, 1=S0_CB, ..., 5=S2_CB) |
| `<state>` | 0|1 | OFF|ON |

**查询响应：** `<s0ca>,<s0cb>,<s1ca>,<s1cb>,<s2ca>,<s2cb>` (各为 0/1)

**示例：**
```scpi
ROUT:SWIT1:OUTP?         → 0,1,1,1,0,0
ROUT:SWIT1:OUTP 0,1      → 设置 S0_CA = ON
```

> **等效 Debug CLI:** `rfsw outputs <addr>`, `rfsw output <addr> <id> <0|1>`

### ROUTe:SWITch:<addr>:INPut?

```scpi
ROUTe:SWITch<addr>:INPut?
```

读取 4 路离散输入引脚状态 (FC=02)。

**响应格式：** `<ctrl1>,<ctrl2>,<ctrl3>,<ctrl4>` (各为 0=LOW, 1=HIGH)

> **等效 Debug CLI:** `rfsw inputs <addr>`

### ROUTe:SWITch:<addr>:ADDRess

```scpi
ROUTe:SWITch<addr>:ADDRess <new_id>
```

修改从机的 Modbus 地址 (FC=06, REG 0x0020)。

| 参数 | 范围 | 说明 |
|------|------|------|
| `<new_id>` | 1–247 | 新地址 |

> **警告:** 地址修改立即生效。需通过 `SYSTem:SWITch<addr>:SAVE` 保存到 Flash。

> **等效 Debug CLI:** `rfsw id <addr> <new_id>`

### ROUTe:SWITch:<addr>:CONDition?

```scpi
ROUTe:SWITch<addr>:CONDition?
```

读取设备状态寄存器 (REG 0x0020)。

**响应格式：** `<status_hex>,<init_done>,<comm_active>`

| 字段 | 含义 |
|------|------|
| `<status_hex>` | 原始 16bit 值 |
| `<init_done>` | 0|1 初始化完成 |
| `<comm_active>` | 0|1 通信活跃 |

> **等效 Debug CLI:** `rfsw status <addr>`

---

## 六、状态子系统 — STATus

遵循 SCPI-1999 标准状态报告模型。

```
STATus
├── OPERation[:EVENt]?
├── OPERation:ENABle <mask>
├── OPERation:ENABle?
├── QUEStionable[:EVENt]?
├── QUEStionable:ENABle <mask>
├── QUEStionable:ENABle?
├── SWITch<addr>:CONDition?   (→ 等同 ROUT:SWIT<addr>:COND?)
└── DETector<node>:CONDition?
```

### STATus:OPERation[:EVENt]?

```scpi
STATus:OPERation?
STATus:OPERation:EVENt?
```

读取操作状态事件寄存器。

| 位 | 权重 | 含义 |
|----|------|------|
| 0 | 1 | 校准进行中 (CALibrating) |
| 2 | 4 | 测量进行中 (MEASuring) |
| 4 | 16 | CAN 总线忙 (CAN Busy) |
| 5 | 32 | RS485 忙 (Modbus Busy) |
| 8 | 256 | OTA 进行中 |

### STATus:QUEStionable[:EVENt]?

```scpi
STATus:QUEStionable?
STATus:QUEStionable:EVENt?
```

读取可疑状态事件寄存器。

| 位 | 权重 | 含义 |
|----|------|------|
| 0 | 1 | CAN 通信超时 |
| 1 | 2 | Modbus 通信超时 |
| 2 | 4 | Modbus 异常响应 |
| 9 | 512 | 外部 Flash 操作失败 |

---

## 七、调试子系统 — DIAGnostic

仅供调试和诊断使用，生产环境中可能禁用。

```
DIAGnostic
├── MODBus:SEND <addr>,<fc>,<data...>  同名 SYST:COMM:MODB:SEND
├── CAN:SEND <id>,<data...>            同名 SYST:COMM:CAN:SEND
├── CAN:SCAN? [start],[end]            同名 SYST:COMM:CAN:SCAN?
└── ECHO <0|1>                         串口回显开关
```

### DIAGnostic:ECHO

```scpi
DIAGnostic:ECHO <state>
DIAGnostic:ECHO?
```

控制串口回显。关闭回显可用于程序化控制时减少噪声。

| `<state>` | 说明 |
|-----------|------|
| 0 | OFF — 不回显 |
| 1 | ON — 回显 (默认) |

> **等效 Debug CLI:** 固件 `Log_DbgSetEcho(0/1)`

---

## 八、Debug CLI → SCPI 对照表

| Debug CLI | SCPI 指令 | CAN/Modbus |
|-----------|-----------|------------|
| `help` | `SYSTem:HELP?` | — |
| `att a <v>` | `SYSTem:ATTenuator:A <v>` | — |
| `att a ?` | `SYSTem:ATTenuator:A?` | — |
| `att b <v>` | `SYSTem:ATTenuator:B <v>` | — |
| `att b ?` | `SYSTem:ATTenuator:B?` | — |
| `att ?` | `SYST:ATT:A?;:SYST:ATT:B?` | — |
| `rfsw get <a>` | `ROUTe:SWITch<a>:CHANnel?` | FC=03 R0x0000 |
| `rfsw set <a> <ch>` | `ROUTe:SWITch<a>:CHANnel <ch>` | FC=06 R0x0000 |
| `rfsw mode <a>` | `ROUTe:SWITch<a>:MODE?` | FC=03 R0x0001 |
| `rfsw mode <a> io\|cmd` | `ROUTe:SWITch<a>:MODE IO\|CMD` | FC=06 R0x0001 |
| `rfsw info <a>` | `ROUTe:SWITch<a>:IDENtity?` | FC=03 R0x0000 x10 |
| `rfsw output <a> <id> <s>` | `ROUTe:SWITch<a>:OUTPut <id>,<s>` | FC=05 |
| `rfsw outputs <a>` | `ROUTe:SWITch<a>:OUTPut?` | FC=01 |
| `rfsw inputs <a>` | `ROUTe:SWITch<a>:INPut?` | FC=02 |
| `rfsw status <a>` | `ROUTe:SWITch<a>:CONDition?` | FC=03 R0x0020 |
| `rfsw id <a> <new>` | `ROUTe:SWITch<a>:ADDRess <new>` | FC=06 R0x0020 |
| `modbus <hex...>` | `SYST:COMM:MODB:SEND <addr>,<fc>,<data...>` | Raw Modbus |
| `can` | `SYSTem:HELP? CAN` | — |
| `can send <id> <hex...>` | `SYST:COMM:CAN:SEND <id>,<data...>` | CAN 0x101 etc. |
| `can scan [s] [e]` | `SYST:COMM:CAN:SCAN? [s],[e]` | CAN 0x101 |
| `detector` | `SYSTem:HELP? DETector` | — |
| `detector sn <n>` | `SYST:DET<n>:SN?` | CAN 0x101 |
| `detector version <n>` | `SYST:DET<n>:VERSion?` | CAN 0x103 |
| `detector reset <n>` | `SYST:DET<n>:RESet` | CAN 0x104 |
| `detector led <n> <s> [p]` | `SYST:DET<n>:LED <s>[,<p>]` | CAN 0x105 |
| `detector nodeid <n> <new>` | `SYST:DET<n>:ADDRess <new>` | CAN 0x102 |
| `detector nodeid <n> reset` | `SYST:DET<n>:ADDRess RESet` | CAN 0x102 |
| `detector writesn <n> <sn>` | `SYST:DET<n>:SNWRite <sn>` | CAN 0x106 |
| `detector control <n> ...` | `SENS:DET<n>:CONTrol ...` | CAN 0x110 |
| `detector stop <n>` | `SENS:DET<n>:STOP` | CAN 0x112 |
| `detector switch <n> ...` | `ROUT:DET<n>:SWITch ...` | CAN 0x111 |
| `detector att <n> ...` | `SENS:DET<n>:ATTenuator ...` | CAN 0x113 |
| `detector freq <n> ...` | `SOUR:DET<n>:FREQuency ...` | CAN 0x114 |
| `detector band <n> ...` | `SENS:DET<n>:BAND ...` | CAN 0x115 |
| `detector temp <n> ...` | `SENS:DET<n>:TEMPerature? ...` | CAN 0x116 |
| `detector flash <n>` | `SYST:DET<n>:FLASh:INFO?` | CAN 0x117 |
| `detector readflash <n> ...` | `SYST:DET<n>:FLASh:DATA? ...` | CAN 0x11A |
| `detector power <n> ...` | `SENS:DET<n>:POWer? ...` | CAN 0x11B |
| `detector txpower <n> ...` | `SOUR:DET<n>:POWer ...` | CAN 0x11C |
| `detector cal <n> enter` | `SYST:DET<n>:CALibration ENTer` | CAN 0x120 |
| `detector cal <n> exit` | `SYST:DET<n>:CALibration EXIT` | CAN 0x121 |
| `detector ota prepare <n> ...` | `SYST:DET<n>:OTA:PREPare ...` | CAN 0x1A0 |

---

## 九、编程示例

### 示例 1: 系统上电初始化

```scpi
*CLS                              // 清除状态
*RST                              // 复位到默认值
*IDN?                             // 确认连接
→ NetBus,PPA-NB100,PINPRFB00300000038,v1.0.0
*TST?                             // 自检
→ 0                               // 全部通过
```

### 示例 2: 读取衰减器并设置

```scpi
SYST:ATT:A?                       // 查询A路
→ 0
SYST:ATT:A 7                      // 设置A路=7
SYST:ATT:A?                       // 验证
→ 7
SYST:ATT:B 15;:SYST:ATT:A?        // 组合命令: 设置B=15, 查询A
→ 7
```

### 示例 3: CAN 总线扫描并查询节点 SN

```scpi
SYST:COMM:CAN:SCAN?               // 扫描1-40号节点
→ 1,1,18,"PINPRFB00300000038"
SYST:DET1:SN?                     // 查询节点1序列号
→ "PINPRFB00300000038"
SYST:DET1:VERS?                   // 查询固件版本
→ "3.1.21"
```

### 示例 4: 读取 RF 开关信息

```scpi
ROUT:SWIT1:IDEN?                  // 读取设备1全部信息
→ "PPA-SP10",0,1,"v0.0",16,21504,0,1,"IO DIRECT (0)"
ROUT:SWIT1:CHAN?                  // 读取当前通道
→ 1
ROUT:SWIT1:OUTP?                  // 读取输出线圈
→ 0,1,1,1,0,0
```

### 示例 5: 检波功率测量

```scpi
SENS:DET1:TEMP? 1,1               // 读取温度
→ -28664,29193,0
SENS:DET1:BAND 100,0,#H3FF        // 设置频段
SENS:DET1:CONT 0,100,500,1000     // 执行检波 (定时模式)
→ 2048,-512,0,0
SENS:DET1:POW? 2450000,100,0,500,1000  // 功率查询 @ 2450MHz
→ -4523,-3890,0,0                 // -45.23 dBm, -38.90 dBm
```

### 示例 6: OTA 升级流程

```scpi
SYST:DET1:OTA:PREP 3,1,22         // 准备OTA: v3.1.22
→ 0                               // 可进入OTA
// ... 数据块传输 (0x1A1, 0x300-0x3FF) ...
*OPC?                             // 等待完成
→ 1
SYST:DET1:VERS?                   // 验证版本
→ "3.1.22"
```

### 示例 7: 错误处理

```scpi
SYST:DET99:SN?                    // 查询不存在的节点
→ -200,"Execution error; Node 99 timeout"
SYST:ERR?                         // 读取错误队列
→ -200,"Node 99: No response"
SYST:ERR?                         // 再次读取
→ 0,"No error"                    // 队列已空
```

---

## 附录 A: 参数类型

| 类型 | 说明 | 示例 |
|------|------|------|
| `<NR1>` | 整数 | `5`, `-100`, `255` |
| `<NR2>` | 定点数 | `3.14`, `-40.5` |
| `<NR3>` | 浮点数 | `2.45E9` |
| `<NRf>` | NR1/NR2/NR3 任一 | `1.0`, `100` |
| `<Bool>` | 布尔 | `0\|1`, `ON\|OFF` |
| `<String>` | 字符串 | `"PINPRFB00300000038"` |
| `<Block>` | 任意块数据 | `#216<16 bytes hex>` |
| `<NonDec>` | 非十进制 | `#H1A`, `#Q777`, `#B1010` |

## 附录 B: 状态报告模型

```
        ┌──────────────┐
        │  Error Queue │ ← SYST:ERR?
        └──────────────┘

┌─────────────────────────┐      ┌──────────────────────┐
│  STAT:OPER:EVENt?       │      │ STAT:QUES:EVENt?     │
│  STAT:OPER:ENABle       │      │ STAT:QUES:ENABle     │
│  STAT:OPER:CONDition?   │      │ STAT:QUES:CONDition? │
└───────────┬─────────────┘      └──────────┬───────────┘
            │                               │
            ▼                               ▼
     ╔══════════════╗               ╔══════════════╗
     ║    ESR       ║               ║    STB       ║
     ║  *ESR?       ║               ║  *STB?       ║
     ║  *ESE        ║               ║  *SRE        ║
     ╚══════════════╝               ╚══════════════╝
```

---

## 参考资料

- [IEEE 488.2-1992] IEEE Standard Codes, Formats, Protocols, and Common Commands
- [SCPI-1999] Standard Commands for Programmable Instruments, Version 1999.0
- [NI-488.2] NI-488.2™ User Manual, National Instruments
- Debug CLI 指令表: `Doc/Debug_CLI_Commands.md`
- A1 检波板 CAN 指令表: `Doc/A1_Detector_CAN_Commands.md`
- RF Switch 命令参考: `Doc/RF_Switch_Commands.md`
