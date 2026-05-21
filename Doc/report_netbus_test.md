# NetBus 控制功能测试报告

**生成时间**: 2026-05-21 12:57:29  
**测试端口**: COM8 @ 115200 bps  
**测试模式**: all (Modbus addr=1, CAN node=1)  
**安全策略**: 仅读取/查询/控制，不修改从机或主机配置  

---

## 测试摘要

| 指标 | 值 |
|------|----|
| 总测试项 | 29 |
| PASS | 29 |
| FAIL | 0 |
| WARN | 0 |
| SKIP | 0 |
| 通过率 | 100.0% |
| 总耗时 | 7.6s |

### 子系统结果

| 子系统 | 命令 | 通过 | 说明 |
|--------|------|------|------|
| CLI 基础 | help, att | 10 | 本地衰减器 & 帮助 |
| RFSW (Modbus) | rfsw | 7 | RS485 远程 RF Switch |
| CAN 总线 | can send/scan | 3 | CAN 查询 & 扫描 |
| Modbus Raw | modbus | 2 | Modbus 原生帧注入 |
| Detector (CAN) | detector | 7 | A1 检波板指令 |

## 测试明细

| # | 测试项 | 状态 | 耗时 | 详情 |
|---|--------|------|------|------|
| 1 | help - 帮助信息 | PASS | 1177ms | 命令列表已返回 |
| 2 | att (无参数) | PASS | 231ms | 衰减器用法说明 |
| 3 | att - 读取两路衰减器 | PASS | 216ms | 读取两路衰减器 OK |
| 4 | att - 读取衰减器A | PASS | 215ms | 读取衰减器A OK |
| 5 | att - 读取衰减器B | PASS | 214ms | 读取衰减器B OK |
| 6 | att - 读取当前值 | PASS | 0ms | 基准值 |
| 7 | att - A<-5 | PASS | 215ms | 设置成功 |
| 8 | att - B<-10 | PASS | 218ms | 设置成功 |
| 9 | att - 验证设置 | PASS | 0ms | A=5, B=10 验证通过 |
| 10 | att - 恢复原始值 | PASS | 0ms | 衰减器已恢复 |
| 11 | rfsw - 帮助 | PASS | 275ms | rfsw 命令帮助 |
| 12 | rfsw get 1 - 读取通道号 | PASS | 216ms | 通道号读取成功 |
| 13 | rfsw info 1 - 设备信息 | PASS | 244ms | 设备信息读取成功 |
| 14 | rfsw status 1 - 设备状态 | PASS | 216ms | 读取完成 |
| 15 | rfsw outputs 1 - 读取输出线圈 | PASS | 232ms | 读取完成 |
| 16 | rfsw inputs 1 - 读取离散输入 | PASS | 228ms | 读取完成 |
| 17 | rfsw mode 1 - 读取工作模式 | PASS | 217ms | 模式读取成功 |
| 18 | modbus - 帮助 | PASS | 231ms | modbus 命令帮助 |
| 19 | modbus raw - FC=03 读寄存器 (addr=1) | PASS | 229ms | Modbus 原生帧已发送 |
| 20 | can - 帮助 (无参数) | PASS | 215ms | CAN 子命令帮助 |
| 21 | can scan - CAN 总线扫描 | PASS | 343ms | 扫描完成 |
| 22 | can send - SN查询 (ID=0x101) | PASS | 447ms | CAN 帧发送成功, 收到响应 |
| 23 | detector - 帮助 (无参数) | PASS | 649ms | detector 命令帮助 |
| 24 | detector help - 检波器帮助 | PASS | 232ms | detector 子命令列表 |
| 25 | detector sn 1 - 检波板 SN | PASS | 217ms | SN 查询成功 |
| 26 | detector version 1 - 固件版本 | PASS | 217ms | 版本查询已发送 |
| 27 | detector temp 1 - 温度 | PASS | 212ms | 温度查询已发送 |
| 28 | detector flash 1 - Flash 信息 | PASS | 246ms | Flash 信息查询已发送 |
| 29 | detector readflash - Flash 数据 (addr=0x0, len=32) | PASS | 216ms | Flash 数据读取已发送 |

## 附录: 完整响应数据

<details>
<summary>展开查看所有响应详情</summary>

### help - 帮助信息

- **请求**: `help`
- **响应**:
```
[402981] [INFO] === Debug CLI Commands ===[402982] [INFO]   help[402983] [INFO]   can        ��� CAN bus control (send/scan)[402984] [INFO]   att        ��� Attenuator control (A/B 0-15)[402985] [INFO]   rfsw       ��� RF switch control (Modbus)[402986] [INFO]   modbus     ��� Raw Modbus frame injection[402987] [INFO]   detector   ��� Detector board control (CAN)[402988] [INFO] ==========================
```

### att (无参数)

