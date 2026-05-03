# Database Guidelines

> Database patterns and conventions for this project.

---

## Overview

**N/A** — This is an STM32F407 embedded firmware project with no database.

There is no ORM, no SQL database, no file-based storage in use. The MCU has no persistent storage layer beyond raw flash memory (not currently used).

---

## Flash Storage (Future)

If non-volatile storage is needed later, the STM32F407 provides:

- Internal Flash: up to 1 MB (sector-based erase)
- External storage via SPI/I2C (e.g., EEPROM, SD card via SDIO)

Patterns to follow when flash storage is added:

```c
// HAL flash unlock sequence
HAL_FLASH_Unlock();
// ... erase/program operations ...
HAL_FLASH_Lock();
```
