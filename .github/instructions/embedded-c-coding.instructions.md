---
description: "Use when writing embedded C code for STM32H7, including drivers, HAL configuration, FreeRTOS tasks, and interrupt handlers. Covers safety, memory management, and real-time constraints."
name: "Embedded C Coding Standards"
applyTo: ["**/*.c", "**/*.h", "App/**", "Core/Src/**", "Drivers/**"]
---

# STM32H7 Embedded C Coding Standards

## Memory & Performance

- **Stack safety**: Monitor stack usage in FreeRTOS; use task stack sizes conservatively (default 256–512 words for simple tasks)
- **const correctness**: Mark read-only buffers and function parameters as `const` to reduce RAM and enable compiler optimizations
- **Volatile**: Use `volatile` for hardware registers, interrupt-modified variables, and DMA buffers
- **Static allocation**: Prefer static allocation over malloc in embedded contexts to avoid fragmentation and runtime failures

## Interrupt & Timing

- **Keep ISRs short**: Defer work to task-level handlers via queues/semaphores; use `BaseType_t xHigherPriorityTaskWoken`
- **Critical sections**: Use `portENTER_CRITICAL()` / `portEXIT_CRITICAL()` only for brief operations; prefer semaphores/mutexes for longer operations
- **Peripheral initialization order**: Initialize clocks → GPIO → peripherals → interrupts (enable at the end)

## FreeRTOS Best Practices

- **Task priorities**: Higher values = higher priority; ensure idle task is lowest priority (0)
- **Blocking calls**: Use timeouts (not `portMAX_DELAY`) to detect deadlocks; prefer event-driven patterns
- **Memory**: Declare static task stacks and queue buffers; avoid heap fragmentation via `pvPortMalloc()`
- **Handle context**: Check `xHigherPriorityTaskWoken` after ISR calls to `xQueueSendFromISR()`, `xSemaphoreGiveFromISR()`

## Code Structure

```c
// Header (.h)
#ifndef __MODULE_NAME_H__
#define __MODULE_NAME_H__

#include <stdint.h>
#include "stm32h7xx_hal.h"

// Public types
typedef struct {
    uint32_t timeout_ms;
    GPIO_TypeDef *port;
    uint16_t pin;
} GPIO_Config_t;

// Public functions
HAL_StatusTypeDef GPIO_Init(const GPIO_Config_t *config);
void GPIO_DeInit(void);

#endif // __MODULE_NAME_H__
```

```c
// Source (.c)
#include "module.h"

// Private static variables
static GPIO_TypeDef *g_gpio_port = NULL;
static uint16_t g_gpio_pin = 0;

// Private functions
static HAL_StatusTypeDef _validate_config(const GPIO_Config_t *config);

HAL_StatusTypeDef GPIO_Init(const GPIO_Config_t *config) {
    if (config == NULL || _validate_config(config) != HAL_OK) {
        return HAL_ERROR;
    }
    
    g_gpio_port = config->port;
    g_gpio_pin = config->pin;
    
    // HAL initialization
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Pin = g_gpio_pin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    
    HAL_GPIO_Init(g_gpio_port, &gpio_init);
    return HAL_OK;
}
```

## Error Handling

- **Check return codes**: All HAL calls return `HAL_StatusTypeDef` or similar; never ignore errors
- **Graceful degradation**: Log errors with context; continue operation when safe (e.g., retrying failed communication)
- **Assertions**: Use `assert()` during development to catch logic errors early; remove in production builds

## Common Pitfalls

- ❌ Calling HAL functions from ISRs without checking `FromISR()` variants
- ❌ Blocking in ISRs or high-priority tasks (use queues instead)
- ❌ Uninitialized peripherals (always call `HAL_*_Init()` before use)
- ❌ Forgetting `__HAL_*_CLK_ENABLE()` macros for clock gating
- ❌ Static allocations without checking size (use `configTOTAL_HEAP_SIZE` for FreeRTOS)

## Documentation

- Comment **why**, not what: `// Delay ensures capacitor discharge (CAN transceiver recovery)`
- Document non-obvious parameters: HAL timeout values, queue sizes, task stack depths
- Link to relevant datasheets or reference manuals in complex routines
