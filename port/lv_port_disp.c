/**
 * @file lv_port_disp.c
 * LVGL 9.2 display driver using BSP LCD (ST7789 via SPI1 DMA).
 *
 * 异步 flush 数据流：
 *   1. disp_flush (LVGL 任务上下文)
 *        ├─ 等上一次 DMA 完成（spi1_dma_done）
 *        ├─ 同步发 RAMWR 地址命令（8-bit 轮询）
 *        ├─ rgb565 swap（LVGL 小端 → ST7789 大端）
 *        └─ 启动 8-bit DMA 发像素数据后立即返回（不 flush_ready）
 *   2. DMA2_Stream3 TC 中断 → HAL_SPI_TxCpltCallback
 *        └─ SPI1_TxCplt_Hook() (ISR 上下文)
 *              ├─ 抬 CS
 *              └─ lv_display_flush_ready(disp) → LVGL 继续渲染下一 chunk
 *
 * 由此 LVGL 渲染与 SPI DMA 传输并行，CPU 不再 busy-wait。
 */

#include "lv_port_disp.h"
#include "lcd.h"
#include "lcd_init.h"
#include "spi.h"

#define DISP_HOR_RES    320
#define DISP_VER_RES    240

/* Buffer 大小 = 1/8 屏幕（320*240/8 = 9600 px = 19200 B RGB565）。
 * 双缓冲 ≈ 38.4 KB，位于默认堆。 */
#define DISP_BUF_PIXELS (DISP_HOR_RES * DISP_VER_RES / 8)

extern volatile uint8_t spi1_dma_done;

static lv_display_t * s_disp;

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    uint32_t px_count = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);

    /* 等上一次 DMA 完成（正常情况 LVGL 渲染下一块的时间已足够覆盖 DMA，这里是兜底） */
    while (!spi1_dma_done) {
        /* busy-wait；若以后切到 LV_OS_FREERTOS 可改成信号量 */
    }

    /* 同步发地址窗口 + RAMWR 命令（内部是轮询 SPI，短数据） */
    LCD_Address_Set(area->x1, area->y1, area->x2, area->y2);

    /* LVGL RGB565 小端 → ST7789 大端 */
    lv_draw_sw_rgb565_swap(px_map, px_count);

    /* 异步启动 DMA：完成后 SPI1_TxCplt_Hook 抬 CS + lv_display_flush_ready */
    spi1_dma_done = 0;
    LCD_CS_Clr();
    if (HAL_SPI_Transmit_DMA(&hspi1, px_map, px_count * 2) != HAL_OK) {
        /* 启动失败：恢复状态并同步确认 flush 完成，避免 LVGL 卡死 */
        spi1_dma_done = 1;
        LCD_CS_Set();
        lv_display_flush_ready(disp);
    }
    /* 成功路径：函数立即返回，LVGL 继续渲染下一块；DMA 完成时 ISR 负责 flush_ready */
}

/* SPI1 DMA TX 完成中断 hook（在 HAL_SPI_TxCpltCallback 内、ISR 上下文被调用） */
void SPI1_TxCplt_Hook(void)
{
    LCD_CS_Set();
    if (s_disp != NULL) {
        lv_display_flush_ready(s_disp);
    }
}

void lv_port_disp_init(void)
{
    LCD_Init();

    s_disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_flush_cb(s_disp, disp_flush);

    uint32_t buf_size = DISP_BUF_PIXELS
                        * lv_color_format_get_size(lv_display_get_color_format(s_disp));

    uint8_t * buf1 = lv_malloc(buf_size);
    if (buf1 == NULL) {
        LV_LOG_ERROR("display buffer 1 malloc failed");
        return;
    }
    uint8_t * buf2 = lv_malloc(buf_size);
    if (buf2 == NULL) {
        LV_LOG_ERROR("display buffer 2 malloc failed");
        lv_free(buf1);
        return;
    }
    lv_display_set_buffers(s_disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
}
