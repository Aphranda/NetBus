# NetBus Architecture Review & Optimization Plan

> 2026-05-29 | STM32H743ZITx | FreeRTOS 10.6.2 | LWIP 2.2.1

---

## 1. System Overview

```
  UART7 (115200)            ETH (100M RMII)              UART8 (115200 RS485)
   SCPI + Debug CLI           SCPI TCP :5025               Modbus RTU Master
       │                          │                             │
       ▼                          ▼                             ▼
  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐
  │ Log Task │   │ CAN Task │   │Modbus Tsk│   │ Atten Tsk│   │ RFSW Tsk │
  │SCPI+Debug│   │FDCAN1 Drv│   │RS485+Mod │   │ IO Att A │   │RF Switch │
  │ process  │   │ idle(ISR)│   │ process  │   │   B ctrl │   │ Modbus   │
  └──────────┘   └──────────┘   └──────────┘   └──────────┘   └──────────┘
                              │
                         ┌──────────┐
                         │Detect Tsk│
                         │CAN Proto │
                         │ A1 Board │
                         └──────────┘
```

---

## 2. Task Architecture

### 2.1 FreeRTOS Tasks (CubeMX Generated)

| Task | Entry Point | CMSIS Priority | FreeRTOS Pri | Stack | Purpose |
|------|------------|----------------|-------------|-------|---------|
| `defaultTask` | `StartDefaultTask` → `App_Task_Loop()` | `osPriorityNormal` | **24** | 2048 | Module round-robin processing loop |
| `ETH_Task` | `StartETHTask` → `MX_LWIP_Init()` | `osPriorityNormal` | **24** | 4096 | LWIP init + SCPI TCP server |
| `Modbus_Task` | `StartModbusTask` → `Modbus_Task_Loop()` | `osPriorityLow` | **8** | 2048 | RS485 RX monitoring (`osDelay(10)`) |
| `Can_Task` | `StartCanTask` → `Can_Task_Loop()` | `osPriorityLow` | **8** | 2048 | CAN idle loop (`osDelay(100)`) |

### 2.2 LWIP Internal Threads

| Thread | Priority | Stack | Created By |
|--------|----------|-------|------------|
| `tcpip_thread` | **24** (TCPIP_THREAD_PRIO) | 1024 | `tcpip_init()` |
| `EthIf` (ethernetif_input) | `osPriorityRealtime` (**48**) | 350 | `low_level_init()` |
| `EthLink` (link monitor) | `osPriorityBelowNormal` (**16**) | 1024 | `MX_LWIP_Init()` |
| `SCPI_TCP` | `osPriorityNormal` (**24**) | 2048 | `NetSCPI_Init()` |

### 2.3 Task Priority Map

```
Priority 48 ── EthIf (ethernetif_input)     ← highest, handles RX packets
Priority 40 ── osPriorityHigh
Priority 32 ── osPriorityAboveNormal
Priority 24 ── defaultTask / ETH_Task / tcpip / SCPI_TCP   ← NORMAL tier
Priority 16 ── EthLink                          ← BelowNormal
Priority  8 ── Modbus_Task / Can_Task           ← LOW tier
Priority  0 ── Idle task
```

**Current Issue**: `defaultTask`, `ETH_Task`, `tcpip`, and `SCPI_TCP` all share priority 24. FreeRTOS time-slices them equally. When `defaultTask`'s `osDelay(1)` expires, it competes with `tcpip` for CPU time, potentially delaying packet processing.

**Recommendation**: Raise `tcpip` to 25 (osPriorityNormal1) to give LWIP core thread a slight edge over application tasks.

---

## 3. NVIC Interrupt Priority Map

STM32H7 uses 4-bit priority (0-15). **Lower number = higher priority**.

`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5` — interrupts with priority **5-15** may call FreeRTOS ISR-safe APIs.

