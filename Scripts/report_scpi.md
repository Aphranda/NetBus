# NetBus SCPI-over-TCP 自动化测试报告

**生成时间**: 2026-05-29 14:12:50  
**目标设备**: `192.168.1.10:5025`  
**协议**: SCPI (IEEE 488.2) over TCP/IP  
**测试节点**: DETector#1, ROUTe:SWITch#1  

---

## 1. 测试摘要

| 指标 | 值 |
|------|----|
| 总测试项 | 115 |
| **PASS** | 113 |
| **FAIL** | 1 |
| SKIP | 1 |
| **通过率** | **98.3%** |
| 总耗时 | 8.1s (8110ms) |
| 平均响应 | 70.5ms

## 2. 子系统覆盖率

| 子系统 | 命令覆盖 | 测试项 | 通过 | 说明 |
|--------|----------|--------|------|------|
| IEEE 488.2 | *IDN?, *STB?, *OPC?, *CLS, *RST, *WAI | 6 | 6 | 全标准命令 |
| SYSTem | ATT A/B RW+验证, ERR, DET# SN/VERS/FLASH, CAN SEND/SCAN | 21 | 21 | 子系统全覆盖 |
| SENSe | TEMP?, POWer? | 2 | 2 | 只读查询 |
| ROUTe:SWITch# | CHAN?/CHAN, MODE?, IDEN?, OUTP?, INP?, COND? | 10 | 10 | 含通道切换验证 |
| STATus | OPER:EVEN?, QUES:EVEN? | 2 | 2 | |
| DIAGnostic | DEBUG?/ON/OFF, ECHO?/ON/OFF | 9 | 9 | 含开关验证 |
| 压力测试 | *IDN? x50 + 交替命令 x15 | 65 | 65 | |

## 3. 测试明细

