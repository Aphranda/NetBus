# 调试命令行指令表

## 概述

调试命令行接口 (Debug CLI) 集成在 Log 模块中，通过 UART7 (PF6-RX, PF7-TX @ 115200 8N1) 与串口终端交互。

### 连接参数

| 参数     | 值        |
| -------- | --------- |
| 接口     | UART7     |
| TX 引脚  | PF7       |
| RX 引脚  | PF6       |
| 波特率   | 115200    |
| 数据位   | 8         |
| 停止位   | 1         |
| 校验     | 无        |
| 流控制   | 无        |

### 终端操作

- 输入命令后回车 (`Enter`) 发送
- 退格键 (`Backspace`) 删除上一个字符
- 回车换行后将发送命令至设备解析执行

### 命令总览

```text
=== Debug CLI Commands ===
  help      — list commands
  att       — attenuator control (A/B 0-15)
  rfsw      — RF switch control (Modbus)
  modbus    — raw Modbus frame injection
  can       — CAN bus control (send/scan)
  detector  — detector board control (CAN)
==========================
```

---

## 指令表

### 1. `help` — 帮助信息

列出所有已注册的调试命令。

```text
help
```

**示例输出：**
```
[1000] [INFO] === Debug CLI Commands ===
[1000] [INFO]   help      — list commands
[1000] [INFO]   att       — attenuator control (A/B 0-15)
[1000] [INFO]   rfsw      — RF switch control (Modbus)
[1000] [INFO]   modbus    — raw Modbus frame injection
[1000] [INFO]   can       — CAN bus control (send/scan)
[1000] [INFO]   detector  — detector board control (CAN)
[1000] [INFO] ==========================
```

---

### 2. `att` — 衰减器控制

控制两路数字衰减器的衰减值 (0~15, 4位控制)。

#### 2.1 设置衰减器A

```text
att a <value>
```

| 参数      | 说明                                   |
| --------- | -------------------------------------- |
| `a`       | 选择衰减器A                            |
| `<value>` | 衰减值，范围 0~15（十进制或十六进制）     |

**示例：**
```
att a 5      设置衰减器A = 5
att a 0xF    设置衰减器A = 15
att a 0      设置衰减器A = 0（最小衰减）
```

**硬件对应关系（衰减器A）：**

| 控制位 | 引脚  | 说明       |
| ------ | ----- | ---------- |
| bit0   | PC8   | CTRL_A1    |
| bit1   | PC9   | CTRL_A2    |
| bit2   | PA8   | CTRL_A3    |
| bit3   | PA9   | CTRL_A4    |

#### 2.2 设置衰减器B

```text
att b <value>
```

| 参数      | 说明                                   |
| --------- | -------------------------------------- |
| `b`       | 选择衰减器B                            |
| `<value>` | 衰减值，范围 0~15（十进制或十六进制）     |

**示例：**
```
att b 10     设置衰减器B = 10
att b 0xA    设置衰减器B = 10
att b 0      设置衰减器B = 0
```

**硬件对应关系（衰减器B）：**

| 控制位 | 引脚   | 说明       |
| ------ | ------ | ---------- |
| bit0   | PD2    | CTRL_B1    |
| bit1   | PC12   | CTRL_B2    |
| bit2   | PC11   | CTRL_B3    |
| bit3   | PC10   | CTRL_B4    |

#### 2.3 读取衰减器值

**读取两路：**
```text
att get
att ?
```

**读取单路：**
```text
att a get
att a ?
att b get
att b ?
```

均返回当前衰减器的值。

**示例输出：**
```
[1200] [INFO] Attenuator A = 5  (0x5)
[1200] [INFO] Attenuator B = 10 (0xA)
```

> **注意：** `att a get?` 这类写法会被视为非法数值输入，将报错而非静默设置衰减器为0（已修复此漏洞）。

#### 2.4 查看用法

```text
att
```

不带参数时显示命令帮助信息。

---

### 3. `rfsw` — RF 开关控制 (Modbus RTU)

通过 RS485 (UART8) 控制 SP10T RF 开关，使用 Modbus RTU 协议。

