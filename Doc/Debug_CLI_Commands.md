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
 ├─ App_RegisterModule(&g_log_task_module)
 ├─ App_Task_Init()
 │   └─ _log_task_init()
 │       └─ Log_Init()         ← 启动 Log 模块，注册内置命令
 │           └─ Log_InitEx()
 │               ├─ Log_RegisterDbgCmd("help", ...)
 │               ├─ Log_RegisterDbgCmd("att",  ...)
 │               └─ HAL_UART_Receive_IT()  ← 启动 RX 中断
 └─ App_Task_Loop()
     └─ _log_task_process()
         └─ Log_DbgProcess()   ← 主循环中处理待解析命令
```
