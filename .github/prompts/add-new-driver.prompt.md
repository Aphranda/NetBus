---
description: "Add a new hardware driver or peripheral module to the STM32H7 project. Guides creation of header, source, initialization, and interrupt handlers."
name: "Add New Driver"
argument-hint: "e.g., I2C temperature sensor driver, SPI flash memory driver"
agent: "STM32H7 Firmware Developer"
---

I need to add a new hardware driver or peripheral module to the STM32H7 project.

Please help me:

1. **Define the interface** - Create a `.h` file with:
   - Type definitions for configuration (e.g., `I2C_Config_t`)
   - Public function signatures (init, deinit, read, write)
   - Constants and error codes

2. **Implement the driver** - Create a `.c` file with:
   - Static state management (peripheral instance, DMA handles, etc.)
   - Initialization that configures HAL and enables clocks
   - Read/write operations with proper error handling
   - ISR/callback handlers if interrupt-driven

3. **Integrate with FreeRTOS** - If needed:
   - Use queues/semaphores for thread-safe communication
   - Provide task-safe wrapper functions
   - Document blocking behavior and timeouts

4. **Review hardware details** - Based on:
   - Pinout and peripheral assignment from the `.ioc` file
   - Datasheets and reference manual requirements
   - Existing driver patterns in the App/ and Drivers/ folders

5. **Suggest placement** - Recommend where to add:
   - Header file (Core/Inc/ for HAL-level, or ThirdParty/ for custom modules)
   - Source file (Core/Src/ or ThirdParty/)
   - Task/callback integration points in freertos.c or app_tasks.c

Provide complete, ready-to-compile code with comments explaining hardware configuration and synchronization.
