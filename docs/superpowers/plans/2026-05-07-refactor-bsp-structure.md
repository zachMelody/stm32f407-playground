# BSP Structure Refactor Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor the STM32CubeMX firmware tree so board drivers, LVGL porting, app logic, and static assets each live in a stable directory with behavior preserved from the current `Core/Src/main.c` baseline.

**Architecture:** Migrate in four chunks. First move files and fix build wiring. Then extract application logic out of `main.c` into focused `app/` modules while preserving the current ADC/PWM/LVGL behavior. Finally decouple font/resource-aware rendering from the display BSP and update the Trellis spec to reflect the new contracts.

**Tech Stack:** STM32CubeMX, STM32 HAL, CMSIS-RTOS2 / FreeRTOS, LVGL 9.2, CMake, PowerShell, ripgrep

---

## Chunk 1: Normalize File Locations And Build Wiring

### Task 1: Create the target directory skeleton

**Files:**
- Create: `bsp/display/`
- Create: `bsp/sensor/`
- Create: `app/demo/`
- Create: `assets/image/`
- Create: `assets/font/`

- [ ] Create the directories above.
- [ ] Move `bsp/lcd.c`, `bsp/lcd.h`, `bsp/lcd_init.c`, `bsp/lcd_init.h` into `bsp/display/`.
- [ ] Move `bsp/ina226.c`, `bsp/ina226.h` into `bsp/sensor/`.
- [ ] Move `bsp/effects.c`, `bsp/effects.h` into `app/demo/`.
- [ ] Move `bsp/BMP.h`, `bsp/pic.h` into `assets/image/`.
- [ ] Move `bsp/lcdfont.h` into `assets/font/`.
- [ ] Search for stale references with `rg -n "bsp/(lcd|ina226|effects)|BMP.h|pic.h|lcdfont.h" .`.

### Task 2: Rewire CMake and include roots for the new tree

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `port/lv_port_disp.c`
- Modify: `app/demo/effects.c`
- Modify: `bsp/display/lcd.c`
- Modify: `Core/Src/main.c`

- [ ] Update `target_sources()` in `CMakeLists.txt` to point at:
  - `bsp/display/lcd.c`
  - `bsp/display/lcd_init.c`
  - `bsp/sensor/ina226.c`
  - `app/demo/effects.c`
  - existing `port/*.c`
- [ ] Update `target_include_directories()` so include roots are `bsp`, `app`, `assets`, and `port`.
- [ ] Convert includes to rooted paths, for example:

```c
#include "display/lcd.h"
#include "display/lcd_init.h"
#include "sensor/ina226.h"
#include "demo/effects.h"
#include "image/pic.h"
#include "image/BMP.h"
#include "font/lcdfont.h"
```

- [ ] Make `port/lv_port_disp.c` include only `display/lcd.h` and `display/lcd_init.h`.
- [ ] Run `rg -n "#include \"(lcd|lcd_init|ina226|effects|BMP|pic|lcdfont)\\.h\"" Core app bsp port`.
- [ ] Run `cmake --build build/Debug` if the local toolchain is available; otherwise record the exact manual command for the human.

### Task 3: Freeze the behavior baseline before logic extraction

**Files:**
- Inspect: `Core/Src/main.c`

- [ ] Confirm the current uncommitted `Core/Src/main.c` contents are treated as the source of truth for PWM, ADC, INA226, and LVGL behavior.
- [ ] Do not change:
  - `PWM_DUTY_MAX_X10`
  - `ADC_SAMPLE_N`
  - `ADC_TRIM_X`
  - task delays
  - LVGL refresh cadence
  - status text format
- [ ] Record any accidental behavior drift during later chunks as a bug, not as part of the refactor.

---

## Chunk 2: Extract Application Logic Out Of `main.c`

### Task 4: Introduce focused app modules

**Files:**
- Create: `app/control/pwm_control.c`
- Create: `app/control/pwm_control.h`
- Create: `app/control/uart_cmd.c`
- Create: `app/control/uart_cmd.h`
- Create: `app/sensor/adc_sampler.c`
- Create: `app/sensor/adc_sampler.h`
- Create: `app/sensor/ina_monitor.c`
- Create: `app/sensor/ina_monitor.h`
- Create: `app/ui/ui_main_screen.c`
- Create: `app/ui/ui_main_screen.h`
- Create: `app/app_tasks/app_init.c`
- Create: `app/app_tasks/app_init.h`
- Create: `app/app_tasks/app_echo_task.c`
- Create: `app/app_tasks/app_echo_task.h`
- Create: `app/app_tasks/app_sensor_task.c`
- Create: `app/app_tasks/app_sensor_task.h`
- Create: `app/app_tasks/app_lvgl_task.c`
- Create: `app/app_tasks/app_lvgl_task.h`

- [ ] Define narrow headers instead of exporting `main.c` globals.
- [ ] Start with these interfaces:

```c
void App_Init(void);
void App_EchoTask(void *argument);
void App_SensorTask(void *argument);
void App_LVGLTask(void *argument);

void PwmControl_SetDutyX10(uint16_t duty_x10);
void PwmControl_OnTim8Elapsed(void);

void AdcSampler_UpdateFiltered(void);
uint16_t AdcSampler_GetRaw(void);
uint32_t AdcSampler_GetVoltageMv(void);
```

- [ ] Keep state ownership local to each module and expose getters where the UI/task layer needs snapshots.

### Task 5: Move logic with behavior-preserving extraction

**Files:**
- Modify: `Core/Src/main.c`
- Modify: `Core/Inc/main.h`
- Modify: `Core/Src/freertos.c`
- Modify: all newly created `app/*` files