| IRQ | Priority | Peripheral | FreeRTOS API Safe? |
|-----|----------|-----------|-------------------|
| SysTick | 15 | System tick | No (kernel internal) |
| PendSV | 15 | Context switch | No (kernel internal) |
| TIM1_UP | 15 | HAL timebase | No |
| DMA1_Stream0 | **8** | UART7 RX | Yes (>=5) |
| DMA1_Stream1 | **9** | UART7 TX | Yes |
| DMA1_Stream2 | **9** | UART8 RX | Yes |
| DMA1_Stream3 | **10** | UART8 TX | Yes |
| UART7 | **9** | Log/SCPI console | Yes |
| UART8 | **8** | RS485/Modbus | Yes |
| ETH | **6** | Ethernet MAC | Yes |
| ETH_WKUP | **6** | Ethernet wake-up | Yes |
| FDCAN1_IT0 | **7** | CAN bus RX | Yes |
| FDCAN1_IT1 | **7** | CAN bus TX | Yes |

**Observations**:
- ETH (pri 6) can preempt FDCAN (7), UART (8-9), and DMA (8-10). Good for low-latency packet handling.
- FDCAN (7) can preempt UART/DMA. If CAN traffic is heavy, it could delay UART DMA ISR completion.
- UART7 DMA RX (pri 8) and UART7 (pri 9) are on different priority levels — the UART IDLE ISR (pri 9) runs at *lower* priority than the DMA TC ISR (pri 8). This is correct standard STM32CubeMX configuration but worth noting.

---

## 4. Module Registration System

### 4.1 Registration Order (in `App_Init()`)

| Index | Module | Init Function | Process Function | Owns |
|-------|--------|--------------|-----------------|------|
| 0 | **Log** | `Log_Init()` + `SCPI_SystemInit()` | `_log_task_process()` | UART7, SCPI parser, Debug CLI |
| 1 | **CAN** | `CAN_Init()` + register "can" CLI | **NULL** (idle in Can_Task) | FDCAN1 peripheral |
| 2 | **Modbus** | `RS485_Init()` + `Modbus_Init()` + register "modbus" CLI | **NULL** (runs in Modbus_Task) | UART8 RS485, Modbus RTU |
| 3 | **Attenuator** | register "att" CLI | `_attenuator_task_process()` (no-op) | IO attenuator pins |
| 4 | **RFSW** | register "rfsw" CLI | `_rfsw_task_process()` | RF switch Modbus ops |
| 5 | **Detector** | register "detector" CLI | `_detector_task_process()` (no-op) | A1 detector CAN protocol |

### 4.2 Module Lifecycle

```
main()
  ├─ MX_GPIO_Init(), MX_DMA_Init(), MX_FDCAN1_Init()
  ├─ MX_UART7_Init(), MX_UART8_Init()
  ├─ App_Init()
  │    ├─ App_RegisterModule() × 6    ← Register all modules in order
  │    └─ App_Task_Init()
  │         └─ for each module: module->init()   ← Init in registration order
  ├─ osKernelInitialize()
  ├─ MX_FREERTOS_Init()              ← Create mutexes + 4 FreeRTOS tasks
  └─ osKernelStart()                  ← Scheduler takes over
```

### 4.3 Command Dispatch (per cycle in defaultTask)

```
App_Task_Loop() → for each module:
  1. Log     → Log_DbgGetLine() → Known debug cmd? → Debug CLI first → else SCPI → else Debug CLI fallback
  2. CAN     → process=NULL → skip → osDelay(1)
  3. Modbus  → process=NULL → skip → osDelay(1)
  4. Atten   → process=no-op → osDelay(1)
  5. RFSW    → RS485_Available() check → osDelay(1)
  6. Detect  → process=no-op → osDelay(1)

Per iteration: ~6 × osDelay(1) = minimum 6ms between cycles
```

---

## 5. Data Flow: UART DMA RX Pipeline

### 5.1 UART7 (Log/SCPI Console)

```
UART7 RX ──DMA1_Stream0 (CIRCULAR)──▶ g_dma_rx_buf[256] (32B aligned)
                                            │
                     IDLE line detected ────┘
                                            │
                     UART7_IRQHandler (pri 9)
                       └─ HAL_UARTEx_RxEventCallback()
                            ├─ SCB_InvalidateDCache_by_Addr()  ← Cache coherency
                            ├─ Compute wr_idx from NDTR
                            ├─ Assemble bytes into lines
                            └─ osMessageQueuePut(g_line_queue, line)
                                                   │
                    ┌──────────────────────────────┘
                    │  Queue: 4 lines × 128 bytes
                    ▼
              Log_DbgGetLine() (from _log_task_process)
                    │
              ┌─────┴─────┐
              ▼           ▼
        Debug CLI       SCPI_TryParse()
        (help/can/      (SCPI_Parse → command callbacks)
         modbus/att/        │
         rfsw/detector)     ├─ SCPI_Write() → Log_WriteRaw() → HAL_UART_Transmit(UART7)
              │             └─ NetSCPI_Write() → lwip_send(client_fd) [TCP]
              ▼
        Log_Print() → HAL_UART_Transmit(UART7)
```

