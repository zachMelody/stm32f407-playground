# Directory Structure

> Executable structure contract for this STM32F407 firmware project.

---

## Overview

This repository is an STM32CubeMX-generated STM32F407 firmware project that now uses a normalized user-code tree:

- `Core/` keeps CubeMX-generated startup, peripheral init, and USER CODE glue.
- `bsp/` keeps hardware-facing board drivers only.
- `port/` keeps third-party adaptation only.
- `app/` keeps application behavior, tasks, UI composition, and policy.
- `assets/` keeps static image/font data only.

The project root is `LED_F407/`.

This is not a "suggested future structure". It is the structure current code is expected to follow.

---

## Scenario: Normalized Firmware Tree And Build Wiring

### 1. Scope / Trigger

- Trigger: adding or moving firmware code under `Core/`, `bsp/`, `port/`, `app/`, or `assets/`
- Trigger: changing `CMakeLists.txt` source paths or include roots
- Trigger: extracting logic out of `Core/Src/main.c` or `Core/Src/freertos.c`
- Trigger: adding new LCD/UI/sensor modules that might blur driver vs app ownership

This is a cross-layer infra contract because file locations, include paths, and callback ownership affect the entire firmware build and extension model.

### 2. Signatures

#### Build Entry Points

```powershell
cube-cmake --build C:/Users/zhoulv/OneDrive/WorkPlace/stm32/LED_F407/build/Debug
cmake --build build/Debug
ninja -C build/Debug
```

#### Build Stack Contract

- VS Code uses `cube-cmake` from `.vscode/settings.json`
- Preset name is `Debug` in `CMakePresets.json`
- The generated backend is Ninja
- The actual compiler is `arm-none-eabi-gcc.exe` from `stm32cube\bundles\gnu-tools-for-stm32`

This means `cube-cmake --build ...` is a wrapper entrypoint, not a separate compiler toolchain.

#### Include Root Contract

`CMakeLists.txt` exposes these user include roots:

```cmake
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    bsp
    app
    assets
    port
)
```

All user includes must be written relative to those roots, for example:

```c
#include "display/lcd.h"
#include "sensor/ina226.h"
#include "sensor/ina_monitor.h"
#include "app_tasks/app_init.h"
#include "ui/lcd_text_renderer.h"
#include "font/lcdfont.h"
```

#### App Entry / Glue Signatures

```c
void App_Init(void);
void App_EchoTask(void *argument);
void App_SensorTask(void *argument);
void App_LVGLTask(void *argument);

void App_EchoTask_OnButtonPressed(void);
void UartCmd_OnTxComplete(UART_HandleTypeDef *huart);
void PwmControl_OnTim8Elapsed(void);
```

These are the allowed app-facing glue points currently called from `Core/Src/main.c` and `Core/Src/freertos.c`.

### 3. Contracts

#### Current Directory Layout

```text
LED_F407/
├── LED_F407.ioc
├── STM32F407XX_FLASH.ld
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/
│   ├── gcc-arm-none-eabi.cmake
│   └── stm32cubemx/
├── Core/
│   ├── Inc/
│   └── Src/
├── Drivers/
├── Middlewares/
├── USB_DEVICE/
├── bsp/
│   ├── display/
│   │   ├── lcd.c
│   │   ├── lcd.h
│   │   ├── lcd_init.c
│   │   └── lcd_init.h
│   └── sensor/
│       ├── ina226.c
│       └── ina226.h
├── port/
│   ├── lv_conf.h
│   ├── lv_port_disp.c
│   ├── lv_port_disp.h
│   ├── lv_port_indev.c
│   └── lv_port_indev.h
├── app/
│   ├── app_tasks/
│   │   ├── app_echo_task.c
│   │   ├── app_echo_task.h
│   │   ├── app_init.c
│   │   ├── app_init.h
│   │   ├── app_lvgl_task.c
│   │   ├── app_lvgl_task.h
│   │   ├── app_sensor_task.c
│   │   └── app_sensor_task.h
│   ├── control/
│   │   ├── pwm_control.c
│   │   ├── pwm_control.h
│   │   ├── uart_cmd.c
│   │   └── uart_cmd.h
│   ├── demo/
│   │   ├── effects.c
│   │   └── effects.h
│   ├── sensor/
│   │   ├── adc_sampler.c
│   │   ├── adc_sampler.h
│   │   ├── ina_monitor.c
│   │   └── ina_monitor.h
│   └── ui/
│       ├── lcd_text_renderer.c
│       ├── lcd_text_renderer.h
│       ├── ui_main_screen.c
│       └── ui_main_screen.h
├── assets/
│   ├── font/
│   │   └── lcdfont.h
│   └── image/
│       ├── BMP.h
│       └── pic.h
├── lvgl/
├── build/
└── .trellis/
```