- [ ] Move PWM state and compare-register write logic into `app/control/pwm_control.*`.
- [ ] Move UART line-buffer parsing into `app/control/uart_cmd.*`.
- [ ] Move ADC DMA snapshot and trimmed-mean filter into `app/sensor/adc_sampler.*`.
- [ ] Move INA226 cached values and conversion formatting into `app/sensor/ina_monitor.*`.
- [ ] Move LVGL widget creation and periodic label updates into `app/ui/ui_main_screen.*` and `app/app_tasks/app_lvgl_task.*`.
- [ ] Move `HAL_UART_Receive_DMA`, `HAL_TIM_Base_Start_IT`, `HAL_ADC_Start_DMA`, and `HAL_TIM_PWM_Start` sequencing into `app/app_tasks/app_init.*`.
- [ ] Keep `Core/Src/main.c` responsible only for:
  - CubeMX init order
  - calling `App_Init()`
  - `MX_FREERTOS_Init()`
  - ISR/fault glue
  - `Error_Handler()`
- [ ] Remove `App_*Task` declarations from `Core/Inc/main.h` once `freertos.c` includes the new app task headers directly.
- [ ] Leave TIM8 callback inside `main.c` USER CODE, but reduce it to a thin call into `PwmControl_OnTim8Elapsed()` once extraction is stable.

### Task 6: Rewire RTOS entrypoints to the app layer

**Files:**
- Modify: `Core/Src/freertos.c`
- Modify: `app/app_tasks/app_echo_task.h`
- Modify: `app/app_tasks/app_sensor_task.h`
- Modify: `app/app_tasks/app_lvgl_task.h`

- [ ] Replace `main.h`-based task declarations with app-task headers in `freertos.c`.
- [ ] Keep `StartEchoTask` and `StartSensorTask` wrappers if needed for CubeMX stability.
- [ ] Keep the LVGL task creation in `USER CODE RTOS_THREADS`, but point it at the new app-layer symbol.
- [ ] Verify no business logic remains inside `freertos.c`.

---

## Chunk 3: Remove Asset Coupling From The Display BSP

### Task 7: Split font-dependent text rendering out of `bsp/display/lcd.c`

**Files:**
- Create: `app/ui/lcd_text_renderer.c`
- Create: `app/ui/lcd_text_renderer.h`
- Modify: `bsp/display/lcd.c`
- Modify: `bsp/display/lcd.h`
- Modify: `assets/font/lcdfont.h`
- Modify: any callers of `LCD_ShowChinese*`, string, or number rendering helpers

- [ ] Identify all APIs in `lcd.c` that require `lcdfont.h`.
- [ ] Move those functions into `app/ui/lcd_text_renderer.c`.
- [ ] Remove `#include "font/lcdfont.h"` from `bsp/display/lcd.c`.
- [ ] Leave only generic pixel/primitive drawing in `bsp/display/lcd.c`.
- [ ] If `LCD_ShowPicture` stays in the BSP, keep it generic: caller supplies the pixel buffer, BSP does not own any named asset.
- [ ] Update headers so resource-aware APIs are no longer declared by `bsp/display/lcd.h`.
- [ ] Run `rg -n "lcdfont.h|LCD_ShowChinese|LCD_ShowString|LCD_ShowNum" app bsp Core port`.

### Task 8: Keep demo code in `app/demo` and ensure it only uses public display APIs

**Files:**
- Modify: `app/demo/effects.c`
- Modify: `app/demo/effects.h`

- [ ] Ensure `effects.c` includes only public display headers such as `display/lcd.h` and `display/lcd_init.h`.
- [ ] Confirm demo code does not reach into `assets/` unless it intentionally consumes named resource headers.
- [ ] Confirm demo code is not referenced by `main.c` after the app extraction unless explicitly needed.

---

## Chunk 4: Verification And Spec Synchronization

### Task 9: Verify structural invariants

**Files:**
- Inspect: entire repository

- [ ] Search for old flat includes:

```powershell
rg -n '#include "(lcd|lcd_init|ina226|effects|BMP|pic|lcdfont)\.h"' Core app bsp port
```

- [ ] Search for illegal dependency direction:

```powershell
rg -n "app/" bsp port
rg -n "assets/" Core bsp
```

- [ ] Check `CMakeLists.txt` for stale source paths and stale include roots.
- [ ] Run `cmake --build build/Debug` if available.
- [ ] Record exact failures instead of silently weakening the structure.

### Task 10: Update Trellis spec and task artifacts

**Files:**
- Modify: `.trellis/spec/backend/directory-structure.md`
- Modify: `.trellis/tasks/05-07-refactor-bsp-structure/prd.md` if implementation decisions diverge from the original evolving notes

- [ ] Update the backend directory-structure spec with the new tree and dependency rules.
- [ ] Add one short section explaining why `port -> bsp` is allowed but `bsp -> assets` is not.
- [ ] If the implementation settles on different module names than this plan, sync the task PRD so the task record matches the code.

### Task 11: Prepare handoff and manual validation checklist

**Files:**
- Create or update: local execution notes if needed

- [ ] Provide a manual flash/runtime checklist:
  - LVGL screen boots
  - PWM duty changes via UART still work
  - ADC raw and voltage labels update
  - INA226 voltage/current/power lines update
  - USB CDC VOFA output still appears
- [ ] Stage only refactor-related files.
- [ ] Leave the final commit to the human or the repo-approved workflow.

---

Plan complete and saved to `docs/superpowers/plans/2026-05-07-refactor-bsp-structure.md`. Ready to execute?