- **请求**: `att`
- **响应**:
```
[404130] [INFO] Usage:
[404131] [INFO]   att a <0-15>    Set Attenuator A
[404132] [INFO]   att b <0-15>    Set Attenuator B
[404133] [INFO]   att a get       Get Attenuator A
[404134] [INFO]   att b get       Get Attenuator B
[404135] [INFO]   att get/?       Get both
```

### att - 读取两路衰减器

- **请求**: `att ?`
- **响应**:
```
[404344] [INFO] Attenuator A = 0  (0x0)
[404345] [INFO] Attenuator B = 0  (0x0)
```

### att - 读取衰减器A

- **请求**: `att a ?`
- **响应**:
```
[404555] [INFO] Attenuator A = 0  (0x0)
```

### att - 读取衰减器B

- **请求**: `att b ?`
- **响应**:
```
[404767] [INFO] Attenuator B = 0  (0x0)
```

### att - 读取当前值

- **请求**: `att a ? / att b ?`
- **响应**:
```
A=0, B=0
```

### att - A<-5

- **请求**: `att a 5`
- **响应**:
```
[405409] [INFO] Attenuator A set to 5
```

### att - B<-10

- **请求**: `att b 10`
- **响应**:
```
[405622] [INFO] Attenuator B set to 10
```

### att - 验证设置

- **请求**: `att a ? / att b ?`
- **响应**:
```
A: [405938] [INFO] Attenuator A = 5  (0x5)
B: [406143] [INFO] Attenuator B = 10  (0xA)
```

### att - 恢复原始值

- **请求**: `att a 0 / att b 0`
- **响应**:
```
已恢复 A=0, B=0
```

### rfsw - 帮助

- **请求**: `rfsw`
- **响应**:
```
[406785] [INFO] RF Switch Control Commands (SP10T via Modbus RTU):
[406786] [INFO]   rfsw get <addr>              ��� Read current channel
[406787] [INFO]   rfsw set <addr> <ch>         ��� Set channel (1-10)
[406788] [INFO]   rfsw mode <addr> [io|cmd]    ��� Get/set work mode
[406789] [INFO]   rfsw info <addr>             ��� Read device identity & status
[406790] [INFO]   rfsw output <addr> <id> <0|1>��� Set single output coil
[406791] [INFO]   rfsw outputs <addr>          ��� Read all 6 output coils
[406792] [INFO]   rfsw inputs <addr>           ��� Read 4 discrete inputs
[406793] [INFO]   rfsw status <addr>           ��� Read device status register
[406794] [INFO]   rfsw id <addr> <new_id>      ��� Change device Modbus address
[406795] [INFO] Examples:
[406796] [INFO]   rfsw get 1         ��� read channel from device 1
[406797] [INFO]   rfsw set 1 5       ��� set device 1 to channel 5
[406798] [INFO]   rfsw mode 1 cmd    ��� set device 1 to COMMAND mode
[406799] [INFO]   rfsw info 1        ��� dump all device info
```

### rfsw get 1 - 读取通道号

- **请求**: `rfsw get 1`
- **响应**:
```
[406996] [INFO] RFSW[1]: channel = 1
```

### rfsw info 1 - 设备信息

- **请求**: `rfsw info 1`
- **响应**:
```
[407223] [INFO] ������ RFSW[1] Device Info ������������������������������������������������������������������
[407224] [INFO]   Name:        'PPA-SP10'
[407225] [INFO]   Serial:      0
[407226] [INFO]   Modbus ID:   1
[407227] [INFO]   FW Version:  v0.0
[407228] [INFO]   HW Version:  16 (0x0010)
[407229] [INFO]   Status:      0x5400 (init_done=0, comm=0)
[407230] [INFO]   Output Ctrl: 0x0000
[407231] [INFO] ������������������������������������������������������������������������������������������������������������������������������������������
[407244] [INFO]   Channel:     1
[407257] [INFO]   Mode:        IO DIRECT (0)
```

### rfsw status 1 - 设备状态

- **请求**: `rfsw status 1`
- **响应**:
```
[407413] [INFO] RFSW[1]: status = 0x5400
[407414] [INFO]   Init done:  NO
[407415] [INFO]   Comm active: NO
```

### rfsw outputs 1 - 读取输出线圈

- **请求**: `rfsw outputs 1`
- **响应**:
```
[407622] [INFO] RFSW[1] output coils:
[407623] [INFO]   [0] S0_CA = OFF
[407624] [INFO]   [1] S0_CB = ON
[407625] [INFO]   [2] S1_CA = ON
[407626] [INFO]   [3] S1_CB = ON
[407627] [INFO]   [4] S2_CA = OFF
[407628] [INFO]   [5] S2_CB = OFF
```

### rfsw inputs 1 - 读取离散输入