#### Ownership Rules

| Directory | Owns | Must Not Own |
|-----------|------|--------------|
| `Core/` | CubeMX-generated init, startup, USER CODE glue, fault handlers | app business logic, resource tables, page composition |
| `bsp/` | hardware-facing drivers, register/HAL access, generic drawing primitives | UI text rendering, app policy, app task logic, named assets |
| `port/` | LVGL adaptation, draw buffer/flush bridge, input-port glue | business logic, page state, app tasks |
| `app/control/` | PWM policy, UART command parsing, application-level control flow | raw peripheral init code |
| `app/sensor/` | ADC filtering, INA snapshot cache, formatted runtime state | raw INA226 register transport |
| `app/ui/` | LVGL page composition, LCD text rendering with font resources | low-level SPI/LCD bus primitives |
| `app/demo/` | demo-only visual routines | raw board-driver internals |
| `assets/` | font and image data tables only | executable logic |

#### Dependency Direction Rules

Allowed:

- `Core -> app`
- `app -> bsp`
- `app -> port`
- `app -> assets`
- `port -> bsp`
- `bsp -> Core/Inc` and HAL/CubeMX generated headers

Forbidden:

- `bsp -> app`
- `port -> app`
- `bsp -> assets`
- `Core -> assets`
- `Core -> demo/resource-specific headers`

#### Main/Freertos Contract

`Core/Src/main.c` is now a thin bootstrap and callback bridge:

- allowed to initialize CubeMX peripherals
- allowed to call `App_Init()`
- allowed to forward HAL callbacks into app helpers
- not allowed to own UART parsing, ADC filtering, INA formatting, or UI composition directly

`Core/Src/freertos.c` may create tasks and keep CubeMX-safe wrappers, but it must not accumulate business logic.

#### BSP Text Rendering Contract

`bsp/display/lcd.c` owns generic drawing primitives only:

- `LCD_Fill`
- `LCD_DrawPoint`
- `LCD_DrawLine`
- `LCD_DrawRectangle`
- `Draw_Circle`
- `LCD_ShowPicture`

Font/resource-aware rendering lives in `app/ui/lcd_text_renderer.c`.

#### Include Path Collision Contract

Because both `bsp` and `app` are include roots, relative include paths are not real namespaces.

Good:

- `bsp/sensor/ina226.h`
- `app/sensor/ina_monitor.h`
- `app/sensor/adc_sampler.h`

Forbidden:

- creating another `app/sensor/ina226.h`
- creating duplicate relative paths like `bsp/control/foo.h` and `app/control/foo.h`

If two headers would resolve to the same rooted include path, one of them must be renamed.

### 4. Validation & Error Matrix

| Check | Expected | Failure Meaning | Action |
|------|----------|-----------------|--------|
| `rg -n '#include "(lcd|lcd_init|ina226|effects|BMP|pic|lcdfont)\.h"' Core app bsp port` | no hits | stale flat include remains | convert to rooted include |
| `rg -n 'app/' bsp port` | no hits | illegal reverse dependency | move logic up to `app/` |
| `rg -n 'assets/' Core bsp` | no hits | resource ownership leaked downward | route through `app/ui` or caller-owned buffers |
| `rg -n '#include "(display|sensor|demo|image|font)/' Core app bsp port` | only allowed roots appear | include tree drift or old names | align with include-root contract |
| `cube-cmake --build ...` or `cmake --build build/Debug` | build succeeds | source path / include root drift | inspect `CMakeLists.txt`, `build.ninja`, `compile_commands.json` |
| VS Code shows `cube-cmake --build ...` | expected | none by itself | treat as wrapper over the same CMake/Ninja graph |

### 5. Good / Base / Bad Cases

**Good** - app UI uses assets and BSP together:

```c
#include "ui/lcd_text_renderer.h"
#include "display/lcd.h"
#include "font/lcdfont.h"
```

**Base** - LVGL port depends on display BSP only:

```c
#include "display/lcd.h"
#include "display/lcd_init.h"
```

**Bad** - BSP reaches up into app or assets:

```c
#include "font/lcdfont.h"
#include "ui/ui_main_screen.h"
```

The first leaks resources into the BSP; the second inverts the dependency direction entirely.

### 6. Tests Required

