/**
 * @file lv_port_disp.c
 * LVGL 9.2 display driver using BSP LCD (ST7789 via SPI1 DMA).
 *
 * 性能策略（极致版）：
 *   1. LVGL 堆体 lvgl_heap[] 放 CCMRAM（48 KB），主 RAM 腾给 DMA 可达的 draw buffer
 *   2. draw buffer 静态分配在主 RAM（.bss，4 字节对齐），双缓冲 1/8 屏
 *   3. flush 时 SPI 切 16-bit 模式 + halfword DMA：
 *      - 硬件自动按大端出字节（先高字节后低字节），与 ST7789 一致
 *      - 省掉 lv_draw_sw_rgb565_swap 的 CPU 开销
 *      - DMA 请求次数减半（数据长度单位从 byte 变 halfword）
 *   4. 异步 flush：DMA 启动后立即返回，TC 中断里抬 CS + lv_display_flush_ready
 *      → LVGL 渲染下一块与 SPI 传输并行
 *
 * 数据流：
 *   disp_flush ──┬─ 同步 8-bit：Address Set + RAMWR 命令
 *                ├─ SPI 切 16-bit + 启动 halfword DMA
 *                └─ 立即返回（不 flush_ready）
 *   DMA2_Stream3 TC IRQ → SPI1_TxCplt_Hook:
 *                ├─ SPI 切回 8-bit（BSP 老代码期望 8-bit）
 *                ├─ CS 抬高
 *                └─ lv_display_flush_ready
 *
 * ※ CCMRAM（0x10000000）**DMA 不能访问**！draw buffer 必须在主 RAM。
 */

#include "lv_port_disp.h"
#include "display/lcd.h"
#include "display/lcd_init.h"
#include "spi.h"

#define DISP_HOR_RES    320
#define DISP_VER_RES    240

/* Buffer 大小 = 1/8 屏幕（320*240/8 = 9600 px = 19200 B RGB565）。
 * 放主 RAM（默认 .bss 段），DMA 可直接读。 */
#define DISP_BUF_PIXELS (DISP_HOR_RES * DISP_VER_RES / 8)
#define DISP_BUF_BYTES  (DISP_BUF_PIXELS * 2)

/* LVGL 堆：放 CCMRAM（由 lv_conf.h 的 LV_MEM_POOL_ALLOC 指向）。
 * lv_conf.h 里 extern 声明为 unsigned char，这里定义实体。 */
unsigned char lvgl_heap[48 * 1024] __attribute__((section(".ccmram"), aligned(4)));

/* Draw buffer：双缓冲，4 字节对齐，强制放主 RAM（.bss 默认就是主 RAM） */
static uint8_t s_draw_buf1[DISP_BUF_BYTES] __attribute__((aligned(4)));
static uint8_t s_draw_buf2[DISP_BUF_BYTES] __attribute__((aligned(4)));

extern volatile uint8_t spi1_dma_done;

static lv_display_t * s_disp;

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    uint32_t px_count = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);

    /* 等上一次 DMA 完成（渲染下一块通常 > DMA 时间，这里是兜底） */
    while (!spi1_dma_done) { }

    /* 同步发地址窗口 + RAMWR（SPI 当前是 8-bit 模式，BSP 老代码也期望 8-bit） */
    LCD_Address_Set(area->x1, area->y1, area->x2, area->y2);

    /* 切 SPI 到 16-bit：
     * ① 硬件自动先发高字节（= ST7789 期望的 R5G3 字节），省 rgb565 swap
     * ② DMA 传输单位变 halfword，请求数减半
     * DataSize 直接写寄存器即可（比 HAL_SPI_Init 快几十倍） */
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_DFF, SPI_DATASIZE_16BIT);
    /* DMA 侧也要切成 halfword */
    hspi1.hdmatx->Instance->CR = (hspi1.hdmatx->Instance->CR & ~(DMA_SxCR_PSIZE | DMA_SxCR_MSIZE))
                                 | DMA_PDATAALIGN_HALFWORD
                                 | DMA_MDATAALIGN_HALFWORD;

    spi1_dma_done = 0;
    LCD_CS_Clr();
    /* HAL_SPI_Transmit_DMA 第三参数在 16-bit 模式下单位是 halfword */
    if (HAL_SPI_Transmit_DMA(&hspi1, px_map, (uint16_t)px_count) != HAL_OK) {
        /* 失败回滚：恢复 8-bit + flush_ready，避免 LVGL 卡死 */
        spi1_dma_done = 1;
        LCD_CS_Set();
        __HAL_SPI_DISABLE(&hspi1);
        MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_DFF, SPI_DATASIZE_8BIT);
        hspi1.hdmatx->Instance->CR = (hspi1.hdmatx->Instance->CR & ~(DMA_SxCR_PSIZE | DMA_SxCR_MSIZE))
                                     | DMA_PDATAALIGN_BYTE
                                     | DMA_MDATAALIGN_BYTE;
        __HAL_SPI_ENABLE(&hspi1);
        lv_display_flush_ready(disp);
        return;
    }
    /* DMA 已启动；SPI 保持 16-bit 直到 TC ISR 切回 8-bit */
    (void)disp;
}

/* SPI1 DMA TX 完成中断 hook（ISR 上下文） */
void SPI1_TxCplt_Hook(void)
{
    LCD_CS_Set();

    /* 切回 8-bit（BSP 老代码用 8-bit；下次 flush 开头的 Address_Set 也用 8-bit） */
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_DFF, SPI_DATASIZE_8BIT);
    hspi1.hdmatx->Instance->CR = (hspi1.hdmatx->Instance->CR & ~(DMA_SxCR_PSIZE | DMA_SxCR_MSIZE))
                                 | DMA_PDATAALIGN_BYTE
                                 | DMA_MDATAALIGN_BYTE;
    __HAL_SPI_ENABLE(&hspi1);

    if (s_disp != NULL) {
        lv_display_flush_ready(s_disp);
    }
}

void lv_port_disp_init(void)
{
    LCD_Init();

    s_disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_flush_cb(s_disp, disp_flush);
    lv_display_set_buffers(s_disp, s_draw_buf1, s_draw_buf2, DISP_BUF_BYTES,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}
