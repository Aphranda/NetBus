# NetBus 控制功能测试报告

**生成时间**: 2026-05-21 09:46:35  
**测试端口**: COM8 @ 115200 bps  
**测试模式**: all (Modbus addr=1, CAN node=1)  
**安全策略**: 仅读取/查询/控制，不修改从机或主机配置  

---

## 测试摘要

| 指标 | 值 |
|------|----|
| 总测试项 | 24 |
| PASS | 24 |
| FAIL | 0 |
| WARN | 0 |
| SKIP | 0 |
| 通过率 | 100.0% |
| 总耗时 | 6.1s |

### 子系统结果

| 子系统 | 命令 | 通过 | 说明 |
|--------|------|------|------|
| CLI 基础 | help, att | 10 | 本地衰减器 & 帮助 |
| RFSW (Modbus) | rfsw | 7 | RS485 远程 RF Switch |
| CAN 总线 | cansn, canscan | 2 | CAN 扫描 & SN 查询 |
| Detector (CAN) | detector | 5 | A1 检波板指令 |

## 测试明细

| # | 测试项 | 状态 | 耗时 | 详情 |
|---|--------|------|------|------|
| 1 | help - 帮助信息 | PASS | 1174ms | 命令列表已返回 |
| 2 | att (无参数) | PASS | 230ms | 衰减器用法说明 |
| 3 | att - 读取两路衰减器 | PASS | 216ms | 读取两路衰减器 OK |
| 4 | att - 读取衰减器A | PASS | 220ms | 读取衰减器A OK |
| 5 | att - 读取衰减器B | PASS | 219ms | 读取衰减器B OK |
| 6 | att - 读取当前值 | PASS | 0ms | 基准值 |
| 7 | att - A<-5 | PASS | 215ms | 设置成功 |
| 8 | att - B<-10 | PASS | 220ms | 设置成功 |
| 9 | att - 验证设置 | PASS | 0ms | A=5, B=10 验证通过 |
| 10 | att - 恢复原始值 | PASS | 0ms | 衰减器已恢复 |
| 11 | rfsw - 帮助 | PASS | 330ms | rfsw 命令帮助 |
| 12 | rfsw get 1 - 读取通道号 | PASS | 221ms | 通道号读取成功 |
| 13 | rfsw info 1 - 设备信息 | PASS | 251ms | 设备信息读取成功 |
| 14 | rfsw status 1 - 设备状态 | PASS | 218ms | 读取完成 |
| 15 | rfsw outputs 1 - 读取输出线圈 | PASS | 226ms | 读取完成 |
| 16 | rfsw inputs 1 - 读取离散输入 | PASS | 218ms | 读取完成 |
| 17 | rfsw mode 1 - 读取工作模式 | PASS | 214ms | 模式读取成功 |
| 18 | canscan - CAN 总线扫描 | PASS | 325ms | 扫描完成 |
| 19 | cansn 1 - 检波板 SN 查询 | PASS | 463ms | SN 查询成功 |
| 20 | detector help - 检波器帮助 | PASS | 219ms | detector 子命令列表 |
| 21 | detector sn 1 - 检波板 SN | PASS | 218ms | SN 查询成功 |
| 22 | detector version 1 - 固件版本 | PASS | 218ms | 版本查询已发送 |
| 23 | detector temp 1 - 温度 | PASS | 219ms | 温度查询已发送 |
| 24 | detector flash 1 - Flash 信息 | PASS | 234ms | Flash 信息查询已发送 |

## 附录: 完整响应数据

<details>
<summary>展开查看所有响应详情</summary>

### help - 帮助信息

- **请求**: `help`
- **响应**:
```
[1514323] [INFO] === Debug CLI Commands ===[1514324] [INFO]   help[1514325] [INFO]   cansn[1514327] [INFO]   cansend[1514328] [INFO]   canscan[1514329] [INFO]   att[1514330] [INFO]   rfsw[1514331] [INFO]   detector[1514332] [INFO] ==========================
```

### att (无参数)

- **请求**: `att`
- **响应**:
```
[1515485] [INFO] Usage:
[1515486] [INFO]   att a <0-15>    Set Attenuator A
[1515487] [INFO]   att b <0-15>    Set Attenuator B
[1515488] [INFO]   att a get       Get Attenuator A
[1515489] [INFO]   att b get       Get Attenuator B
[1515490] [INFO]   att get/?       Get both
```

### att - 读取两路衰减器

- **请求**: `att ?`
- **响应**:
```
[1515697] [INFO] Attenuator A = 0  (0x0)
[1515698] [INFO] Attenuator B = 0  (0x0)
```

### att - 读取衰减器A

- **请求**: `att a ?`
- **响应**:
```
[1515908] [INFO] Attenuator A = 0  (0x0)
```

### att - 读取衰减器B

- **请求**: `att b ?`
- **响应**:
```
[1516125] [INFO] Attenuator B = 0  (0x0)
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
[1516775] [INFO] Attenuator A set to 5
```