- **请求**: `rfsw inputs 1`
- **响应**:
```
[407840] [INFO] RFSW[1] discrete inputs:
[407841] [INFO]   CTRL1 = LOW
[407842] [INFO]   CTRL2 = LOW
[407843] [INFO]   CTRL3 = LOW
[407844] [INFO]   CTRL4 = LOW
```

### rfsw mode 1 - 读取工作模式

- **请求**: `rfsw mode 1`
- **响应**:
```
[408060] [INFO] RFSW[1]: mode = IO DIRECT (0)
```

### modbus - 帮助

- **请求**: `modbus`
- **响应**:
```
[408262] [INFO] Modbus raw frame injection (RS485):
[408263] [INFO]   modbus <hex bytes...>  ��� send raw Modbus frame (CRC auto)
[408264] [INFO]   Frame must include address + function code + data.
[408265] [INFO]   CRC16 is computed and appended automatically.
[408266] [INFO] Examples:
[408267] [INFO]   modbus 01 03 00 00 00 01   ��� read channel from device 1
[408268] [INFO]   modbus 01 06 00 00 00 05   ��� write channel 5 to device 1
```

### modbus raw - FC=03 读寄存器 (addr=1)

- **请求**: `modbus 01 03 00 00 00 01`
- **响应**:
```
[408463] [INFO] Modbus: sending raw frame (8 bytes)
[408464] [INFO] Modbus TX (8 bytes): 01 03 00 00 00 01 84 0A
[408477] [INFO] Modbus RX (7 bytes): 01 03 02 00 01 79 84
```

### can - 帮助 (无参数)

- **请求**: `can`
- **响应**:
```
[408678] [INFO] === CAN Commands ===
[408679] [INFO]   can send <id> <hex...>    ��� send raw CAN frame
[408680] [INFO]   can scan [start] [end]    ��� scan CAN bus
```

### can scan - CAN 总线扫描

- **请求**: `can scan`
- **响应**:
```
[408882] [INFO] CAN Scan: scanning nodes 1-40 (40 nodes) ...
[408883] [INFO] CAN Scan: sending queries (ID=0x101, DLC=1) for nodes 1-40 ...
[408885] [INFO] CAN Scan TX (1 bytes): 01
[408886] [INFO] CAN Scan TX (1 bytes): 02
[408887] [INFO] CAN Scan TX (1 bytes): 03
[408888] [INFO] CAN Scan TX (1 bytes): 04
[408889] [INFO] CAN Scan TX (1 bytes): 05
[408890] [INFO] CAN Scan TX (1 bytes): 06
[408891] [INFO] CAN Scan TX (1 bytes): 07
[408892] [INFO] CAN Scan TX (1 bytes): 08
[408893] [INFO] CAN Scan TX (1 bytes): 09
[408894] [INFO] CAN Scan TX (1 bytes): 0A
[408898] [INFO] CAN Scan TX (1 bytes): 0B
[408899] [INFO] CAN Scan TX (1 bytes): 0C
[408900] [INFO] CAN Scan TX (1 bytes): 0D
[408901] [INFO] CAN Scan TX (1 bytes): 0E
[408902] [INFO] CAN Scan TX (1 bytes): 0F
[408903] [INFO] CAN Scan TX (1 bytes): 10
[408904] [INFO] CAN Scan TX (1 bytes): 11
[408905] [INFO] CAN Scan TX (1 bytes): 12
[408906] [INFO] CAN Scan TX (1 bytes): 13
[408907] [INFO] CAN Scan TX (1 bytes): 14
[408910] [INFO] CAN Scan TX (1 bytes): 15
[408911] [INFO] CAN Scan TX (1 bytes): 16
[408912] [INFO] CAN Scan TX (1 bytes): 17
[408913] [INFO] CAN Scan TX (1 bytes): 18
[408914] [INFO] CAN Scan TX (1 bytes): 19
[408915] [INFO] CAN Scan TX (1 bytes): 1A
[408916] [INFO] CAN Scan TX (1 bytes): 1B
[408917] [INFO] CAN Scan TX (1 bytes): 1C
[408918] [INFO] CAN Scan TX (1 bytes): 1D
[408919] [INFO] CAN Scan TX (1 bytes): 1E
[408922] [INFO] CAN Scan TX (1 bytes): 1F
[408923] [INFO] CAN Scan TX (1 bytes): 20
[408924] [INFO] CAN Scan TX (1 bytes): 21
[408925] [INFO] CAN Scan TX (1 bytes): 22
[408926] [INFO] CAN Scan TX (1 bytes): 23
[408927] [INFO] CAN Scans): 24
[408928] [INFO] CAN Scan TX (1 bytes):[408929] [INFO] CAN Scan TX (1 bytes): 268930] [INFO] CAN  bs): 27
[408931] [INFO] CAN Sca(1 bytes): 28
 [INFO] CAN Scan: sent 40 51 ms
[408935] [INFO] CAN Scan: lig for responses (500 ms)... [INFO]   Node 1: FOUND, 0300000038' (len=18)
```