```text
rfsw get <addr>              — Read current channel
rfsw set <addr> <ch>         — Set channel (1-10)
rfsw mode <addr> [io|cmd]    — Get/set work mode
rfsw info <addr>             — Read device identity & status
rfsw output <addr> <id> <0|1>— Set single output coil
rfsw outputs <addr>          — Read all 6 output coils
rfsw inputs <addr>           — Read 4 discrete inputs
rfsw status <addr>           — Read device status register
rfsw id <addr> <new_id>      — Change device Modbus address
```

| 参数      | 说明                                |
| --------- | ----------------------------------- |
| `<addr>`  | 从站 Modbus 地址，范围 1~247         |
| `<ch>`    | 通道号，范围 1~10                    |

#### 3.1 读取通道号 (FC=03)

```text
rfsw get <addr>
```

**示例：**
```
rfsw get 1    读取设备1的当前通道号
```

**示例输出：**
```
[1000] [INFO] RFSW[1]: channel = 1
```

#### 3.2 设置通道号 (FC=06)

```text
rfsw set <addr> <ch>
```

**示例：**
```
rfsw set 1 5    设置设备1通道为5
```

**示例输出：**
```
[1000] [INFO] RFSW[1]: channel set to 5
```

#### 3.3 读取/设置工作模式 (FC=03/06)

```text
rfsw mode <addr>          读取当前模式
rfsw mode <addr> io       设置为 IO DIRECT 模式
rfsw mode <addr> cmd      设置为 COMMAND 模式
```

| 参数      | 说明                                      |
| --------- | ----------------------------------------- |
| `io`      | IO DIRECT 模式（硬件引脚直接控制通道切换）   |
| `cmd`     | COMMAND 模式（通过 Modbus 命令控制）        |

**示例输出：**
```
[1000] [INFO] RFSW[1]: mode = IO DIRECT (0)
```

#### 3.4 读取设备信息 (FC=03 x10)

```text
rfsw info <addr>
```

读取完整的设备身份信息：设备名称、序列号、固件/硬件版本、状态寄存器、输出控制寄存器，以及当前通道和模式。

**示例输出：**
```
[1000] [INFO] -- RFSW[1] Device Info ----------------------
[1000] [INFO]   Name:        'PPA-SP10'
[1000] [INFO]   Serial:      0
[1000] [INFO]   Modbus ID:   1
[1000] [INFO]   FW Version:  v0.0
[1000] [INFO]   HW Version:  16 (0x0010)
[1000] [INFO]   Status:      0x5400 (init_done=0, comm=0)
[1000] [INFO]   Output Ctrl: 0x0000
[1000] [INFO] ----------------------------------------------
[1000] [INFO]   Channel:     1
[1000] [INFO]   Mode:        IO DIRECT (0)
```

#### 3.5 读取设备状态寄存器 (FC=03)

```text
rfsw status <addr>
```

**示例输出：**
```
[1000] [INFO] RFSW[1]: status = 0x5400
[1000] [INFO]   Init done:   NO
[1000] [INFO]   Comm active: NO
```

**状态位定义：**

| 位   | 名称        | 说明               |
| ---- | ----------- | ------------------ |
| bit0 | INIT_DONE   | 设备初始化完成     |
| bit1 | COMM_ACTIVE | Modbus 通信活跃    |

#### 3.6 读取输出线圈状态 (FC=01)

```text
rfsw outputs <addr>
```

读取全部 6 路输出线圈 (S0_CA ~ S2_CB)。

**示例输出：**
```
[1000] [INFO] RFSW[1] output coils:
[1000] [INFO]   [0] S0_CA = OFF
[1000] [INFO]   [1] S0_CB = ON
[1000] [INFO]   [2] S1_CA = ON
[1000] [INFO]   [3] S1_CB = ON
[1000] [INFO]   [4] S2_CA = OFF
[1000] [INFO]   [5] S2_CB = OFF
```

#### 3.7 设置单个输出线圈 (FC=05)

```text
rfsw output <addr> <id> <0|1>
```

| 参数      | 说明                                          |
| --------- | --------------------------------------------- |
| `<id>`    | 线圈编号，0=S0_CA, 1=S0_CB, ..., 5=S2_CB       |
| `<0|1>`   | 0=OFF, 1=ON                                   |

