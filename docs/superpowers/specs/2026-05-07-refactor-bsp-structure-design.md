# BSP Structure Refactor Design

**Date:** 2026-05-07
**Task:** `.trellis/tasks/05-07-refactor-bsp-structure`
**Goal:** Normalize the STM32CubeMX project so `Core/`, `bsp/`, `port/`, `app/`, and `assets/` each own one clear responsibility without changing current firmware behavior.

---

## Context

The current repository has already grown beyond a single LCD demo:

- `Core/Src/main.c` contains UART command parsing, PWM control, ADC filtering, INA226 polling, VOFA output, LVGL screen composition, and task entrypoints.
- `bsp/` mixes board drivers (`ina226.c`), display drivers (`lcd.c`, `lcd_init.c`), demo logic (`effects.c`), and static assets (`BMP.h`, `pic.h`, `lcdfont.h`).
- `port/` is already close to the correct role: LVGL adaptation on top of the display BSP.

This layout still builds, but it no longer gives reliable ownership boundaries for future sensors, screens, input devices, or UI pages.

---

## Design Goals

1. Preserve current runtime behavior as the migration baseline.
2. Keep STM32CubeMX-generated code under `Core/` and user glue inside `USER CODE` blocks.
3. Restrict `bsp/` to hardware-facing drivers only.
4. Restrict `port/` to third-party adaptation only.
5. Create stable homes for application logic and static resources.
6. Make dependency direction visible from file paths and `#include` lines.

---

## Target Directory Layout

```text
LED_F407/
├── Core/
│   ├── Inc/
│   └── Src/
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
│   │   ├── app_sensor_task.c
│   │   ├── app_sensor_task.h
│   │   ├── app_lvgl_task.c
│   │   ├── app_lvgl_task.h
│   │   ├── app_init.c
│   │   └── app_init.h
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
│       ├── ui_main_screen.c
│       ├── ui_main_screen.h
│       ├── lcd_text_renderer.c
│       └── lcd_text_renderer.h
└── assets/
    ├── image/
    │   ├── BMP.h
    │   └── pic.h
    └── font/
        └── lcdfont.h
```

---

## Dependency Rules

Allowed:

- `Core -> app`
- `app -> bsp`
- `app -> port`
- `app -> assets`
- `port -> bsp`
- `bsp -> Core/Inc` and HAL/CubeMX generated modules

Forbidden:

- `bsp -> app`
- `port -> app`
- `bsp -> assets`
- `Core -> assets`
- `Core -> demo/resource-specific headers`

The main consequence is that `Core/Src/main.c` must stop directly including resource and demo headers, and `bsp/display/lcd.c` must stop depending on `lcdfont.h`.

---

## Module Ownership

### `bsp/display`

Owns hardware-facing LCD primitives:

- `LCD_Init`
- `LCD_Address_Set`
- `LCD_WriteBytes`
- `LCD_Fill`
- `LCD_DrawPoint`
- geometry helpers that do not require project-specific assets

### `bsp/sensor`

Owns raw INA226 driver behavior:

- register read/write
- conversion helpers
- no UI formatting and no task scheduling

### `port`

Owns LVGL adaptation only:

- LVGL heap placement
- draw buffers
- flush callback
- input-device adapter

It may depend on `bsp/display`, but it must not own page construction or business state.

### `app/control`

Owns control-plane logic:

- PWM state
- UART command parsing
- clamps and policy decisions tied to the application, not raw peripherals

### `app/sensor`

Owns sampled and formatted runtime sensor state:

- ADC DMA snapshot handling
- trimmed-mean filter
- INA226 polling cache
- VOFA payload creation

### `app/ui`

Owns UI composition and resource-aware rendering:

- LVGL widget creation
- periodic label refresh
- font-backed LCD text helpers

### `app/demo`

Owns demo-only effects such as `Effects_RunDemo`.

### `assets`

Owns static bitmaps and font tables only.

---

## Migration Strategy

### Phase 1: Physical relocation

Move files into `bsp/display`, `bsp/sensor`, `app/demo`, and `assets/*` first, then update CMake and include roots so the tree matches ownership before logic extraction starts.

### Phase 2: Application extraction

Split task logic out of `Core/Src/main.c` into `app/app_tasks`, `app/control`, `app/sensor`, and `app/ui`. Keep `main.c` behavior-identical by turning it into a thin bootstrap and ISR glue layer.

### Phase 3: Resource decoupling

Move font-dependent text rendering out of `bsp/display/lcd.c` into `app/ui/lcd_text_renderer.c`. Keep low-level drawing primitives inside the BSP. `LCD_ShowPicture` can stay in the BSP only if it remains a generic "push caller-owned pixel buffer" API.

### Phase 4: Spec sync and verification

Update `.trellis/spec/backend/directory-structure.md`, then verify old include paths and file locations no longer leak into the tree.

---

## Risk Controls

1. Treat the current uncommitted `Core/Src/main.c` contents as the baseline to preserve.
2. Do not change duty limits, sampling constants, refresh cadence, or text formatting unless required by extraction.
3. Keep TIM8 callback code inside `main.c` as CubeMX-safe glue, but allow it to call a helper in `app/control` once extraction is stable.
4. Prefer narrow headers and getters over exporting mutable globals across modules.
5. Verify stale references with `rg` after every migration chunk.

---

## Validation

Static validation:

- `rg -n "#include .*lcd|ina226|BMP|pic|lcdfont|effects" Core Src app bsp port`
- `rg -n "bsp/" CMakeLists.txt`
- header/implementation pairing and include-root consistency

Build validation:

- `cmake --build build/Debug`

Manual runtime validation after flash:

- LVGL screen still initializes
- PWM duty command path still works
- ADC raw/voltage values still refresh
- INA226 values still appear
- UART and VOFA output still function

---

## Non-Goals

- No new features
- No LVGL redesign
- No HAL/CubeMX directory reshuffle
- No task-model rewrite beyond moving ownership to `app/`
