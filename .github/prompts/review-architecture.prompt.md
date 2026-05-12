---
description: "Review and suggest improvements to the STM32H7 project architecture, module organization, data flow, and design patterns."
name: "Review Project Architecture"
argument-hint: "e.g., Evaluate FreeRTOS task design, Check module dependencies, Assess communication protocol handling"
agent: "STM32H7 Firmware Developer"
tools: [read, search]
---

Perform an architecture review of the STM32H7 project with focus on:

1. **Module Organization**
   - Are Core, Drivers, App, and Middleware layers clearly separated?
   - Do public headers expose only necessary interfaces?
   - Are there circular dependencies or over-coupling?

2. **Data Flow & Communication**
   - How do modules exchange data (queues, callbacks, direct calls)?
   - Are FreeRTOS primitives (queues, semaphores, mutexes) used appropriately?
   - Is there clear ownership of resources?

3. **Real-Time Behavior**
   - Are task priorities and stack depths appropriate?
   - Are critical sections and ISRs kept short?
   - Is there proper synchronization around shared data?

4. **Error Handling & Robustness**
   - How are errors propagated through the stack?
   - Are there timeout mechanisms to detect deadlocks?
   - Is there graceful degradation for non-critical failures?

5. **Memory Management**
   - Is heap fragmentation a risk? (prefer static allocation)
   - Are stack overflows caught early?
   - Is CCRAM/ITCM/DTCM utilized for performance-critical code?

Please analyze the existing code structure (Core/, App/, Drivers/, Middlewares/) and provide:
- Strengths of the current architecture
- Potential issues or anti-patterns
- Specific recommendations with examples
- Priority of changes (high/medium/low impact)