**示例：**
```
rfsw output 1 0 1    设置设备1的 S0_CA = ON
```

#### 3.8 读取离散输入 (FC=02)

```text
rfsw inputs <addr>
```

读取 4 路离散输入引脚 (CTRL1~CTRL4)。

**示例输出：**
```
[1000] [INFO] RFSW[1] discrete inputs:
[1000] [INFO]   CTRL1 = LOW
[1000] [INFO]   CTRL2 = LOW
[1000] [INFO]   CTRL3 = LOW
[1000] [INFO]   CTRL4 = LOW
```

#### 3.9 修改设备 Modbus 地址 (FC=06)

```text
rfsw id <addr> <new_id>
```

| 参数       | 说明                                      |
| ---------- | ----------------------------------------- |
| `<new_id>` | 新 Modbus 地址，范围 1~247                 |

> **警告：** 地址修改立即生效但不一定持久化。需通过 SCPI `SYSTem:CONFigure:SAVE` 命令保存到 Flash。

#### 3.10 查看用法

```text
rfsw
```

不带参数时显示命令帮助信息。

> **参考：** 完整寄存器映射参见 `Doc/RF_Switch_Commands.md`。

---

### 4. `modbus` — Modbus RTU 原生帧注入

通过 RS485 (UART8) 发送原始 Modbus RTU 帧，CRC16 自动计算并追加。

```text
modbus <hex bytes...>
```

| 参数             | 说明                                   |
| ---------------- | -------------------------------------- |
| `<hex bytes...>` | 十六进制字节序列（地址+功能码+数据），字节间用空格分隔 |

帧格式：`[ADDR] [FC] [DATA...]`，CRC16 由固件自动计算和追加。发送后等待从机响应并显示。

**示例：**

```text
modbus 01 03 00 00 00 01      读取从站1的通道号 (FC=03, REG=0x0000)
modbus 01 06 00 00 00 05      设置从站1通道=5 (FC=06)
```

**示例输出（成功）：**
```
[1000] [INFO] Modbus: sending raw frame (8 bytes)
[1000] [INFO] Modbus TX (8 bytes): 01 03 00 00 00 01 84 0A
[1050] [INFO] Modbus RX (7 bytes): 01 03 02 00 01 39 85
```

**示例输出（异常）：**
```
[2500] [INFO] Modbus: EXCEPTION (code=0x02: ILLEGAL DATA ADDRESS)
```

**示例输出（超时）：**
```
[5000] [ERR]  RFSW: Modbus FAILED - Timeout waiting for response
```

> **注意：** CRC16 由固件自动计算追加，无需在命令中手动提供。如需高层 Modbus 操作（读/写寄存器、控制线圈等），请使用 `rfsw` 命令。

#### 4.1 查看用法

```text
modbus
```

不带参数时显示命令帮助信息。

---

### 5. `can` — CAN 总线控制

通过 FDCAN1 (PD0-RX, PD1-TX) 发送和接收 CAN 帧。支持 CAN FD 模式 (with BRS)，11 位标准 ID。

```text
can send <id> <hex data...>    — send raw CAN frame
can scan [start] [end]         — scan bus for active nodes
```

#### 5.1 发送 CAN 帧

```text
can send <id> <hex data...>
```

| 参数             | 说明                                      |
| ---------------- | ----------------------------------------- |
| `<id>`           | CAN ID，范围 0x000~0x7FF（十进制或十六进制） |
| `<hex data...>`  | 数据字节，空格分隔，最大 64 字节 (CAN FD)     |

发送后自动等待响应（超时 500ms），并打印所有收到的响应帧。

**示例：**
```text
can send 0x101 01            发送 ID=0x101, DLC=1, Data=01
can send 0x123 01 02 03 04   发送 ID=0x123, DLC=4
```

**示例输出：**
```
[1000] [INFO] CAN Send: ID=0x101, DLC=1
[1000] [INFO] CAN TX (1 bytes): 01
[1000] [INFO] CAN Send: frame sent, listening for response (500 ms)...
[1050] [INFO] CAN Response #1: ID=0x001, DLC=20
[1050] [INFO] CAN RX (20 bytes): 01 00 12 50 49 4E 50 52 ...
[1050] [INFO] CAN Send: received 1 response(s)
```

