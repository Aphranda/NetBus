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
[1000] [INFO]   help
[1000] [INFO]   att
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

### 3. `modbus` — Modbus RTU 主站控制

通过 RS485 (UART8) 发送 Modbus RTU 查询，支持功能码 FC=03 (读保持寄存器) 和 FC=06 (写单个寄存器)。

#### 3.1 读取保持寄存器 (FC=03)

```text
modbus read <slave> <reg> <qty>
```

| 参数      | 说明                                |
| --------- | ----------------------------------- |
| `<slave>` | 从站地址，范围 1~247                 |
| `<reg>`   | 起始寄存器地址，范围 0~65535          |
| `<qty>`   | 读取寄存器数量，范围 1~125            |

**示例：**

```text
modbus read 1 2 1      读取从站1，寄存器2，读1个
modbus read 2 0 8      读取从站2，寄存器0，读8个
```

**示例输出（成功）：**
```
[1000] [INFO] Modbus: FC=03 read (slave=1, reg=2, qty=1)
[1000] [INFO] Modbus: SUCCESS - 1 register(s):
[1000] [INFO]   reg[2] = 42 (0x002A)
```

**示例输出（异常）：**
```
[2500] [ERR]  Modbus: EXCEPTION (code=0x02: ILLEGAL DATA ADDRESS)
```

**示例输出（超时）：**
```
[5000] [ERR]  Modbus: FAILED - Timeout waiting for response
```

#### 3.2 写单个寄存器 (FC=06)

```text
modbus write <slave> <reg> <value>
```

| 参数      | 说明                                |
| --------- | ----------------------------------- |
| `<slave>` | 从站地址，范围 1~247                 |
| `<reg>`   | 寄存器地址，范围 0~65535             |
| `<value>` | 写入值，范围 0~65535                 |

**示例：**

```text
modbus write 1 0 1234     向从站1的寄存器0写入 1234
modbus write 1 0 0xFF     写入十六进制 0xFF
modbus write 2 10 0       向从站2的寄存器10写入 0
```

**示例输出（成功）：**
```
[3000] [INFO] Modbus: FC=06 write (slave=1, reg=0, value=1234)
[3000] [INFO] Modbus: SUCCESS - wrote 1234 to reg[0]
```

#### 3.3 发送原始数据（透传）

```text
modbus send <hex bytes...>
```

| 参数             | 说明                                   |
| ---------------- | -------------------------------------- |
| `<hex bytes...>` | 十六进制字节序列，字节间用空格分隔         |

以原始字节形式发送到 RS485 总线，**不附加 CRC**，不等待响应。适用于调试和测试自定义 Modbus 帧。

**示例：**

```text
modbus send 01 03 00 02 00 01 24 0A   发送 FC=03 查询（含 CRC）
modbus send 01 06 00 00 04 D2 09 A5   发送 FC=06 写入（含 CRC）
```

> **注意：** `modbus send` 为纯透传模式，需自行计算并附加 CRC 校验值。
> `modbus read` 和 `modbus write` 使用 Modbus 库自动处理 CRC、等待响应并解析数据。

#### 3.4 查看用法

```text
modbus
```

不带参数时显示命令帮助信息。

---

## 编程接口
### 注册自定义命令

```c
// 在 Log_Init() 之后调用
Log_RegisterDbgCmd("mycmd", _my_cmd_handler);

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
 ├─ App_RegisterModule(&g_log_task_module)     ← Log 模块注册（优先）
 ├─ App_RegisterModule(&g_rs485_task_module)   ← RS485 模块注册
 ├─ App_Task_Init()
 │   ├─ _log_task_init()
 │   │   └─ Log_Init()         ← 启动 Log 模块，注册内置命令
 │   │       └─ Log_InitEx()
 │   │           ├─ Log_RegisterDbgCmd("help", ...)
 │   │           ├─ Log_RegisterDbgCmd("att",  ...)
 │   │           └─ HAL_UART_Receive_IT()  ← 启动 RX 中断
 │   │
 │   └─ _rs485_task_init()
 │       └─ RS485_Init()       ← 启动 RS485 (UART8 DMA CIRCULAR + IDLE)
 │           └─ Log_RegisterDbgCmd("modbus", _dbg_cmd_modbus)  ← 注册 modbus 命令
 │
 └─ App_Task_Loop()
     ├─ _log_task_process()
     │   └─ Log_DbgProcess()   ← 主循环中处理待解析命令
     └─ _rs485_task_process()
         └─ 检查 RS485 接收数据 ← 非 Modbus 事务期间打印 RX 数据
```

