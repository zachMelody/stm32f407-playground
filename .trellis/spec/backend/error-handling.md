# Error Handling

> How errors are handled in this STM32F407 embedded project.

---

## Overview

Error handling in this embedded project is minimal and follows STM32 HAL conventions. The system uses a centralized `Error_Handler()` function and ARM Cortex-M fault handlers.

---

## Error Types

### HAL Return Codes

All HAL functions return `HAL_StatusTypeDef`:

```c
typedef enum {
    HAL_OK       = 0x00U,
    HAL_ERROR    = 0x01U,
    HAL_BUSY     = 0x02U,
    HAL_TIMEOUT  = 0x03U
} HAL_StatusTypeDef;
```

### Usage Pattern

Check HAL return values and call `Error_Handler()` on failure:

```c
// From Core/Src/main.c:130-133
if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
{
    Error_Handler();
}

// From Core/Src/main.c:144-147
if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
{
    Error_Handler();
}
```

---

## Error_Handler

The central error handler in `Core/Src/main.c`:

```c
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
```

This is a blocking handler — disables all interrupts and loops forever. In production code, users can add LED blink patterns, UART error messages, or reset logic.

---

## ARM Cortex-M Fault Handlers

Defined in `Core/Src/stm32f4xx_it.c`:

| Handler | Purpose |
|---------|---------|
| `HardFault_Handler` | Hard fault (default fault) |
| `MemManage_Handler` | Memory protection violation |
| `BusFault_Handler` | Bus error (prefetch, memory access) |
| `UsageFault_Handler` | Illegal instruction, undefined state |
| `NMI_Handler` | Non-maskable interrupt |

All fault handlers currently trap in infinite `while(1)` loops. This is standard for learning/debugging — in production they should log the fault and attempt recovery or safe shutdown.

---

## Assert Pattern

Optional assert support via `USE_FULL_ASSERT`:

```c
// From Core/Src/main.c:168-183
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    // User can add printf: "Wrong parameters value: file %s on line %d\r\n"
}
#endif
```

---

## Common Mistakes

- **Ignoring HAL return values** — Always check `HAL_StatusTypeDef` from HAL functions. Unchecked errors lead to silent failures.
- **No watchdog** — Currently no IWDG/WWDG is configured. Consider adding watchdog for production reliability.
- **Empty fault handlers** — The default fault handlers trap silently. Add debug output (LED blink, UART log) to diagnose crashes.
