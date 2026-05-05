# Directory Structure

> How embedded firmware code is organized in this STM32F407 project.

---

## Overview

This is an STM32CubeMX-generated project for STM32F407VGT6 (ARM Cortex-M4). The project structure follows STM32CubeMX conventions with user code placed between `USER CODE BEGIN/END` markers.

Project root: `LED_F407/`

---

## Directory Layout

```
LED_F407/
├── LED_F407.ioc              # STM32CubeMX project configuration
├── STM32F407XX_FLASH.ld      # Linker script (defines RAM / CCMRAM / FLASH regions)
├── CMakeLists.txt            # Top-level build (links stm32cubemx + lvgl + lvgl_demos)
├── Core/
│   ├── Inc/                  # User header files
│   │   ├── main.h            #   Main header, HAL include, Error_Handler prototype
│   │   ├── gpio.h            #   GPIO init prototype
│   │   ├── spi.h             #   SPI prototypes + spi1_dma_done + SPI1_TxCplt_Hook
│   │   ├── stm32f4xx_it.h    #   Interrupt handler prototypes
│   │   └── stm32f4xx_hal_conf.h  # HAL module configuration
│   └── Src/                  # User source files
│       ├── main.c            #   Entry point, system clock config, App_*Task
│       ├── gpio.c            #   GPIO initialization (MX_GPIO_Init)
│       ├── spi.c             #   SPI1 init + HAL_SPI_TxCpltCallback (calls hook)
│       ├── freertos.c        #   FreeRTOS task creation
│       ├── stm32f4xx_it.c    #   Interrupt service routines
│       ├── stm32f4xx_hal_msp.c   # HAL MSP (peripheral init callbacks)
│       ├── system_stm32f4xx.c    # System init (startup)
│       ├── sysmem.c          #   Dynamic memory allocator (new/delete)
│       └── syscalls.c        #   Low-level syscalls (_write, _read, etc.)
├── bsp/                      # Board support package — TFT LCD low-level driver
│   ├── lcd_init.c/h          #   ST7789 init sequence + LCD_Address_Set + LCD_WriteBytes (sync DMA)
│   ├── lcd.c/h               #   Higher-level draw helpers (LCD_Fill, LCD_ShowChar, ...)
│   ├── effects.c/h           #   Demo visual effects (legacy, pre-LVGL)
│   ├── BMP.h / pic.h         #   Static image data
│   └── lcdfont.h             #   GB2312 font tables
├── port/                     # LVGL porting layer (DMA-aware bridge to BSP)
│   ├── lv_conf.h             #   LVGL config (heap location, monitor, demos toggles)
│   ├── lv_port_disp.c/h      #   LVGL display driver (async SPI DMA flush)
│   └── lv_port_indev.c/h     #   LVGL input device (encoder placeholder)
├── lvgl/                     # LVGL 9.2.x source tree (do not modify; configured via lv_conf.h)
├── Drivers/
│   ├── STM32F4xx_HAL_Driver/ # STM32 HAL library
│   └── CMSIS/                # ARM CMSIS (core + device headers)
├── build/                    # CMake build output (generated)
└── .trellis/                 # Trellis workflow system
```

### Memory Region Conventions

STM32F407 has **three** memory regions; usage rules:

| Region | Address | Size | DMA-accessible? | Usage |
|--------|---------|------|----------------|-------|
| **FLASH** | 0x0800_0000 | 1 MB | N/A | Code + const data |
| **SRAM1** (main RAM) | 0x2000_0000 | 128 KB | ✅ Yes | Stacks, default `.bss`/`.data`, **all DMA buffers** |
| **CCMRAM** | 0x1000_0000 | 64 KB | ❌ **No** | CPU-only data: LVGL heap, large lookup tables |

**Critical rule**: any buffer that will be passed to `HAL_*_Transmit_DMA` / `HAL_*_Receive_DMA` **must** live in SRAM1. CCMRAM is on the D-code bus only — DMA controllers cannot reach it; using a CCMRAM address as a DMA source/target results in silent data corruption or `HAL_ERROR`.

**Section attribute** to place data in CCMRAM:

```c
unsigned char lvgl_heap[48 * 1024]
    __attribute__((section(".ccmram"), aligned(4)));
```

The `.ccmram` section is already defined in `STM32F407XX_FLASH.ld`.

---

## Key Conventions

### User Code Sections

All STM32CubeMX-generated files contain `USER CODE BEGIN/END` markers. User code MUST be placed between these markers to survive code regeneration from the `.ioc` file.

```c
/* USER CODE BEGIN Includes */
// Your custom includes here
/* USER CODE END Includes */
```

Common marker types:
- `Includes` — additional #include directives
- `PTD` — private typedef
- `PD` — private define
- `PM` — private macro
- `PV` — private variables
- `PFP` — private function prototypes
- `0`, `1`, `2`, `3`, `4` — user code blocks in main()
- `Init` — extra init code after HAL_Init()
- `SysInit` — extra init code after SystemClock_Config()

### File Naming

- Generated peripheral files: lowercase with underscores (`gpio.c`, `gpio.h`)
- HAL driver files: `stm32f4xx_hal_<module>.c/.h`
- CMSIS headers: standard ARM naming
- Main application: `main.c`, `main.h`

### Module Organization

New features should follow this pattern:

1. If using CubeMX: configure in `.ioc` → regenerate → add user code between markers
2. If manually adding: create new `.c`/`.h` pair in `Core/Src` and `Core/Inc`
3. **BSP-level drivers** (raw register / HAL access for a hardware module): put under `bsp/`
4. **LVGL porting glue** (display flush, input read, lv_conf customizations): put under `port/`
5. Never modify files under `lvgl/` — configure exclusively via `port/lv_conf.h`

---

## Examples

- `Core/Src/gpio.c` + `Core/Inc/gpio.h` — GPIO peripheral configuration (generated from CubeMX)
- `Core/Src/main.c` — Application entry point with system clock config, FreeRTOS task entry points
- `bsp/lcd_init.c` — ST7789 SPI command/init sequence + sync DMA helper `LCD_WriteBytes`
- `port/lv_port_disp.c` — Async SPI DMA flush (16-bit DMA mode, TX-complete callback hook), CCMRAM heap definition
- `port/lv_conf.h` — LVGL feature toggles + `LV_MEM_POOL_ALLOC` redirected to CCMRAM-backed `lvgl_heap[]`
