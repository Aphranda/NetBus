# NetBus 控制功能测试报告

**生成时间**: 2026-05-21 17:06:48  
**测试端口**: COM8 @ 115200 bps  
**测试模式**: scpi (Modbus addr=1, CAN node=1)  
**安全策略**: 仅读取/查询/控制，不修改从机或主机配置  

---

## 测试摘要

| 指标 | 值 |
|------|----|
| 总测试项 | 29 |
| PASS | 28 |
| FAIL | 1 |
| WARN | 0 |
| SKIP | 0 |
| 通过率 | 96.6% |
| 总耗时 | 11.1s |

### 子系统结果

| 子系统 | 命令 | 通过 | 说明 |
|--------|------|------|------|
| CLI 基础 | help, att | 0 | 本地衰减器 & 帮助 |
| RFSW (Modbus) | rfsw | 0 | RS485 远程 RF Switch |
| CAN 总线 | can send/scan | 1 | CAN 查询 & 扫描 |
| Modbus Raw | modbus | 0 | Modbus 原生帧注入 |
| Detector (CAN) | detector | 0 | A1 检波板指令 |
| SCPI (IEEE 488.2) | *IDN?, SYST, ROUT, STAT, DIAG | 28 | SCPI 标准指令集 |

## 测试明细

| # | 测试项 | 状态 | 耗时 | 详情 |
|---|--------|------|------|------|
| 1 | SCPI *IDN? - 标识查询 | PASS | 218ms | SCPI IDN 正确: NetBus,PPA-NB100,00000000,v1.0.0 |
| 2 | SCPI *STB? - 状态字节 | PASS | 218ms | STB = 0 |
| 3 | SCPI *OPC? - 操作完成 | PASS | 217ms | OPC=1 (操作完成) |
| 4 | SCPI *CLS - 清除状态 | PASS | 1163ms | *CLS 已执行 |
| 5 | SCPI *RST - 复位 | PASS | 1169ms | *RST 已执行 |
| 6 | SCPI SYST:ERR? - 错误队列 | PASS | 215ms | 错误队列为空 |
| 7 | SCPI 读取衰减器A | PASS | 218ms | 读取衰减器A = 0 |
| 8 | SCPI 读取衰减器B | PASS | 219ms | 读取衰减器B = 0 |
| 9 | SCPI ATT - 读取当前值 | PASS | 0ms | 基准值 |
| 10 | SCPI ATT - A<-5 | PASS | 1155ms | 设置成功 (target=5) |
| 11 | SCPI ATT - B<-10 | PASS | 1165ms | 设置成功 (target=10) |
| 12 | SCPI ATT - 验证设置 | PASS | 0ms | A=5, B=10 验证通过 |
| 13 | SCPI ATT - 恢复原始值 | PASS | 0ms | 衰减器已恢复 |
| 14 | SCPI DIAG:DEBUG? - 调试状态 | PASS | 218ms | Debug CLI = 1 |
| 15 | SCPI DIAG:ECHO? - 回显状态 | PASS | 214ms | Echo = ON |
| 16 | SCPI DIAG:DEBUG? - 当前状态 | PASS | 0ms | 当前: 1 |
| 17 | SCPI DIAG:DEBUG OFF | PASS | 1166ms | 已关闭调试 |
| 18 | SCPI DIAG:DEBUG? - 验证关闭 | PASS | 0ms | 状态: 0 |
| 19 | SCPI DIAG:DEBUG ON | PASS | 1169ms | 已开启调试 |
| 20 | SCPI DIAG:DEBUG? - 验证恢复 | PASS | 0ms | 已恢复: 1 |
| 21 | SCPI CAN:SCAN? (1-5) | PASS | 566ms | CAN 扫描完成 |
| 22 | SCPI ROUT:SWIT1:CHAN? - 通道查询 | PASS | 223ms | 通道: 1 |
| 23 | SCPI ROUT:SWIT1:IDEN? - 设备身份 | PASS | 220ms | 设备信息已返回 |
| 24 | SCPI ROUT:SWIT1:OUTP? - 输出线圈 | PASS | 219ms | 输出线圈: 0,1,1,1,0,0 |
| 25 | SCPI ROUT:SWIT1:INP? - 离散输入 | PASS | 217ms | 离散输入: 0,0,0,0 |
| 26 | SCPI ROUT:SWIT1:MODE? - 工作模式 | PASS | 218ms | 模式: 0 |
| 27 | SCPI ROUT:SWIT1:COND? - 状态寄存器 | **FAIL** | 280ms | SCPI 未识别 |
| 28 | SCPI STAT:OPER:EVEN? - 操作状态 | PASS | 220ms | 操作状态: 0 |
| 29 | SCPI STAT:QUES:EVEN? - 可疑状态 | PASS | 220ms | 可疑状态: 0 |

## 失败项详情

### SCPI ROUT:SWIT1:COND? - 状态寄存器

