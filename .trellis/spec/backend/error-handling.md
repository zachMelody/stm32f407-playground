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

## DMA Transmit Failure: tx_busy Deadlock

> `HAL_UART_Transmit_DMA` / `HAL_SPI_Transmit_DMA` 启动失败时必须清除 busy 标志。

### 1. Scope / Trigger

- Trigger: `HAL_*_Transmit_DMA` 返回非 HAL_OK 时，调用方若未清除 busy 标志，后续所有操作卡死

### 2. Signatures

```c
// 错误案例（main.c DMA_EchoCheck 旧版）
tx_busy = 1;
HAL_UART_Transmit_DMA(&huart1, tx_buf, bytes);  // 失败返回 HAL_ERROR → tx_busy 永久为 1

// 正确案例
tx_busy = 1;
if (HAL_UART_Transmit_DMA(&huart1, tx_buf, bytes) != HAL_OK)
  tx_busy = 0;  // 启动失败则解锁
```

同样模式适用于 `spi1_dma_done`（SPI DMA 场景）。

### 3. Contracts

| Busy Flag | Set By | Cleared By (Normal) | Cleared By (Error) |
|-----------|--------|---------------------|--------------------|
| `tx_busy` | `__io_putchar`, `DMA_EchoCheck` | `HAL_UART_TxCpltCallback` | Caller checks return value |
| `spi1_dma_done` | `LCD_WriteBytes` | `HAL_SPI_TxCpltCallback` | Caller checks return value + timeout |

### 4. Validation & Error Matrix

| Return | Action |
|--------|--------|
| `HAL_OK` | busy 由回调清除 |
| `HAL_ERROR` | caller 置 busy=0 |
| `HAL_BUSY` | caller 置 busy=0 |
| `HAL_TIMEOUT` | caller 置 busy=0 |

### 5. Good/Base/Bad Cases

**Bad** — 不检查返回值:
```c
tx_busy = 1;
HAL_UART_Transmit_DMA(&huart1, buf, len);
// HAL_UART_Transmit_DMA 返回 HAL_ERROR → tx_busy 永远 = 1
// 后续: while(tx_busy) 死等, 所有 printf 卡死, 系统静默
```

**Good** — 检查返回值:
```c
tx_busy = 1;
if (HAL_UART_Transmit_DMA(&huart1, buf, len) != HAL_OK)
  tx_busy = 0;
// 失败时解锁，下次循环重试
```

### 6. Tests Required

- [ ] 构建 0 错误
- [ ] UART 回显持续工作（不卡死在 while(tx_busy)）
- [ ] printf 持续输出（SensorTask ADC 日志不中断）

### 7. Wrong vs Correct

**Wrong**:
```c
// 假设 HAL_*_Transmit_DMA 总会成功
tx_busy = 1;
HAL_UART_Transmit_DMA(handle, data, size);
```

**Correct**:
```c
// 显式处理返回值，失败即解锁
tx_busy = 1;
HAL_StatusTypeDef rc = HAL_UART_Transmit_DMA(handle, data, size);
if (rc != HAL_OK) {
  tx_busy = 0;
  // 可选：记录错误日志
}
```

---

## Common Mistakes

- **Ignoring HAL return values** — Always check `HAL_StatusTypeDef` from HAL functions. Unchecked errors lead to silent failures.
- **DMA busy flag not cleared on error** — See "DMA Transmit Failure: tx_busy Deadlock" above. The `tx_busy` / `spi1_dma_done` pattern is mandatory for all DMA transmit calls.
- **No watchdog** — Currently no IWDG/WWDG is configured. Consider adding watchdog for production reliability.
- **Empty fault handlers** — The default fault handlers trap silently. Add debug output (LED blink, UART log) to diagnose crashes.