### att - B<-10

- **请求**: `att b 10`
- **响应**:
```
[1516988] [INFO] Attenuator B set to 10
```

### att - 验证设置

- **请求**: `att a ? / att b ?`
- **响应**:
```
A: [1517305] [INFO] Attenuator A = 5  (0x5)
B: [1517514] [INFO] Attenuator B = 10  (0xA)
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
[1518159] [INFO] RF Switch Control Commands (SP10T via Modbus RTU):
[1518160] [INFO]   rfsw get <addr>              ��� Read current channel
[1518161] [INFO]   rfsw set <addr> <ch>         ��� Set channel (1-10)
[1518162] [INFO]   rfsw mode <addr> [io|cmd]    ��� Get/set work mode
[1518163] [INFO]   rfsw info <addr>             ��� Read device identity & status
[1518164] [INFO]   rfsw output <addr> <id> <0|1>��� Set single output coil
[1518166] [INFO]   rfsw outputs <addr>          ��� Read all 6 output coils
[1518167] [INFO]   rfsw inputs <addr>           ��� Read 4 discrete inputs
[1518168] [INFO]   rfsw status <addr>           ��� Read device status register
[1518169] [INFO]   rfsw id <addr> <new_id>      ��� Change device Modbus address
[1518170] [INFO]   rfsw raw <addr> <hex...>     ��� Send raw Modbus frame
[1518171] [INFO] Examples:
[1518172] [INFO]   rfsw get 1         ��� read channel from device 1
[1518173] [INFO]   rfsw set 1 5       ��� set device 1 to channel 5
[1518174] [INFO]   rfsw mode 1 cmd    ��� set device 1 to COMMAND mode
[1518175] [INFO]   rfsw info 1        ��� dump all device info
```

### rfsw get 1 - 读取通道号

- **请求**: `rfsw get 1`
- **响应**:
```
[1518419] [INFO] RFSW[1]: channel = 1
```

### rfsw info 1 - 设备信息

- **请求**: `rfsw info 1`
- **响应**:
```
[1518651] [INFO] ������ RFSW[1] Device Info ������������������������������������������������������������������
[1518652] [INFO]   Name:        'PPA-SP10'
[1518653] [INFO]   Serial:      0
[1518654] [INFO]   Modbus ID:   1
[1518655] [INFO]   FW Version:  v0.0
[1518656] [INFO]   HW Version:  16 (0x0010)
[1518657] [INFO]   Status:      0x5400 (init_done=0, comm=0)
[1518658] [INFO]   Output Ctrl: 0x0000
[1518659] [INFO] ������������������������������������������������������������������������������������������������������������������������������������������
[1518672] [INFO]   Channel:     1
[1518684] [INFO]   Mode:        IO DIRECT (0)
```

### rfsw status 1 - 设备状态

- **请求**: `rfsw status 1`
- **响应**:
```
[1518845] [INFO] RFSW[1]: status = 0x5400
[1518846] [INFO]   Init done:  NO
[1518847] [INFO]   Comm active: NO
```

### rfsw outputs 1 - 读取输出线圈

- **请求**: `rfsw outputs 1`
- **响应**:
```
[1519056] [INFO] RFSW[1] output coils:
[1519057] [INFO]   [0] S0_CA = OFF
[1519058] [INFO]   [1] S0_CB = ON
[1519059] [INFO]   [2] S1_CA = ON
[1519060] [INFO]   [3] S1_CB = ON
[1519061] [INFO]   [4] S2_CA = OFF
[1519062] [INFO]   [5] S2_CB = OFF
```

### rfsw inputs 1 - 读取离散输入

- **请求**: `rfsw inputs 1`
- **响应**:
```
[1519269] [INFO] RFSW[1] discrete inputs:
[1519270] [INFO]   CTRL1 = LOW
[1519271] [INFO]   CTRL2 = LOW
[1519272] [INFO]   CTRL3 = LOW
[1519273] [INFO]   CTRL4 = LOW
```

### rfsw mode 1 - 读取工作模式

- **请求**: `rfsw mode 1`
- **响应**:
```
[1519477] [INFO] RFSW[1]: mode = IO DIRECT (0)
```

### canscan - CAN 总线扫描

