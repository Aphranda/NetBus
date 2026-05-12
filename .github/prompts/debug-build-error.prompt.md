---
description: "Diagnose and fix STM32H7 compilation, linking, or build errors. Analyzes error messages to identify root causes and provides solutions."
name: "Debug Build Error"
argument-hint: "Paste the full build error or describe the issue"
agent: "STM32H7 Firmware Developer"
tools: [read, search, execute]
---

Help me diagnose and fix this build error in the STM32H7 project.

Please:

1. **Parse the error message** - Identify:
   - Error type (compilation, linking, assembly, code generation)
   - Affected file(s) and line numbers
   - Specific problem (undefined reference, type mismatch, missing include, etc.)

2. **Trace the root cause**:
   - Check if headers are included in the right order
   - Verify #define guards and conditional compilation
   - Look for missing function implementations or circular dependencies
   - Check build configuration (optimization level, compiler flags)

3. **Inspect relevant code**:
   - Read the offending file and related headers
   - Search for symbol definitions and their usage
   - Check linker script and memory layout if it's a linking error

4. **Provide the fix**:
   - Show the exact code change with full context
   - Explain why the error occurred
   - Suggest preventative measures

5. **Verify the solution**:
   - Run a clean build to confirm the fix
   - Highlight any warnings that may emerge

Include the full error message, the affected code section, and step-by-step debugging instructions.
