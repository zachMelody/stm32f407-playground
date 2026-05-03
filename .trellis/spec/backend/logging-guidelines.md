# Logging Guidelines

> How logging / debug output works in this STM32F407 embedded project.

---

## Overview

This is an embedded project with no structured logging framework. Debug output options depend on available peripherals and toolchain.

---

## Available Output Methods

### 1. Semihosting (Debugger printf)

The `syscalls.c` file provides `_write()` that can redirect `printf` to the debugger console via semihosting (ITM/SWO).

```c
// syscalls.c provides the low-level _write() syscall
// Use standard printf once a debugger is connected:
printf("System clock: %lu Hz\r\n", SystemCoreClock);
```

### 2. UART Serial Output

For standalone logging without a debugger, configure a UART peripheral and redirect `printf` or use `HAL_UART_Transmit()`:

```c
// Direct UART output (no printf dependency)
char msg[] = "Init complete\r\n";
HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
```

When UART TX uses `HAL_UART_Transmit_DMA()` in normal mode, enable both the DMA stream IRQ and the matching USART global IRQ. The DMA IRQ ends the memory transfer, but HAL finishes the final byte and calls `HAL_UART_TxCpltCallback()` from the USART transmit-complete interrupt.

### 3. GPIO / LED Debug

Simplest form of "logging" — toggle a GPIO pin to indicate state:

```c
// Toggle LED to indicate alive state in main loop
HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);
HAL_Delay(500);
```

---

## What to Log

| Event | Method | Priority |
|-------|--------|----------|
| System init start/complete | printf / LED | High |
| Clock configuration result | printf (debug) | Medium |
| Peripheral init status | LED / printf | Medium |
| Fault handler entry | GPIO toggle / printf | **Critical** |
| Error_Handler entry | GPIO toggle / printf | **Critical** |
| Sensor readings / state changes | UART / printf (debug) | Low |

---

## What NOT to Log

- In tight ISRs: avoid printf/UART (blocking). Use GPIO toggles only.
- In time-critical loops: logging can cause timing jitter.
- No PII/secrets concerns in embedded context.

---

## Debug Levels (Convention)

When printf is available, use a simple severity prefix:

```c
#define LOG_ERROR(fmt, ...)   printf("[ERR] " fmt "\r\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)    printf("[WRN] " fmt "\r\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)    printf("[INF] " fmt "\r\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)   printf("[DBG] " fmt "\r\n", ##__VA_ARGS__)
```