- **请求**: `canscan`
- **响应**:
```
[1519676] [INFO] CAN Scan: scanning nodes 1-40 (40 nodes) ...
[1519677] [INFO] CAN Scan: sending queries (ID=0x101, DLC=1) for nodes 1-40 ...
[1519678] [INFO] CAN Scan TX (1 bytes): 01
[1519679] [INFO] CAN Scan TX (1 bytes): 02
[1519680] [INFO] CAN Scan TX (1 bytes): 03
[1519681] [INFO] CAN Scan TX (1 bytes): 04
[1519682] [INFO] CAN Scan TX (1 bytes): 05
[1519683] [INFO] CAN Scan TX (1 bytes): 06
[1519684] [INFO] CAN Scan TX (1 bytes): 07
[1519685] [INFO] CAN Scan TX (1 bytes): 08
[1519686] [INFO] CAN Scan TX (1 bytes): 09
[1519688] [INFO] CAN Scan TX (1 bytes): 0A
[1519691] [INFO] CAN Scan TX (1 bytes): 0B
[1519692] [INFO] CAN Scan TX (1 bytes): 0C
[1519693] [INFO] CAN Scan TX (1 bytes): 0D
[1519694] [INFO] CAN Scan TX (1 bytes): 0E
[1519695] [INFO] CAN Scan TX (1 bytes): 0F
[1519696] [INFO] CAN Scan TX (1 bytes): 10
[1519697] [INFO] CAN Scan TX (1 bytes): 11
[1519698] [INFO] CAN Scan TX (1 bytes): 12
[1519699] [INFO] CAN Scan TX (1 bytes): 13
[1519700] [INFO] CAN Scan TX (1 bytes): 14
[1519703] [INFO] CAN Scan TX (1 bytes): 15
[1519704] [INFO] CAN Scan TX (1 bytes): 16
[1519705] [INFO] CAN Scan TX (1 bytes): 17
[1519706] [INFO] CAN Scan TX (1 bytes): 18
[1519707] [INFO] CAN Scan TX (1 bytes): 19
[1519708] [INFO] CAN Scan TX (1 bytes): 1A
[1519709] [INFO] CAN Scan TX (1 bytes): 1B
[1519710] [INFO] CAN Scan TX (1 bytes): 1C
[1519711] [INFO] CAN Scan TX (1 bytes): 1D
[1519712] [INFO] CAN Scan TX (1 bytes): 1E
[1519715] [INFO] CAN Scan TX (1 bytes): 1F
[1519716] [INFO] CAN Scan TX (1 bytes): 20
[1519717] [INFO] CAN Scan TX (1 bytes): 21
[1519718] [INFO] CAN Scan TX (1 bytes): 22
[1519719] [INFO] CAN Scan TX23
[1519720] [INFO] C (1 bytes): 24
[1519721] Scan TX (1 19722] [INFO] CAN Scan TX (1 b
9723] [INFO] CAN Sces): 27
[1CAN Scan TX (1 byte[1519727] [can: sent 40 queries in[151728] [INFO] CAN Scan:  for responses)..[1519729] [INFO]   NOUND, SN='PINPR0000' (len=18)
```

### cansn 1 - 检波板 SN 查询

- **请求**: `cansn 1`
- **响应**:
```
[1520228] [INFO] CAN Scan: scan complete, 1 node(s) found
[1520229] [INFO] CAN eryinrom node 1 ...
[1520230] [INSN: sending ID=0x101, DLC=1
[1520231NX (1 bytes): 01
[15 [INFO] CAN SN: SUCCESS - node 1 SN = 'PINPRFB00300000038' (len=18)
[1520234] [INFOSN = 50 49 4E 50 52 46 42 30 30 33 30 30 30 30 30 30 33 38
```

### detector help - 检波器帮助

- **请求**: `detector help`
- **响应**:
```
[1520304] [INFO] Available sub-commands:
[1520305] [INFO]   sn, version, reset, led, nodeid, control, stop, switch
[1520306] [INFO]   att, freq, band, temp, power, txpower, flash, readflash
[1520307] [INFO]   cal, ota, writesn
[1520308] [INFO]   Use 'detector' without args for full listing
```

### detector sn 1 - 检波板 SN

- **请求**: `detector sn 1`
- **响应**:
```
[1520508] [INFO] Node 1 SN: 'PINPRFB00300000038' (len=18)
```

### detector version 1 - 固件版本

- **请求**: `detector version 1`
- **响应**:
```
[1520723] [INFO] Node 1 version: 3.1.21
```

### detector temp 1 - 温度

- **请求**: `detector temp 1 1 1`
- **响应**:
```
[1520938] [INFO] Node 1 temp: det=-25592, mcu=-23799, rsv=0
```

### detector flash 1 - Flash 信息

- **请求**: `detector flash 1`
- **响应**:
```
[1521154] [INFO] Node 1 Flash Info:
[1521155] [INFO]   JEDEC ID: 0x1540EF, Status: 0x00
[1521156] [INFO]   OTA image:   0x000000 (36 KB)
[1521157] [INFO]   H coarse:    0x009001 (8 KB)
[1521158] [INFO]   H fine:      0x00D007 (8 KB)
[1521159] [INFO]   V coarse:    0x00F00A (8 KB)
[1521160] [INFO]   V fine:      0x00B004 (8 KB)
[1521161] [INFO]   Ext cal:     0x00100E (52 KB)
[1521162] [INFO]   Reserved:    0x00E015 (8 KB)
```

</details>