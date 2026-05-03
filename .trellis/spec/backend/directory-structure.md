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
├── Core/
│   ├── Inc/                  # User header files
│   │   ├── main.h            #   Main header, HAL include, Error_Handler prototype
│   │   ├── gpio.h            #   GPIO init prototype
│   │   ├── stm32f4xx_it.h    #   Interrupt handler prototypes
│   │   └── stm32f4xx_hal_conf.h  # HAL module configuration
│   └── Src/                  # User source files
│       ├── main.c            #   Entry point, system clock config
│       ├── gpio.c            #   GPIO initialization (MX_GPIO_Init)
│       ├── stm32f4xx_it.c    #   Interrupt service routines
│       ├── stm32f4xx_hal_msp.c   # HAL MSP (peripheral init callbacks)
│       ├── system_stm32f4xx.c    # System init (startup)
│       ├── sysmem.c          #   Dynamic memory allocator (new/delete)
│       └── syscalls.c        #   Low-level syscalls (_write, _read, etc.)
├── Drivers/
│   ├── STM32F4xx_HAL_Driver/ # STM32 HAL library
│   │   ├── Inc/              #   HAL headers
│   │   └── Src/              #   HAL source
│   └── CMSIS/                # ARM CMSIS (core + device headers)
├── build/                    # CMake build output (generated)
└── .trellis/                 # Trellis workflow system
```

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

---

## Examples

- `Core/Src/gpio.c` + `Core/Inc/gpio.h` — GPIO peripheral configuration (generated from CubeMX)
- `Core/Src/main.c` — Application entry point with system clock config