### 5.2 UART8 (RS485/Modbus)

```
UART8 RX ──DMA1_Stream2 (CIRCULAR)──▶ g_rs485.rx_ring.buf[256] (32B aligned)
                                            │
                     IDLE line detected ────┘
                                            │
                     UART8_IRQHandler (pri 8)
                       └─ HAL_UARTEx_RxEventCallback()
                            └─ RS485_UART_RxEventCallback()
                                 ├─ SCB_InvalidateDCache_by_Addr()
                                 ├─ Compute head from NDTR
                                 └─ Invoke rx_callback (if registered)

Application reads via RS485_Available() / RS485_Receive()
  - _rfsw_task_process() in defaultTask (pri 24)
  - _modbus_task_process() in Modbus_Task (pri 8)
Both check Modbus_IsTransactionPending() before consuming.
```

---

## 6. Key Buffer & Queue Sizing

| Component | Parameter | Size | Notes |
|-----------|----------|------|-------|
| UART7 DMA RX | `LOG_DBG_BUF_SIZE` | 256 B | Ring buffer, 32B aligned |
| UART7 Line Queue | `LOG_DBG_QUEUE_SIZE` | **4 lines** | 128 B each, can overflow |
| UART7 Max Line | `LOG_DBG_LINE_LEN` | 128 B | Truncation if exceeded |
| UART8 DMA RX | `RS485_RX_BUF_SIZE` | 256 B | Ring buffer, 32B aligned |
| SCPI Input Buffer | `SCPI_INPUT_BUFFER_LENGTH` | 256 B | Command line buffer |
| SCPI Commands | `scpi_commands[]` | **44 commands** | Static table |
| Debug Commands | `LOG_DBG_MAX_CMDS` | **10 slots** | 6 used (help/can/modbus/att/rfsw/detector) |
| Modbus Frame | `MODBUS_MAX_FRAME_LEN` | 256 B | Max RTU frame |
| CAN Data | `CAN_MAX_DATA_LEN` | 64 B | CAN FD with BRS |
| TCP SCPI RX | `SCPI_TCP_BUF_SIZE` | 512 B | Socket read buffer |
| TCP SCPI Stack | `SCPI_TCP_STACK_SIZE` | 2048 B | SCPI_TCP thread |
| LWIP Heap | `MEM_SIZE` | 14336 B | RAM heap at 0x30044000 |
| FreeRTOS Heap | `configTOTAL_HEAP_SIZE` | 40960 B | Dynamic allocations |

---

## 7. Memory Layout (MPU)

| Region | Address | Size | Cacheable | Bufferable | Purpose |
|--------|---------|------|-----------|------------|---------|
| MPU Region 0 | 0x30040000 | 256 B | **No** | Yes | ETH DMA descriptors |
| MPU Region 1 | 0x30044000 | 16 KB | **Yes** | No | LWIP RAM heap |
| Background (DTCM) | 0x20000000 | 128 KB | **No** (default) | — | Static data, stacks, DMA buffers |

**Key Point**: DMA buffers (`g_dma_rx_buf`, `g_rs485.rx_ring.buf`) are likely in DTCM (non-cacheable). The `SCB_InvalidateDCache_by_Addr()` calls are **no-ops** on non-cacheable memory — they're safe but unnecessary. However, the 32-byte alignment is still important as insurance against future memory layout changes.

---

## 8. Mutex Inventory

| Mutex | Purpose | Held By |
|-------|---------|---------|
| `g_uart_tx_mutex` | Serialize UART7 TX (HAL_UART_Transmit blocking) | Log_Print, Log_WriteRaw |
| `SCPI_Mutex` (CubeMX) | SCPI parser context protection | SCPI_TryParse |
| `Modbus_Mutex` (CubeMX) | Modbus transaction serialization | Modbus API calls |