---

### 4. `can` — CAN 总线控制

通过 FDCAN1 (PD0-RX, PD1-TX) 发送和接收 CAN 帧。支持经典 CAN 模式，11 位标准 ID。

#### 4.1 发送 CAN 帧

```text
can send <id> <hex data...>
```

| 参数             | 说明                                      |
| ---------------- | ----------------------------------------- |
| `<id>`           | CAN ID，范围 0x000~0x7FF（十进制或十六进制） |
| `<hex data...>`  | 数据字节，空格分隔，最多 8 字节              |

**示例：**
```text
can send 0x101 01         发送 ID=0x101, DLC=1, Data=01
can send 0x123 01 02 03 04 发送 ID=0x123, DLC=4
```

#### 4.2 查看 CAN 状态

```text
can status
```

**示例输出：**
```
[1000] [INFO] CAN: TX_ErrCnt=0, RX_ErrCnt=0
[1000] [INFO] CAN: Bus-Off=NO
[1000] [INFO] CAN: RxFIFO0 pending=0
```

#### 4.3 查询检波板 SN

通过 CAN 0x101 命令读取 A1 检波板的序列号（SN）。

```text
can sn <node_id>
```

| 参数        | 说明                                       |
| ----------- | ------------------------------------------ |
| `<node_id>` | 目标节点 ID，范围 0x00~0xFF（0xFF 为广播）    |

**协议说明（参考 V3.x A1 检波板指令）：**

| 项目           | 内容                                      |
| -------------- | ----------------------------------------- |
| 请求帧 ID      | `0x101`                                    |
| 请求帧数据     | Byte0 = Node ID                            |
| 响应帧 ID      | Node ID（目标节点回显自身 ID）               |
| 响应帧 Byte0   | `0x01`（命令字回显）                        |
| 响应帧 Byte1   | `0x00` 成功，`0x01` 失败                    |
| 响应帧 Byte2   | SN 字符串长度（0~61，0 表示未写入或损坏）    |
| 响应帧 Byte3+  | SN 字符串内容（可打印 ASCII，不带 \\0 结尾） |

**示例：**
```text
can sn 1
```

**示例输出（成功）：**
```
[1000] [INFO] CAN: querying SN via 0x101, node ID=0x01...
[1000] [INFO] CAN: sending ID=0x101, DLC=1, Data=01
[1200] [INFO] CAN: SN query success (len=16)
[1200] [INFO] CAN: SN = 'A1PA-2025-000001'
```

**示例输出（SN 为空/未写入）：**
```
[1000] [INFO] CAN: querying SN via 0x101, node ID=0x01...
[1000] [INFO] CAN: sending ID=0x101, DLC=1, Data=01
[1200] [INFO] CAN: SN is empty or not programmed yet
```

**示例输出（超时/无响应）：**
```
[1000] [WARN] CAN: SN query timeout (no response within 500 ms)
```

> **注意：** SN 存储在检波板内部 Flash 的 SN 存储页（0x0801D800 ~ 0x0801DFFF），格式为"长度 + 可打印 ASCII 字符串内容"。
> `can sn` 命令发送查询后会等待最多 500ms 的响应超时。
> 若总线有多节点，响应会以节点 ID 作为响应帧 ID，因此不会相互混淆。
```