#### 5.2 扫描 CAN 总线

```text
can scan [start_id] [end_id]
```

| 参数         | 说明                                |
| ------------ | ----------------------------------- |
| `[start_id]` | 起始节点 ID，默认 1，范围 1~255       |
| `[end_id]`   | 结束节点 ID，默认 40，范围 1~255      |

扫描指定范围的 CAN 节点，发送 0x101 SN 查询并收集 500ms 内的响应。

**示例：**
```text
can scan          扫描节点 1-40 (默认范围)
can scan 1 10     扫描节点 1-10
```

**示例输出：**
```
[1000] [INFO] CAN Scan: scanning nodes 1-40 (40 nodes) ...
[1000] [INFO] CAN Scan: sent 40 queries in 45 ms
[1000] [INFO] CAN Scan: listening for responses (500 ms)...
[1200] [INFO]   Node 1: FOUND, SN='PINPRFB00300000038' (len=18)
[1200] [INFO] CAN Scan: scan complete, 1 node(s) found
```

> **注意：** `can scan` 会向范围内所有节点发送查询，响应收集窗口为 500ms。如需查询单个节点的 SN，可使用 `detector sn <node_id>`。

#### 5.3 查看用法

```text
can
```

不带参数时显示子命令列表。

---

### 6. `detector` — 检波板控制 (CAN A1 协议)

通过 CAN 总线控制 A1 检波板，支持 19 个子命令。协议参考 `Doc/V3.x A1检波板指令.docx`。

```text
detector help                              — Show this help
detector sn <node_id>                      — Read serial number (0x101)
detector version <node_id>                 — Read firmware version (0x103)
detector reset <node_id>                   — Reset MCU (0x104)
detector led <node_id> <0|1> [period]      — LED control (0x105)
detector nodeid <node_id> <new_id|reset>   — Write/Reset Node ID (0x102)
detector control <node> <mode> <hold> <thr> [gate] — Detector command (0x110)
detector stop <node_id>                    — Stop detection (0x112)
detector switch <node> <s1>..<s6>          — Switch control (0x111)
detector att <node> <a1>..<a7>             — Attenuator set (0x113)
detector freq <node> <ch> <khz> [pwr]      — VCO frequency (0x114)
detector band <node> <mhz> <mode> <mask>   — Band select (0x115)
detector temp <node> [det] [mcu]           — Temperature (0x116)
detector power <node> <khz> <hold> <mode> <thr> <gate> — Power query (0x11B)
detector txpower <node> <ch> <khz> <dbm100> [gps] [comp] — Tx power (0x11C)
detector flash <node>                      — Flash info (0x117)
detector readflash <node> <addr> <len>     — Read flash (0x11A)
detector cal <node> enter|exit             — Calibration mode (0x120/0x121)
detector ota prepare <node> <maj> <min> <pat> — OTA prepare (0x1A0)
detector writesn <node> <sn_string>        — Write SN (0x106)
```

| 参数        | 说明                                |
| ----------- | ----------------------------------- |
| `<node_id>` | 目标节点 ID，范围 1~255（0xFF=广播） |
| `<node>`    | 同 node_id，某些子命令的别名         |

#### 6.1 查询序列号 (0x101)

```text
detector sn <node_id>
```

**示例：**
```
detector sn 1
```

**示例输出（成功）：**
```
[1000] [INFO] Node 1 SN: 'PINPRFB00300000038' (len=18)
```

**协议：**
| 项目           | 内容                                      |
| -------------- | ----------------------------------------- |
| 请求帧 ID      | `0x101`                                    |
| 请求帧数据     | Byte0 = Node ID                            |
| 响应帧 ID      | Node ID（目标节点回显自身 ID）               |
| 响应帧 Byte0   | `0x01`（命令字回显）                        |
| 响应帧 Byte1   | `0x00` 成功，`0x01` 失败                    |
| 响应帧 Byte2   | SN 字符串长度                               |
| 响应帧 Byte3+  | SN 字符串内容（可打印 ASCII）               |

