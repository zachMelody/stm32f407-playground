# LVGL 9.2 移植到 STM32F407

## Goal
将 LVGL 9.2.2 移植到 STM32F407，驱动 240x240 TFT LCD (ST7789 + SPI1 DMA)，与 FreeRTOS 集成。

## Hardware Context
- MCU: STM32F407 (Cortex-M4, 168MHz, 192KB SRAM)
- Display: 240x240 TFT LCD, ST7789 controller, SPI1
- SPI Tx: DMA2_Stream3, 8-bit / 16-bit auto-switch
- RTOS: FreeRTOS (CMSIS-OS2 wrapper)

## Decisions
- [x] LVGL 9.2.2 放在项目根目录 `lvgl/`
- [x] 保留 BSP 底层（LCD_WriteBytes/LCD_Address_Set/LCD_Init），LVGL 接管所有上层绘制
- [x] 编码器输入框架预留，后续接入
- [x] 双缓冲 1/10 屏（约 9.6KB RGB565）
- [x] 颜色格式: RGB565 (16-bit)
- [x] SPI 8-bit 命令 + 16-bit DMA 数据混合模式
- [x] HAL_SPI_RegisterCallback 回调通知 `lv_display_flush_ready`

## Files Created
- `port/lv_conf.h` — LVGL 配置（48KB 内存池、ST7789 启用、FreeRTOS 手动 tick）
- `port/lv_port_disp.h` — 显示驱动头文件
- `port/lv_port_disp.c` — 显示驱动实现（lv_st7789_create + SPI1 DMA）
- `port/lv_port_indev.h` — 输入设备头文件
- `port/lv_port_indev.c` — 编码器输入框架（占位，后续接入）

## Files Modified
- `CMakeLists.txt` — 添加 lvgl 子目录、port 源码、链接 lvgl 库
- `Core/Src/main.c` — LVGL 初始化 + App_LVGLTask 任务（5ms tick）
- `Core/Inc/stm32f4xx_hal_conf.h` — 启用 USE_HAL_SPI_REGISTER_CALLBACKS

## Build
- [x] cmake --build 通过 (0 错误)
- RAM: 79KB / 128KB (60%)
- Flash: 464KB / 1MB (44%)