- [ ] `rg -n '#include "(lcd|lcd_init|ina226|effects|BMP|pic|lcdfont)\.h"' Core app bsp port`
- [ ] `rg -n 'app/' bsp port`
- [ ] `rg -n 'assets/' Core bsp`
- [ ] `rg -n 'App_.*Task|App_Init|PwmControl|UartCmd' Core/Inc/main.h Core/Src/freertos.c Core/Src/main.c`
- [ ] `cmake --build build/Debug` or `cube-cmake --build <repo>/build/Debug`

Manual assertion points after flashing:

- [ ] LVGL screen still boots
- [ ] PWM duty changes via UART still work
- [ ] ADC raw and voltage values still refresh
- [ ] INA226 voltage/current/power values still refresh
- [ ] USB CDC VOFA output still appears

### 7. Wrong vs Correct

**Wrong** - `main.c` owns app details and resource-aware rendering:

```c
#include "font/lcdfont.h"
#include "image/pic.h"

static void BuildUiAndSampleSensorsInline(void) {
  /* mixed app logic inside Core */
}
```

**Correct** - `main.c` stays as bootstrap + callback glue:

```c
#include "app_tasks/app_init.h"
#include "app_tasks/app_echo_task.h"
#include "control/pwm_control.h"
#include "control/uart_cmd.h"

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_TIM8_Init();
  MX_TIM3_Init();
  App_Init();
  osKernelInitialize();
  MX_FREERTOS_Init();
  osKernelStart();
}
```

---

## Memory Region Conventions

STM32F407 has three relevant memory regions:

| Region | Address | Size | DMA-accessible? | Usage |
|--------|---------|------|----------------|-------|
| **FLASH** | `0x0800_0000` | 1 MB | N/A | code and const data |
| **SRAM1** | `0x2000_0000` | 128 KB | Yes | stacks, default `.bss`/`.data`, DMA buffers |
| **CCMRAM** | `0x1000_0000` | 64 KB | No | CPU-only data such as LVGL heap |

Critical rule:

- any buffer passed to `HAL_*_Transmit_DMA` or `HAL_*_Receive_DMA` must live in SRAM1
- CCMRAM must not be used as a DMA source or destination

Example:

```c
unsigned char lvgl_heap[48 * 1024]
    __attribute__((section(".ccmram"), aligned(4)));
```

The `.ccmram` section is defined in `STM32F407XX_FLASH.ld`.

---

## CubeMX USER CODE Rule

All edits inside CubeMX-generated files must stay inside `USER CODE BEGIN/END` markers.

Typical safe areas:

- `Includes`
- `PTD`
- `PD`
- `PM`
- `PV`
- `PFP`
- `0`, `1`, `2`, `3`, `4`
- `Init`
- `SysInit`

If user logic cannot fit safely into those markers, move it out into `app/`, `bsp/`, or `port/` instead of stretching generated files.

---

## Design Decision: Rooted Includes Over Relative File Paths

**Context**: after splitting the firmware into `bsp/`, `app/`, `assets/`, and `port/`, relative includes like `../sensor/foo.h` would make ownership blurry and break easily during moves.

**Options Considered**:

1. include files by repository-relative path such as `bsp/display/lcd.h`
2. include files by rooted path such as `display/lcd.h`
3. allow both forms

**Decision**: use rooted include paths only, with `bsp`, `app`, `assets`, and `port` registered as explicit include roots in `CMakeLists.txt`.

**Why**:

- shorter and more stable include lines
- ownership is visible in the first path segment
- refactors do not cascade `../` rewrites

**Constraint**: avoid duplicate relative paths across include roots.

---

## Examples

- `Core/Src/main.c` - CubeMX bootstrap + callback glue, not business logic
- `Core/Src/freertos.c` - task creation and thin wrappers only
- `bsp/display/lcd_init.c` - ST7789 init sequence and SPI write primitives
- `bsp/display/lcd.c` - generic LCD draw primitives only
- `bsp/sensor/ina226.c` - raw INA226 driver
- `port/lv_port_disp.c` - LVGL display bridge to LCD BSP
- `app/control/pwm_control.c` - PWM duty policy and TIM8 breathing helper
- `app/control/uart_cmd.c` - UART receive/echo command parsing
- `app/sensor/adc_sampler.c` - ADC DMA filtering and voltage conversion
- `app/sensor/ina_monitor.c` - INA226 cached snapshot layer
- `app/ui/ui_main_screen.c` - LVGL page composition and refresh
- `app/ui/lcd_text_renderer.c` - font-backed LCD text rendering