#### 6.2 查询固件版本 (0x103)

```text
detector version <node_id>
```

**示例：**
```
detector version 1
```

**示例输出：**
```
[1000] [INFO] Node 1 version: 3.1.21
```

#### 6.3 复位 MCU (0x104)

```text
detector reset <node_id>
```

**示例：**
```
detector reset 1
```

#### 6.4 LED 控制 (0x105)

```text
detector led <node_id> <0|1> [period_ms]
```

| 参数          | 说明                              |
| ------------- | --------------------------------- |
| `<0|1>`       | 0=关闭, 1=开启                     |
| `[period_ms]` | 闪烁周期 (ms)，默认 1000            |

**示例：**
```
detector led 1 1 500     节点1 LED 以 500ms 周期闪烁
detector led 1 0         节点1 LED 关闭
```

#### 6.5 读取/修改节点 ID (0x102)

```text
detector nodeid <node_id> <new_id>    修改节点 ID
detector nodeid <node_id> reset       复位为默认 ID
```

> **警告：** 修改 Node ID 后设备将以新 ID 响应。

#### 6.6 检波控制命令 (0x110)

```text
detector control <node> <mode> <hold> <thr> [gate]
```

| 参数     | 说明                        |
| -------- | --------------------------- |
| `<mode>` | 检波模式                    |
| `<hold>` | 保持时间                    |
| `<thr>`  | 门限值                      |
| `[gate]` | 门控设置（可选）             |

#### 6.7 停止检波 (0x112)

```text
detector stop <node_id>
```

#### 6.8 开关控制 (0x111)

```text
detector switch <node> <s1> <s2> <s3> <s4> <s5> <s6>
```

6 个开关状态参数，每个为 0 或 1。

#### 6.9 衰减器设置 (0x113)

```text
detector att <node> <a1> <a2> <a3> <a4> <a5> <a6> <a7>
```

7 个衰减器参数。

#### 6.10 VCO 频率设置 (0x114)

```text
detector freq <node> <ch> <khz> [lmx_pwr]
```

| 参数        | 说明                    |
| ----------- | ----------------------- |
| `<ch>`      | 通道号                  |
| `<khz>`     | 频率 (kHz)              |
| `[lmx_pwr]` | LMX 功率设置（可选）     |

#### 6.11 频段选择 (0x115)

```text
detector band <node> <mhz> <mode> <mask_hex>
```

| 参数         | 说明                  |
| ------------ | --------------------- |
| `<mhz>`      | 频段中心频率 (MHz)    |
| `<mode>`     | 模式                  |
| `<mask_hex>` | 掩码（十六进制）       |

#### 6.12 温度读取 (0x116)

```text
detector temp <node_id> [det] [mcu]
```

**示例：**
```
detector temp 1 1 1    读取检波器和MCU温度
```

**示例输出：**
```
[1000] [INFO] Node 1 temp: det=-25592, mcu=-23799, rsv=0
```

#### 6.13 功率查询 (0x11B)

```text
detector power <node> <khz> <hold> <mode> <thr> <gate>
```

#### 6.14 发射功率设置 (0x11C)

```text
detector txpower <node> <ch> <khz> <dbm100> [gps] [comp]
```

| 参数       | 说明                 |
| ---------- | -------------------- |
| `<dbm100>` | 功率值 (dBm × 100)   |
| `[gps]`    | GPS 状态（可选）      |
| `[comp]`   | 补偿值（可选）        |

#### 6.15 Flash 信息查询 (0x117)

```text
detector flash <node_id>
```

**示例输出：**
```
[1000] [INFO] Node 1 Flash Info:
[1000] [INFO]   JEDEC ID: 0x1540EF, Status: 0x00
[1000] [INFO]   OTA image:   0x000000 (36 KB)
[1000] [INFO]   H coarse:    0x009001 (8 KB)
[1000] [INFO]   H fine:      0x00D007 (8 KB)
[1000] [INFO]   V coarse:    0x00F00A (8 KB)
[1000] [INFO]   V fine:      0x00B004 (8 KB)
[1000] [INFO]   Ext cal:     0x00100E (52 KB)
[1000] [INFO]   Reserved:    0x00E015 (8 KB)
```