**Current Issue**: `g_uart_tx_mutex` is held during blocking `HAL_UART_Transmit()` which can take 10-50ms per call. During this time, the `defaultTask` is blocked, and the Debug CLI / SCPI line queue may fill up (only 4 slots).

---

## 9. Identified Issues & Optimization Plan

### 9.1 Critical — UART7 Line Queue Overflow

**Problem**: `LOG_DBG_QUEUE_SIZE = 4` is too small. A single `help` output takes ~43ms at 115200 baud. If the user sends commands while output is still in progress, the ISR pushes lines to the queue, and after 4 lines, subsequent lines are silently dropped.

**Fix**: Increase `LOG_DBG_QUEUE_SIZE` from 4 to **8**. Cost: 4 × 128 = 512 extra bytes.

**File**: [ThirdParty/Log/Src/log.c:56](ThirdParty/Log/Src/log.c#L56)

### 9.2 Critical — TCP/IP Thread Priority

**Problem**: `TCPIP_THREAD_PRIO = 24` is equal to `defaultTask` and `ETH_Task`. Under heavy application load, round-robin scheduling could delay TCP packet processing.

**Fix**: Set `TCPIP_THREAD_PRIO` to **25** in `lwipopts.h`. This gives LWIP core a slight edge.

**File**: [LWIP/Target/lwipopts.h:83](LWIP/Target/lwipopts.h#L83)

### 9.3 Important — Blocking UART TX Blocks defaultTask

**Problem**: `Log_Print()` → `HAL_UART_Transmit()` is blocking. With verbose logging and help output, each LOG_INFO call blocks for ~1-5ms while holding `g_uart_tx_mutex`. This stalls the entire `App_Task_Loop()` iteration.

**Mitigation**: 
- Reduce log level to `LOG_LEVEL_INFO` in production (already default).
- Consider interrupt-driven TX (DMA) for UART7 in the future.

### 9.4 Important — ETH_Task Idle Loop Timing

**Problem**: `StartETHTask` sleeps 1000ms between iterations. During startup, this means `NetSCPI_Init()` happens, then the task idles for 1000ms. Not harmful, but unnecessary — the SCPI_TCP thread is independent.

**Fix**: Reduce to `osDelay(100)` or remove sleep entirely (use `osThreadSuspend` or `vTaskSuspend(NULL)`).

**File**: [APP/Src/app_task.c:275](APP/Src/app_task.c#L275)

### 9.5 Medium — UART7 DMA Rx Event Callback Shared

**Problem**: `HAL_UARTEx_RxEventCallback` handles both UART7 (log) and UART8 (RS485). The dispatch uses `if (huart == &huart8)` check, which is fine, but both paths run in their respective ISR contexts. The UART8 ISR (pri 8) can nest inside UART7 ISR (pri 9) since UART8 has numerically lower (higher logical) priority.

**Risk**: If RS485 RX heavy traffic fires UART8 ISR while UART7 ISR is processing, both access their own DMA buffers — not a conflict but adds latency.

### 9.6 Medium — Debug Command `_hex_to_byte` Duplication

**Problem**: Both `can_task.c` and `modbus_task.c` have their own `_hex_to_byte()` static function. Identical logic, code duplication.

**Fix**: Move to a shared utility header or log.h.

### 9.7 Low — App_Task_Loop osDelay Granularity

**Problem**: `osDelay(1)` is called for every module (6 times per loop), for 6ms minimum cycle time. For modules with NULL process (CAN, Modbus), this is wasted delay.

**Optimization**: Only `osDelay(1)` for modules that actually have a `process` function, or use a single `osDelay(1)` per loop iteration instead of per-module.

**File**: [APP/Src/app_task.c:229](APP/Src/app_task.c#L229)

### 9.8 Low — STM32H7 ICache/DCache Enabled Without DMA Buffer Management

**Problem**: D-Cache and I-Cache are enabled in `main()`:
```c
SCB_EnableICache();
SCB_EnableDCache();
```

DMA buffers in DTCM (non-cacheable) are safe. But if any DMA buffer migrates to cacheable SRAM (e.g., AXI SRAM 0x24000000), cache coherency bugs will appear silently. The 32-byte alignment + `SCB_InvalidateDCache_by_Addr` pattern is the first line of defense, but future developers must be aware.

**Recommendation**: Add a compile-time assertion or MPU region for DMA buffers to guarantee non-cacheable placement.

---

## 10. Startup Sequence Diagram

```
Power-on / Reset
  │
  ├─ MPU_Config()         ← ETH descriptors (non-cacheable) + LWIP heap (cacheable)
  ├─ SCB_EnableICache()
  ├─ SCB_EnableDCache()   ← D-Cache ON — all DMA buffers need 32B alignment
  ├─ HAL_Init()
  ├─ SystemClock_Config() ← HSE 25MHz → PLL → 480MHz SYSCLK
  ├─ MX_GPIO_Init()
  ├─ MX_DMA_Init()        ← DMA1 config with NVIC priorities 8-10
  ├─ MX_FDCAN1_Init()     ← FDCAN1 with NVIC priorities 7,7
  ├─ MX_UART7_Init()      ← UART7 (log console) with NVIC priority 9
  ├─ MX_UART8_Init()      ← UART8 (RS485) with NVIC priority 8
  │
  ├─ App_Init()           ← Sequential module registration + init
  │    ├─ [0] Log:     Log_Init() → UART7 DMA RX start + SCPI init
  │    ├─ [1] CAN:     CAN_Init() → FDCAN1 ready
  │    ├─ [2] Modbus:  RS485_Init() → UART8 DMA RX start + Modbus_Init()
  │    ├─ [3] Atten:   register "att" CLI
  │    ├─ [4] RFSW:    register "rfsw" CLI
  │    └─ [5] Detect:  register "detector" CLI
  │
  ├─ osKernelInitialize()
  ├─ MX_FREERTOS_Init()
  │    ├─ osMutexNew(SCPI_Mutex)
  │    ├─ osMutexNew(Modbus_Mutex)
  │    ├─ osThreadNew(defaultTask,   pri 24, stack 2048)
  │    ├─ osThreadNew(ETH_Task,      pri 24, stack 4096)
  │    ├─ osThreadNew(Modbus_Task,   pri  8, stack 2048)
  │    └─ osThreadNew(Can_Task,      pri  8, stack 2048)
  │
  └─ osKernelStart()      ← Scheduler takes over
       │
       ├─ defaultTask: App_Task_Loop()
       │    └─ LOG_INFO("FreeRTOS started, entering main loop (6 modules)")
       │
       └─ ETH_Task: MX_LWIP_Init() → LWIP + PHY + ETH start
            └─ NetSCPI_Init() → SCPI TCP thread (osDelay(2000) → listen :5025)
```

---

## 11. Performance Budget

| Metric | Value | Notes |
|--------|-------|-------|
| CPU Clock | 480 MHz | HSE 25MHz × PLL |
| FreeRTOS Tick | 1 kHz (1ms) | `configTICK_RATE_HZ = 1000` |
| UART Baud | 115200 | ~11.5 KB/s, ~87us/char |
| Help output (~500 chars) | ~43 ms | Blocking TX |
| SCPI parse (typical) | <100 us | Mutex-protected |
| Modbus RTU timeout | Configurable | `Modbus_GetTimeout()` |
| CAN FD bitrate | Up to 5 Mbps | BRS enabled |
| ETH PHY | LAN8742, RMII, 100M | Full duplex |

---

## 12. Checklist for Next PR

- [ ] Raise `TCPIP_THREAD_PRIO` to 25
- [ ] Increase `LOG_DBG_QUEUE_SIZE` to 8
- [ ] Reduce ETH_Task idle `osDelay(1000)` → `osDelay(100)` or `vTaskSuspend`
- [ ] Add `configASSERT` for DMA buffer alignment (compile-time check: `_Alignof(g_dma_rx_buf) >= 32`)
- [ ] Extract common `_hex_to_byte()` to shared utility
- [ ] Consider per-loop `osDelay` optimization (one delay per iteration, not per module)
- [ ] Audit all DMA buffer locations — verify non-cacheable region or 32B-aligned + SCB invalidate