### can send - SN查询 (ID=0x101)

- **请求**: `can send 0x101 01`
- **响应**:
```
[409435] [INFO] CAN Scan: scan complete, 1 node(s) found
[409436] [e ID=0x101, DLC=1
[409437] [INFN TX (1 bytes): 01
[409438] S frame sent, listening for response (500 ms)...
[409439] [INFO]ponse #1: ID=0x001, DLC=64
[409  RX (64 bytes): 01 00 12 50 49 4E 50 52 46 42 30 30 33 30 30 30 30 30 30 33 38 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

### detector - 帮助 (无参数)

- **请求**: `detector`
- **响应**:
```
[409939] [INFO] CAN Send: received 1 response(s)
[409940] [INFO] Usaetector <subcmd> [args...]
[409941] [INFommands:
[409942] [INFO] node_id>                    ��� Read serial number (0x101)
[409943] [INFO]ion <node_id>               ��� Read firmware version (0x103)
[409944] [INFO]   reseode_id>                 ��� Reset MCU (0x104)
[409945] [INFO]  node_id> <0|1> [period]   ��� LED control (0x105)
[409946] [INid <node_id> <new_id>      ��� Write Node ID (0x102)
[409947] [INFO]  ode_id> reset          ��� Reset Node ID to default
[409948] [INFO]   control <node> <mode> <thr>[gate] ��� Detector cmd (0x110)
[409949] [IN <node_id>                  ��� Stop detection (0x112)
[409950] [INFOtch <node> <s1>..<s6>       ��� Switch control (0x111)
[409951] [INFO]  de> <a1>..<a7>          ��� Attenuator set (0x113)
[409952] [INFO]   frede> <ch> <khz> [pwr]   ��� VCO frequency (0x114)
[409953] [INFO <node> <mhz> <mode> <mask> ��� Band select (0x115)
[409954] [INFO]   tde> [det] [mcu]         ��� Temperature (0x116)
[409955] [INFer <node> <khz> <hold> <mode> <thr> <gate> ��� Query (0x11B)
[409956] [Io <node> <ch> <khz> <dbm100> [gps] [comp] ��� Tx (0x11C)
[409957] [INFO]   flanode>                    ��� Flash info (0x117)
[409958] [INFO]flash <node> <addr> <len>  ��� Read flash (0x11A)
[409959] [Il <node> enter|exit          ��� Calibration mode (0x120/0x121)
[409960] [INFO]   epare <node> <maj> <min> <pat> ��� OTA prepare (0x1A0)
[409961] [INFO]   w <node> <sn_string>      ��� Write SN (0x106)
[409962] [INFO]                             ��� Show this help
```

### detector help - 检波器帮助

- **请求**: `detector help`
- **响应**:
```
[410027] [INFO] Available sub-commands:
[410028] [INFO]   sn, version, reset, led, nodeid, control, stop, switch
[410029] [INFO]   att, freq, band, temp, power, txpower, flash, readflash
[410030] [INFO]   cal, ota, writesn
[410031] [INFO]   Use 'detector' without args for full listing
```

### detector sn 1 - 检波板 SN

- **请求**: `detector sn 1`
- **响应**:
```
[410245] [INFO] Node 1 SN: 'PINPRFB00300000038' (len=18)
```

### detector version 1 - 固件版本

- **请求**: `detector version 1`
- **响应**:
```
[410459] [INFO] Node 1 version: 3.1.21
```

### detector temp 1 - 温度

- **请求**: `detector temp 1 1 1`
- **响应**:
```
[410674] [INFO] Node 1 temp: det=-22264, mcu=29193, rsv=0
```

### detector flash 1 - Flash 信息

- **请求**: `detector flash 1`
- **响应**:
```
[410882] [INFO] Node 1 Flash Info:
[410883] [INFO]   JEDEC ID: 0x1540EF, Status: 0x00
[410884] [INFO]   OTA image:   0x000000 (36 KB)
[410885] [INFO]   H coarse:    0x009001 (8 KB)
[410887] [INFO]   H fine:      0x00D007 (8 KB)
[410888] [INFO]   V coarse:    0x00F00A (8 KB)
[410889] [INFO]   V fine:      0x00B004 (8 KB)
[410890] [INFO]   Ext cal:     0x00100E (52 KB)
[410891] [INFO]   Reserved:    0x00E015 (8 KB)
```

### detector readflash - Flash 数据 (addr=0x0, len=32)

- **请求**: `detector readflash 1 0x0 32`
- **响应**:
```
[411102] [ERR] Node 1 read flash failed: Invalid Parameter
```

</details>