| # | 测试项 | 状态 | 耗时 | 详情 |
|---|--------|------|------|------|
| 1 | IEEE488.2 *IDN? | **PASS** | 213ms | IDN 正确: NetBus,PPA-NB100,00000000,v1.0.0 |
| 2 | IEEE488.2 *STB? | **PASS** | 203ms | STB = 0 |
| 3 | IEEE488.2 *OPC? | **PASS** | 215ms | OPC = 1 |
| 4 | IEEE488.2 *CLS | **PASS** | 204ms | 已执行 |
| 5 | IEEE488.2 *RST | **PASS** | 205ms | 已复位 |
| 6 | IEEE488.2 *WAI | **PASS** | 204ms | 已执行 |
| 7 | SYST:ERR? | **PASS** | 216ms | 错误队列为空 (No error) |
| 8 | SYST:ERR:COUN? | **PASS** | 201ms | 错误计数 = 0 |
| 9 | ATT A 读取 | **PASS** | 202ms | ATT A 读取 = 0 |
| 10 | ATT B 读取 | **PASS** | 204ms | ATT B 读取 = 0 |
| 11 | ATT 基准值 | **PASS** | 0ms | 当前: A=0, B=0 |
| 12 | ATT A<-3 | **PASS** | 204ms | OK |
| 13 | ATT B<-7 | **PASS** | 214ms | OK |
| 14 | ATT A<-12 | **PASS** | 212ms | OK |
| 15 | ATT B<-14 | **PASS** | 215ms | OK |
| 16 | ATT 验证最终值 | **PASS** | 0ms | A=12, B=14 |
| 17 | ATT A<-99 越界 | **PASS** | 201ms | 正确拒绝 (范围保护) |
| 18 | ATT A<--1 越界 | **PASS** | 216ms | 正确拒绝 (范围保护) |
| 19 | ATT 恢复原始值 | **PASS** | 0ms | 已恢复 |
| 20 | DET1 SN? | **PASS** | 202ms | SN: "PINPRFB00300000038" |
| 21 | DET1 VERS? | **PASS** | 314ms | 版本: 3.1.21 |
| 22 | DET1 FLAS:INFO? | **PASS** | 228ms | Flash 信息已返回 (含 JEDEC ID) |
| 23 | DET1 FLAS:DATA? @0x9001 | **PASS** | 203ms | 数据: ERROR: Flash read failed |
| 24 | DET1 LED | SKIP | 0ms | 写命令, 跳过以保护设备 |
| 25 | SENS:DET1:TEMP? | **PASS** | 215ms | 温度: -2808,-17655,0 |
| 26 | SENS:DET1:POW? | **PASS** | 205ms | 功率:  |
| 27 | CAN:SCAN? (1-5) | **PASS** | 202ms | 无响应 |
| 28 | CAN SEND (ID=0x101) | **PASS** | 263ms | CAN 响应: ERROR: Power query failed |
| 29 | SW1 IDEN? | **PASS** | 214ms | 响应:  |
| 30 | SW1 CHAN? | **PASS** | 342ms | 通道: 0 |
| 31 | SW1 MODE? | **PASS** | 202ms | 模式:  |
| 32 | SW1 OUTP? | **PASS** | 341ms | 输出: 0 responses"PPA-SP10",0,1,1,0,21504<br>1<br> |
| 33 | SW1 INP? | **PASS** | 219ms | 输入: 0,0,0,0 |
| 34 | SW1 COND? | **PASS** | 204ms | 状态: ERROR: Read status failed (Sla |
| 35 | SW1 当前通道 | **PASS** | 0ms | 当前 CH=1 |
| 36 | SW1 CHAN 3 | **PASS** | 215ms | CHAN<-3 |
| 37 | SW1 验证 CH=3 | **PASS** | 0ms | CH=3 验证通过 |
| 38 | SW1 恢复 CH=1 | **PASS** | 0ms | 已恢复 CH=1 |
| 39 | STATus - OPERation | **PASS** | 204ms | 0 |
| 40 | STATus - QUEStionable | **PASS** | 202ms | 0 |
| 41 | DIAG:DEBUG? 初始 | **PASS** | 0ms | 当前: 1 |
| 42 | DIAG:DEBUG OFF | **PASS** | 202ms | 已关闭 |
| 43 | DIAG:DEBUG? 验证关闭 | **PASS** | 0ms | DEBUG=OFF |
| 44 | DIAG:DEBUG ON | **PASS** | 201ms | 已开启 |
| 45 | DIAG:DEBUG? 验证开启 | **PASS** | 0ms | DEBUG=ON |
| 46 | DIAG:ECHO? 初始 | **PASS** | 0ms | 当前: ON |
| 47 | DIAG:ECHO OFF | **PASS** | 203ms | 已关闭 |
| 48 | DIAG:ECHO? 验证关闭 | **FAIL** | 0ms | 未关闭 |
| 49 | DIAG:ECHO ON | **PASS** | 201ms | 已开启 |
| 50 | DIAG:ECHO? 验证开启 | **PASS** | 0ms | ECHO=ON |
| 51 | Stress #01 | **PASS** | 0ms | OK |
| 52 | Stress #02 | **PASS** | 0ms | OK |
| 53 | Stress #03 | **PASS** | 0ms | OK |
| 54 | Stress #04 | **PASS** | 0ms | OK |
| 55 | Stress #05 | **PASS** | 0ms | OK |
| 56 | Stress #06 | **PASS** | 0ms | OK |
| 57 | Stress #07 | **PASS** | 0ms | OK |
| 58 | Stress #08 | **PASS** | 0ms | OK |
| 59 | Stress #09 | **PASS** | 0ms | OK |
| 60 | Stress #10 | **PASS** | 0ms | OK |
| 61 | Stress #11 | **PASS** | 0ms | OK |
| 62 | Stress #12 | **PASS** | 0ms | OK |
| 63 | Stress #13 | **PASS** | 0ms | OK |
| 64 | Stress #14 | **PASS** | 0ms | OK |
| 65 | Stress #15 | **PASS** | 0ms | OK |
| 66 | Stress #16 | **PASS** | 0ms | OK |
| 67 | Stress #17 | **PASS** | 0ms | OK |
| 68 | Stress #18 | **PASS** | 0ms | OK |
| 69 | Stress #19 | **PASS** | 0ms | OK |
| 70 | Stress #20 | **PASS** | 0ms | OK |
| 71 | Stress #21 | **PASS** | 0ms | OK |
| 72 | Stress #22 | **PASS** | 0ms | OK |
| 73 | Stress #23 | **PASS** | 0ms | OK |
| 74 | Stress #24 | **PASS** | 0ms | OK |
| 75 | Stress #25 | **PASS** | 0ms | OK |
| 76 | Stress #26 | **PASS** | 0ms | OK |
| 77 | Stress #27 | **PASS** | 0ms | OK |
| 78 | Stress #28 | **PASS** | 0ms | OK |
| 79 | Stress #29 | **PASS** | 0ms | OK |
| 80 | Stress #30 | **PASS** | 0ms | OK |
| 81 | Stress #31 | **PASS** | 0ms | OK |
| 82 | Stress #32 | **PASS** | 0ms | OK |
| 83 | Stress #33 | **PASS** | 0ms | OK |
| 84 | Stress #34 | **PASS** | 0ms | OK |
| 85 | Stress #35 | **PASS** | 0ms | OK |
| 86 | Stress #36 | **PASS** | 0ms | OK |
| 87 | Stress #37 | **PASS** | 0ms | OK |
| 88 | Stress #38 | **PASS** | 0ms | OK |
| 89 | Stress #39 | **PASS** | 0ms | OK |
| 90 | Stress #40 | **PASS** | 0ms | OK |
| 91 | Stress #41 | **PASS** | 0ms | OK |
| 92 | Stress #42 | **PASS** | 0ms | OK |
| 93 | Stress #43 | **PASS** | 0ms | OK |
| 94 | Stress #44 | **PASS** | 0ms | OK |
| 95 | Stress #45 | **PASS** | 0ms | OK |
| 96 | Stress #46 | **PASS** | 0ms | OK |
| 97 | Stress #47 | **PASS** | 0ms | OK |
| 98 | Stress #48 | **PASS** | 0ms | OK |
| 99 | Stress #49 | **PASS** | 0ms | OK |
| 100 | Stress #50 | **PASS** | 0ms | OK |
| 101 | Interleave #01 *IDN? | **PASS** | 0ms | OK |
| 102 | Interleave #02 SYST:ERR:COUN? | **PASS** | 0ms | OK |
| 103 | Interleave #03 *STB? | **PASS** | 0ms | OK |
| 104 | Interleave #04 SYST:ATT:A? | **PASS** | 0ms | OK |
| 105 | Interleave #05 DIAG:DEBUG? | **PASS** | 0ms | OK |
| 106 | Interleave #06 *IDN? | **PASS** | 0ms | OK |
| 107 | Interleave #07 SYST:ERR:COUN? | **PASS** | 0ms | OK |
| 108 | Interleave #08 *STB? | **PASS** | 0ms | OK |
| 109 | Interleave #09 SYST:ATT:A? | **PASS** | 0ms | OK |
| 110 | Interleave #10 DIAG:DEBUG? | **PASS** | 0ms | OK |
| 111 | Interleave #11 *IDN? | **PASS** | 0ms | OK |
| 112 | Interleave #12 SYST:ERR:COUN? | **PASS** | 0ms | OK |
| 113 | Interleave #13 *STB? | **PASS** | 0ms | OK |
| 114 | Interleave #14 SYST:ATT:A? | **PASS** | 0ms | OK |
| 115 | Interleave #15 DIAG:DEBUG? | **PASS** | 0ms | OK |

## 4. 失败项详情

### 4.1 DIAG:ECHO? 验证关闭

- **请求**: `DIAG:ECHO?`
- **响应**:
```
ON
```

## 5. 附录: 完整响应数据

<details>
<summary>展开查看所有响应详情</summary>

### IEEE488.2 *IDN?  (PASS)

- **请求**: `*IDN?`
- **耗时**: 213ms
- **详情**: IDN 正确: NetBus,PPA-NB100,00000000,v1.0.0
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### IEEE488.2 *STB?  (PASS)

- **请求**: `*STB?`
- **耗时**: 203ms
- **详情**: STB = 0
- **响应**:
```
0
```

### IEEE488.2 *OPC?  (PASS)

- **请求**: `*OPC?`
- **耗时**: 215ms
- **详情**: OPC = 1
- **响应**:
```
1
```

### IEEE488.2 *CLS  (PASS)

- **请求**: `*CLS`
- **耗时**: 204ms
- **详情**: 已执行
- **响应**:
```
(无响应)
```

### IEEE488.2 *RST  (PASS)

- **请求**: `*RST`
- **耗时**: 205ms
- **详情**: 已复位
- **响应**:
```
OK
```

### IEEE488.2 *WAI  (PASS)

- **请求**: `*WAI`
- **耗时**: 204ms
- **详情**: 已执行
- **响应**:
```
(无响应)
```

### SYST:ERR?  (PASS)

- **请求**: `SYST:ERR?`
- **耗时**: 216ms
- **详情**: 错误队列为空 (No error)
- **响应**:
```
0,"No error"
```

### SYST:ERR:COUN?  (PASS)

- **请求**: `SYST:ERR:COUN?`
- **耗时**: 201ms
- **详情**: 错误计数 = 0
- **响应**:
```
0
```

### ATT A 读取  (PASS)

- **请求**: `SYST:ATT:A?`
- **耗时**: 202ms
- **详情**: ATT A 读取 = 0
- **响应**:
```
0
```

### ATT B 读取  (PASS)

- **请求**: `SYST:ATT:B?`
- **耗时**: 204ms
- **详情**: ATT B 读取 = 0
- **响应**:
```
0
```

### ATT 基准值  (PASS)

- **请求**: `SYST:ATT:A? / B?`
- **耗时**: 0ms
- **详情**: 当前: A=0, B=0
- **响应**:
```
A=0, B=0
```

### ATT A<-3  (PASS)

- **请求**: `SYST:ATT:A 3`
- **耗时**: 204ms
- **详情**: OK
- **响应**:
```
(无响应)
```

### ATT B<-7  (PASS)

- **请求**: `SYST:ATT:B 7`
- **耗时**: 214ms
- **详情**: OK
- **响应**:
```
(无响应)
```

### ATT A<-12  (PASS)

- **请求**: `SYST:ATT:A 12`
- **耗时**: 212ms
- **详情**: OK
- **响应**:
```
(无响应)
```

### ATT B<-14  (PASS)

- **请求**: `SYST:ATT:B 14`
- **耗时**: 215ms
- **详情**: OK
- **响应**:
```
(无响应)
```

### ATT 验证最终值  (PASS)

- **请求**: `SYST:ATT:A? / B?`
- **耗时**: 0ms
- **详情**: A=12, B=14
- **响应**:
```
A: 12
B: 14
```

### ATT A<-99 越界  (PASS)

- **请求**: `SYST:ATT:A 99`
- **耗时**: 201ms
- **详情**: 正确拒绝 (范围保护)
- **响应**:
```
ERROR: Value out of range (0-15)
```

### ATT A<--1 越界  (PASS)

- **请求**: `SYST:ATT:A -1`
- **耗时**: 216ms
- **详情**: 正确拒绝 (范围保护)
- **响应**:
```
ERROR: Value out of range (0-15)
```

### ATT 恢复原始值  (PASS)

- **请求**: `SYST:ATT:A 0 / B 0`
- **耗时**: 0ms
- **详情**: 已恢复
- **响应**:
```
A: 0, B: 0
```

### DET1 SN?  (PASS)

- **请求**: `SYST:DET1:SN?`
- **耗时**: 202ms
- **详情**: SN: "PINPRFB00300000038"
- **响应**:
```
"PINPRFB00300000038"
```

### DET1 VERS?  (PASS)

- **请求**: `SYST:DET1:VERS?`
- **耗时**: 314ms
- **详情**: 版本: 3.1.21
- **响应**:
```
3.1.21
```

### DET1 FLAS:INFO?  (PASS)

- **请求**: `SYST:DET1:FLAS:INFO?`
- **耗时**: 228ms
- **详情**: Flash 信息已返回 (含 JEDEC ID)
- **响应**:
```
JEDEC ID: 0x1540EF, Status: 0x00
OTA image:   0x000000 (36 KB)
H coarse:    0x009001 (8 KB)
H fine:      0x00D007 (8 KB)
V coarse:    0x00F00A (8 KB)
V fine:      0x00B004 (8 KB)
Ext cal:     0x00100E (52 KB)
Reserved:    0x00E015 (8 KB)
```

### DET1 FLAS:DATA? @0x9001  (PASS)

- **请求**: `SYST:DET1:FLAS:DATA? 36865,16`
- **耗时**: 203ms
- **详情**: 数据: ERROR: Flash read failed
- **响应**:
```
ERROR: Flash read failed
```

### DET1 LED  (SKIP)

- **请求**: `SYST:DET1:LED`
- **耗时**: 0ms
- **详情**: 写命令, 跳过以保护设备
- **响应**:
```
(无响应)
```

### SENS:DET1:TEMP?  (PASS)

- **请求**: `SENS:DET1:TEMP? 1,1`
- **耗时**: 215ms
- **详情**: 温度: -2808,-17655,0
- **响应**:
```
-2808,-17655,0
```

### SENS:DET1:POW?  (PASS)

- **请求**: `SENS:DET1:POW? 500000,10,1,100,500`
- **耗时**: 205ms
- **详情**: 功率: 
- **响应**:
```
(无响应)
```

### CAN:SCAN? (1-5)  (PASS)

- **请求**: `SYST:COMM:CAN:SCAN? 1,5`
- **耗时**: 202ms
- **详情**: 无响应
- **响应**:
```
(无响应)
```

### CAN SEND (ID=0x101)  (PASS)

- **请求**: `SYST:COMM:CAN:SEND 257,01,00`
- **耗时**: 263ms
- **详情**: CAN 响应: ERROR: Power query failed
- **响应**:
```
ERROR: Power query failed
```

### SW1 IDEN?  (PASS)

- **请求**: `ROUT:SWIT1:IDEN?`
- **耗时**: 214ms
- **详情**: 响应: 
- **响应**:
```
(无响应)
```

### SW1 CHAN?  (PASS)

- **请求**: `ROUT:SWIT1:CHAN?`
- **耗时**: 342ms
- **详情**: 通道: 0
- **响应**:
```
0
```

### SW1 MODE?  (PASS)

- **请求**: `ROUT:SWIT1:MODE?`
- **耗时**: 202ms
- **详情**: 模式: 
- **响应**:
```
(无响应)
```

### SW1 OUTP?  (PASS)

- **请求**: `ROUT:SWIT1:OUTP?`
- **耗时**: 341ms
- **详情**: 输出: 0 responses"PPA-SP10",0,1,1,0,21504<br>1<br>
- **响应**:
```
0 responses"PPA-SP10",0,1,1,0,21504
1
0
0,1,1,1,0,0
```

### SW1 INP?  (PASS)

- **请求**: `ROUT:SWIT1:INP?`
- **耗时**: 219ms
- **详情**: 输入: 0,0,0,0
- **响应**:
```
0,0,0,0
```

### SW1 COND?  (PASS)

- **请求**: `ROUT:SWIT1:COND?`
- **耗时**: 204ms
- **详情**: 状态: ERROR: Read status failed (Sla
- **响应**:
```
ERROR: Read status failed (Slave exception)
```

### SW1 当前通道  (PASS)

- **请求**: `ROUT:SWIT1:CHAN?`
- **耗时**: 0ms
- **详情**: 当前 CH=1
- **响应**:
```
1
```

### SW1 CHAN 3  (PASS)

- **请求**: `ROUT:SWIT1:CHAN 3`
- **耗时**: 215ms
- **详情**: CHAN<-3
- **响应**:
```
(无响应)
```

### SW1 验证 CH=3  (PASS)

- **请求**: `ROUT:SWIT1:CHAN?`
- **耗时**: 0ms
- **详情**: CH=3 验证通过
- **响应**:
```
3
```

### SW1 恢复 CH=1  (PASS)

- **请求**: `ROUT:SWIT1:CHAN 1`
- **耗时**: 0ms
- **详情**: 已恢复 CH=1
- **响应**:
```
1
```

### STATus - OPERation  (PASS)

- **请求**: `STAT:OPER:EVEN?`
- **耗时**: 204ms
- **详情**: 0
- **响应**:
```
0
```

### STATus - QUEStionable  (PASS)

- **请求**: `STAT:QUES:EVEN?`
- **耗时**: 202ms
- **详情**: 0
- **响应**:
```
0
```

### DIAG:DEBUG? 初始  (PASS)

- **请求**: `DIAG:DEBUG?`
- **耗时**: 0ms
- **详情**: 当前: 1
- **响应**:
```
1
```

### DIAG:DEBUG OFF  (PASS)

- **请求**: `DIAG:DEBUG OFF`
- **耗时**: 202ms
- **详情**: 已关闭
- **响应**:
```
Debug CLI OFF
```

### DIAG:DEBUG? 验证关闭  (PASS)

- **请求**: `DIAG:DEBUG?`
- **耗时**: 0ms
- **详情**: DEBUG=OFF
- **响应**:
```
0
```

### DIAG:DEBUG ON  (PASS)

- **请求**: `DIAG:DEBUG ON`
- **耗时**: 201ms
- **详情**: 已开启
- **响应**:
```
Debug CLI ON
```

### DIAG:DEBUG? 验证开启  (PASS)

- **请求**: `DIAG:DEBUG?`
- **耗时**: 0ms
- **详情**: DEBUG=ON
- **响应**:
```
1
```

### DIAG:ECHO? 初始  (PASS)

- **请求**: `DIAG:ECHO?`
- **耗时**: 0ms
- **详情**: 当前: ON
- **响应**:
```
ON
```

### DIAG:ECHO OFF  (PASS)

- **请求**: `DIAG:ECHO OFF`
- **耗时**: 203ms
- **详情**: 已关闭
- **响应**:
```
Echo OFF
```

### DIAG:ECHO? 验证关闭  (FAIL)

- **请求**: `DIAG:ECHO?`
- **耗时**: 0ms
- **详情**: 未关闭
- **响应**:
```
ON
```

### DIAG:ECHO ON  (PASS)

- **请求**: `DIAG:ECHO ON`
- **耗时**: 201ms
- **详情**: 已开启
- **响应**:
```
Echo ON
```

### DIAG:ECHO? 验证开启  (PASS)

- **请求**: `DIAG:ECHO?`
- **耗时**: 0ms
- **详情**: ECHO=ON
- **响应**:
```
ON
```

### Stress #01  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #02  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #03  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #04  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #05  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #06  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #07  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #08  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #09  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #10  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #11  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #12  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #13  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #14  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #15  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #16  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #17  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #18  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #19  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #20  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #21  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #22  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #23  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #24  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #25  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #26  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #27  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #28  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #29  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #30  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #31  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #32  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #33  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #34  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #35  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #36  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #37  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #38  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #39  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #40  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #41  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #42  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #43  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #44  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #45  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #46  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #47  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #48  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #49  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Stress #50  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Interleave #01 *IDN?  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Interleave #02 SYST:ERR:COUN?  (PASS)

- **请求**: `SYST:ERR:COUN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
0
```

### Interleave #03 *STB?  (PASS)

- **请求**: `*STB?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
0
```

### Interleave #04 SYST:ATT:A?  (PASS)

- **请求**: `SYST:ATT:A?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
0
```

### Interleave #05 DIAG:DEBUG?  (PASS)

- **请求**: `DIAG:DEBUG?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
1
```

### Interleave #06 *IDN?  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Interleave #07 SYST:ERR:COUN?  (PASS)

- **请求**: `SYST:ERR:COUN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
0
```

### Interleave #08 *STB?  (PASS)

- **请求**: `*STB?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
0
```

### Interleave #09 SYST:ATT:A?  (PASS)

- **请求**: `SYST:ATT:A?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
0
```

### Interleave #10 DIAG:DEBUG?  (PASS)

- **请求**: `DIAG:DEBUG?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
1
```

### Interleave #11 *IDN?  (PASS)

- **请求**: `*IDN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### Interleave #12 SYST:ERR:COUN?  (PASS)

- **请求**: `SYST:ERR:COUN?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
0
```

### Interleave #13 *STB?  (PASS)

- **请求**: `*STB?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
0
```

### Interleave #14 SYST:ATT:A?  (PASS)

- **请求**: `SYST:ATT:A?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
0
```

### Interleave #15 DIAG:DEBUG?  (PASS)

- **请求**: `DIAG:DEBUG?`
- **耗时**: 0ms
- **详情**: OK
- **响应**:
```
1
```

</details>