#### 6.16 读取 Flash (0x11A)

```text
detector readflash <node> <addr> <len>
```

| 参数     | 说明                      |
| -------- | ------------------------- |
| `<addr>` | 起始地址（十六进制）       |
| `<len>`  | 读取长度（字节）           |

#### 6.17 校准模式 (0x120/0x121)

```text
detector cal <node> enter    进入校准模式
detector cal <node> exit     退出校准模式
```

#### 6.18 OTA 准备 (0x1A0)

```text
detector ota prepare <node> <maj> <min> <pat>
```

| 参数    | 说明                    |
| ------- | ----------------------- |
| `<maj>` | 主版本号 (major)        |
| `<min>` | 次版本号 (minor)        |
| `<pat>` | 补丁版本号 (patch)      |

#### 6.19 写入序列号 (0x106)

```text
detector writesn <node_id> <sn_string>
```

> **警告：** 此操作将修改检波板内部 Flash 中的 SN 存储页。

---

## 编程接口

### 注册自定义命令

```c
// 注册命令（不带帮助文本，兼容旧接口）
Log_RegisterDbgCmd("mycmd", _my_cmd_handler);

// 注册命令（带帮助文本描述，推荐使用）
Log_RegisterDbgCmdEx("mycmd", _my_cmd_handler, "My custom command description");

static void _my_cmd_handler(int argc, char **argv)
{
    // argv[0] = "mycmd"
    // argv[1..argc-1] = 后续参数
    Log_Print(LOG_LEVEL_INFO, "mycmd called, argc=%d", argc);
}
```

### 注入命令（程序化调用）

```c
Log_DbgInject("att a 7");  // 等效于在串口输入 att a 7 + 回车
```

### 开关回显

```c
Log_DbgSetEcho(0);  // 关闭串口回显
Log_DbgSetEcho(1);  // 开启串口回显（默认）
```

---

## 系统启动流程

```
main()
 ├─ MX_UART7_Init()            ← HAL 初始化 UART7
 ├─ App_RegisterModule(&g_log_task_module)        ← Log 模块注册（优先）
 ├─ App_RegisterModule(&g_attenuator_task_module) ← Attenuator 模块
 ├─ App_RegisterModule(&g_rfsw_task_module)       ← RFSW 模块
 ├─ App_RegisterModule(&g_detector_task_module)   ← Detector 模块
 ├─ App_Task_Init()
 │   ├─ _log_task_init()
 │   │   ├─ Log_Init()          ← 启动 Log 模块
 │   │   ├─ CAN_Init()          ← 初始化 FDCAN1
 │   │   └─ Log_RegisterDbgCmdEx("can", _dbg_cmd_can, "CAN bus control (send/scan)")
 │   │
 │   ├─ _attenuator_task_init()
 │   │   └─ Log_RegisterDbgCmdEx("att", _dbg_cmd_att, "Attenuator control (A/B 0-15)")
 │   │
 │   ├─ _rfsw_task_init()
 │   │   ├─ RS485_Init()        ← 启动 RS485 (UART8 DMA CIRCULAR + IDLE)
 │   │   ├─ Modbus_Init()       ← 初始化 Modbus RTU Master
 │   │   ├─ Log_RegisterDbgCmdEx("rfsw", _dbg_cmd_rfsw, "RF switch control (Modbus)")
 │   │   └─ Log_RegisterDbgCmdEx("modbus", _dbg_cmd_modbus, "Raw Modbus frame injection")
 │   │
 │   └─ _detector_task_init()
 │       └─ Log_RegisterDbgCmdEx("detector", _dbg_cmd_detector, "Detector board control (CAN)")
 │
 └─ App_Task_Loop()
     ├─ _log_task_process()
     │   └─ Log_DbgProcess()    ← 主循环中处理待解析命令
     ├─ _attenuator_task_process()
     ├─ _rfsw_task_process()
     │   └─ 检查 RS485 接收数据 ← 非 Modbus 事务期间打印 RX 数据
     └─ _detector_task_process()
```
