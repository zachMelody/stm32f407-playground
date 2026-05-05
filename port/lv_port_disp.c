/**
 * @file lv_port_disp.c
 * LVGL 9.2 display driver using BSP LCD (ST7789 via SPI1 DMA).
 */

#include "lv_port_disp.h"
#include "lcd.h"
#include "lcd_init.h"
#include "spi.h"

#define DISP_HOR_RES    320
#define DISP_VER_RES    240

extern volatile uint8_t spi1_dma_done;

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    uint32_t px_count = w * h;

    /* LVGL RGB565 在内存中是小端（B 在低字节），ST7789 期望大端（R 在高字节先发）。
     * SPI 是 8-bit DMA，按内存字节序原样发，因此需要在发送前交换每像素的两个字节。
     * 若以后切到 SPI 16-bit DMA 发送，可去掉这行（硬件会自动按大端发）。 */
    lv_draw_sw_rgb565_swap(px_map, px_count);

    LCD_Address_Set(area->x1, area->y1, area->x2, area->y2);
    LCD_WriteBytes(px_map, px_count * 2);
    lv_display_flush_ready(disp);
}

void lv_port_disp_init(void)
{
    LCD_Init();

    lv_display_t * disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush);

    uint32_t buf_size = DISP_HOR_RES * DISP_VER_RES / 10
                        * lv_color_format_get_size(lv_display_get_color_format(disp));

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
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
}