- **请求**: `ROUT:SWIT1:COND?`
- **响应**:
```
ERROR: Read status failed (Slave exception)[62342] [ERR] SCPI: -200, "Execution error"
[62343] [INFO] Unknown command: 'ROUT:SWIT1:COND?'. Type 'help' for available commands.
```

## 附录: 完整响应数据

<details>
<summary>展开查看所有响应详情</summary>

### SCPI *IDN? - 标识查询

- **请求**: `*IDN?`
- **响应**:
```
NetBus,PPA-NB100,00000000,v1.0.0
```

### SCPI *STB? - 状态字节

- **请求**: `*STB?`
- **响应**:
```
0
```

### SCPI *OPC? - 操作完成

- **请求**: `*OPC?`
- **响应**:
```
1
```

### SCPI *CLS - 清除状态

- **请求**: `*CLS`
- **响应**:
```
(无响应)
```

### SCPI *RST - 复位

- **请求**: `*RST`
- **响应**:
```
OK
```

### SCPI SYST:ERR? - 错误队列

- **请求**: `SYST:ERR?`
- **响应**:
```
0,"No error"
```

### SCPI 读取衰减器A

- **请求**: `SYST:ATT:A?`
- **响应**:
```
0
```

### SCPI 读取衰减器B

- **请求**: `SYST:ATT:B?`
- **响应**:
```
0
```

### SCPI ATT - 读取当前值

- **请求**: `SYST:ATT:A? / SYST:ATT:B?`
- **响应**:
```
A=0, B=0
```

### SCPI ATT - A<-5

- **请求**: `SYST:ATT:A 5`
- **响应**:
```
(无响应)
```

### SCPI ATT - B<-10

- **请求**: `SYST:ATT:B 10`
- **响应**:
```
(无响应)
```

### SCPI ATT - 验证设置

- **请求**: `SYST:ATT:A? / SYST:ATT:B?`
- **响应**:
```
A: 5
B: 10
```

### SCPI ATT - 恢复原始值

- **请求**: `SYST:ATT:A 0 / SYST:ATT:B 0`
- **响应**:
```
已恢复 A=0, B=0
```

### SCPI DIAG:DEBUG? - 调试状态

- **请求**: `DIAG:DEBUG?`
- **响应**:
```
1
```

### SCPI DIAG:ECHO? - 回显状态

- **请求**: `DIAG:ECHO?`
- **响应**:
```
ON
```

### SCPI DIAG:DEBUG? - 当前状态

- **请求**: `DIAG:DEBUG?`
- **响应**:
```
1
```

### SCPI DIAG:DEBUG OFF

- **请求**: `DIAG:DEBUG OFF`
- **响应**:
```
Debug CLI OFF
```

### SCPI DIAG:DEBUG? - 验证关闭

- **请求**: `DIAG:DEBUG?`
- **响应**:
```
0
```

### SCPI DIAG:DEBUG ON

- **请求**: `DIAG:DEBUG ON`
- **响应**:
```
Debug CLI ON
```

### SCPI DIAG:DEBUG? - 验证恢复

- **请求**: `DIAG:DEBUG?`
- **响应**:
```
1
```

### SCPI CAN:SCAN? (1-5)

- **请求**: `SYST:COMM:CAN:SCAN? 1,5`
- **响应**:
```
1,1,18,"PINPRFB00300000038"1
```

### SCPI ROUT:SWIT1:CHAN? - 通道查询

- **请求**: `ROUT:SWIT1:CHAN?`
- **响应**:
```
1
```

### SCPI ROUT:SWIT1:IDEN? - 设备身份

- **请求**: `ROUT:SWIT1:IDEN?`
- **响应**:
```
"PPA-SP10",0,1,1,0,21504
```

### SCPI ROUT:SWIT1:OUTP? - 输出线圈

- **请求**: `ROUT:SWIT1:OUTP?`
- **响应**:
```
0,1,1,1,0,0
```

### SCPI ROUT:SWIT1:INP? - 离散输入

- **请求**: `ROUT:SWIT1:INP?`
- **响应**:
```
0,0,0,0
```

### SCPI ROUT:SWIT1:MODE? - 工作模式

- **请求**: `ROUT:SWIT1:MODE?`
- **响应**:
```
0
```

### SCPI ROUT:SWIT1:COND? - 状态寄存器

- **请求**: `ROUT:SWIT1:COND?`
- **响应**:
```
ERROR: Read status failed (Slave exception)[62342] [ERR] SCPI: -200, "Execution error"
[62343] [INFO] Unknown command: 'ROUT:SWIT1:COND?'. Type 'help' for available commands.
```

### SCPI STAT:OPER:EVEN? - 操作状态

- **请求**: `STAT:OPER:EVEN?`
- **响应**:
```
0
```

### SCPI STAT:QUES:EVEN? - 可疑状态

- **请求**: `STAT:QUES:EVEN?`
- **响应**:
```
0
```

</details>