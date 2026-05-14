# 调试 CLI 命令碎片化问题 — 第二次根因分析

## 回顾

上一次修复（环形缓冲区 wraparound）是正确但**不充分**的。问题依然存在。

## 新的发现

### D-Cache 分析（已排除 ❌）

- 系统初始化文件 [`system_stm32h7xx.c`](Core/Src/system_stm32h7xx.c:180) 中的 `SystemInit()` **没有调用** `SCB_EnableDCache()`
- 整个项目中没有任何地方启用 D-Cache
- 因此 D-Cache 一致性问题不适用

### 真正的第二个根因：TC 事件处理器错误重置读指针

**文件：** [`ThirdParty/Log/Src/log.c`](ThirdParty/Log/Src/log.c)

在 `HAL_UARTEx_RxEventCallback` 中，第 478-482 行的 TC 处理器：

```c
if (huart->RxEventType == HAL_UART_RXEVENT_TC)
{
    g_rb_rd_idx = wr_idx;  // wr_idx = 0 at TC
    return;
}
```

**问题：** 当 DMA CIRCULAR 模式回绕时，TC（Transfer Complete）事件触发，此时 `wr_idx = 0`。TC 处理器将 `g_rb_rd_idx` 重置为 0，导致 `[旧_g_rb_rd_idx, BUF_SIZE)` 之间的数据被静默丢弃。

**具体场景：**

1. `g_rb_rd_idx = 50`（上次处理后）
2. DMA 写入 18 字节到位置 50→63（14 字节），然后回绕到 0→3（4 字节）
3. TC 事件触发 → `g_rb_rd_idx = 0` → 位置 [50, 64) 的 14 字节丢失！
4. IDLE 事件触发 → 只读到 [0, 4) = 4 字节 → 碎片字符串

**这完美解释了日志中的碎片模式：**

| 碎片 | 说明 |
|------|------|
| `'read'` | 从 `"modbus read"` 中间开始 |
| `'odbus'` | 从 'o' 开始，偏移了 'm' |
| `'0'`, `'1'` | 参数值 |
| `'ead'` | 从 `"read"` 的中间开始 |
| `'bus'` | 从 `"modbus"` 的后半部分开始 |
| `'d'` | 单个字符 |

## 修复方案

**删除 TC 处理器代码块**（第 474-482 行）。

IDLE 处理器已经通过 Case B 正确处理了回绕情况。移除 TC 处理器后：

1. TC 触发 → 什么都不做，`g_rb_rd_idx` 保持不变
2. IDLE 触发 → Case B 读取 `[old_rd_idx, BUF_SIZE) + [0, wr_idx)` → 完整数据

## 修改内容

**文件：** `ThirdParty/Log/Src/log.c`

删除以下代码块（约 9 行）：

```c
    /* ── Handle TC (DMA wrap) events ───────────────────────────────────── */
    /* On TC, the entire buffer just wrapped. All data from the completed
     * cycle was already processed by prior IDLE events (CLI use case).
     * Reset read index to current write position to stay in sync. */
    if (huart->RxEventType == HAL_UART_RXEVENT_TC)
    {
        g_rb_rd_idx = wr_idx;
        return;
    }
```

IDLE 处理代码保持不变。
