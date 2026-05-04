#include "effects.h"
#include "lcd.h"
#include "lcd_init.h"
#include <stdlib.h>

/*
 * 说明：为提升跨屏兼容度，效果内部全部输出 RGB565。
 * 性能：避免使用浮点；使用整型与简易三角函数近似，适配 F103C8。
 */

static inline u16 hsv_to_rgb565(u16 h, u8 s, u8 v)
{
    /* h: 0..359，s,v: 0..255 */
    u8 region = (u8)(h / 60);
    u16 remainder = (u16)((h % 60) * 255 / 60);
    u8 p = (u8)((v * (255 - s)) / 255);
    u8 q = (u8)((v * (255 - (s * remainder) / 255)) / 255);
    u8 t = (u8)((v * (255 - (s * (255 - remainder)) / 255)) / 255);

    u8 r, g, b;
    switch (region % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }

    u16 R = (u16)(r >> 3);
    u16 G = (u16)(g >> 2);
    u16 B = (u16)(b >> 3);
    return (u16)((R << 11) | (G << 5) | B);
}

/* 近似三角函数：x 单位为角度 0..359，返回 -255..255 */
static inline int16_t sin_fast_deg(u16 x)
{
    /* 分段线性近似：避免查表 */
    x %= 360;
    int16_t y;
    if (x < 90) {
        y = (int16_t)( (x * 255) / 90 );
    } else if (x < 180) {
        y = (int16_t)( 255 - ((x - 90) * 255) / 90 );
    } else if (x < 270) {
        y = (int16_t)( -((x - 180) * 255) / 90 );
    } else {
        y = (int16_t)( -255 + ((x - 270) * 255) / 90 );
    }
    return y;
}

/* 简单正弦查表（0..359），在首次运行时填充，避免每像素分支 */
static int16_t SIN_LUT[360];
static uint8_t SIN_LUT_READY = 0;
static inline int16_t sin_lut(u16 a)
{
    return SIN_LUT[a % 360];
}

static void effect_color_waves(uint32_t t)
{
    /* 纵向彩虹流动（按行批量推送） */
    uint8_t linebuf[LCD_W * 2];
    for (u16 y = 0; y < LCD_H; y++) {
        u16 hue = (u16)((y * 360 / LCD_H + t) % 360);
        u16 color = hsv_to_rgb565(hue, 255, 255);
        for (u16 x = 0; x < LCD_W; x++) {
            linebuf[2*x]   = (uint8_t)(color >> 8);
            linebuf[2*x+1] = (uint8_t)(color & 0xFF);
        }
        LCD_Address_Set(0, y, LCD_W-1, y);
        LCD_WriteBytes(linebuf, LCD_W * 2);
    }
}

static void effect_rotating_gradient(uint32_t t)
{
    /* 以屏幕中心为极坐标的旋转渐变（按行批量推送） */
    int16_t cx = (int16_t)(LCD_W / 2);
    int16_t cy = (int16_t)(LCD_H / 2);
    uint8_t linebuf[LCD_W * 2];
    for (u16 y = 0; y < LCD_H; y++) {
        for (u16 x = 0; x < LCD_W; x++) {
            int16_t dx = (int16_t)x - cx;
            int16_t dy = (int16_t)y - cy;
            u16 angle;
            if (dx == 0 && dy == 0) {
                angle = 0;
            } else if (dx >= 0 && dy >= 0) {
                angle = (u16)( (dy * 90) / (abs(dx) + abs(dy)) );
            } else if (dx < 0 && dy >= 0) {
                angle = (u16)( 90 + (abs(dx) * 90) / (abs(dx) + abs(dy)) );
            } else if (dx < 0 && dy < 0) {
                angle = (u16)( 180 + (abs(dy) * 90) / (abs(dx) + abs(dy)) );
            } else {
                angle = (u16)( 270 + (dx * 90) / (abs(dx) + abs(dy)) );
            }

            u16 hue = (u16)((angle + t) % 360);
            u16 dist = (u16)(abs(dx) + abs(dy));
            u8 v = (u8)(255 - (dist > 255 ? 255 : dist));
            u16 color = hsv_to_rgb565(hue, 200, v);
            linebuf[2*x]   = (uint8_t)(color >> 8);
            linebuf[2*x+1] = (uint8_t)(color & 0xFF);
        }
        LCD_Address_Set(0, y, LCD_W-1, y);
        LCD_WriteBytes(linebuf, LCD_W * 2);
    }
}

static void effect_plasma(uint32_t t)
{
    /* 等离子流动：多波干涉（按行批量推送，定点角度增量） */
    uint8_t linebuf[LCD_W * 2];
    /* 定点角：0..(360<<8)-1 */
    const uint32_t SCALE = 256u;
    const uint32_t FULL = 360u * SCALE;
    const uint32_t incA = FULL / LCD_W;     /* ~1.5度/像素（定点） */
    const uint32_t incB = FULL / LCD_H;     /* ~1.5度/像素（定点） */
    for (u16 y = 0; y < LCD_H; y++) {
        uint32_t bfp = ((uint32_t)y * incB + (uint32_t)(t * SCALE * 2u)) % FULL;
        uint32_t afp = (uint32_t)(t * SCALE) % FULL;
        for (u16 x = 0; x < LCD_W; x++) {
            u16 a = (u16)((afp >> 8) % 360u);
            u16 b = (u16)((bfp >> 8) % 360u);
            int16_t s1 = sin_lut(a);
            int16_t s2 = sin_lut(b);
            u16 cidx = (u16)(((afp + bfp + (uint32_t)(t * SCALE)) % FULL) >> 8);
            int16_t s3 = sin_lut(cidx);
            int16_t vv = (int16_t)((s1 + s2 + s3) / 3 + 255) / 2; /* 0..255 */
            u16 hue = cidx;
            u16 color = hsv_to_rgb565(hue, 255, (u8)vv);
            linebuf[2*x]   = (uint8_t)(color >> 8);
            linebuf[2*x+1] = (uint8_t)(color & 0xFF);
            afp += incA; if (afp >= FULL) afp -= FULL;
        }
        LCD_Address_Set(0, y, LCD_W-1, y);
        LCD_WriteBytes(linebuf, LCD_W * 2);
    }
}

static void effect_starfield(uint32_t t)
{
    /* 星空：整帧生成到缓冲（逐行渲染+写入），避免逐点 IO */
    uint8_t linebuf[LCD_W * 2];
    for (u16 y = 0; y < LCD_H; y++) {
        /* 背景先清黑 */
        for (u16 x = 0; x < LCD_W; x++) {
            linebuf[2*x] = 0x00; linebuf[2*x+1] = 0x00;
        }
        /* 少量星点（基于行与时间的伪随机） */
        for (u8 k = 0; k < 6; k++) {
            uint32_t seed = (uint32_t)(y * 131 + k * 977 + t * 12345u + 0x9E3779B9u);
            u16 x = (u16)(seed % LCD_W);
            u8 tw = (u8)((seed >> 8) & 0xFF);
            u16 color = hsv_to_rgb565(60, 0, (u8)(220 + (tw % 36)));
            linebuf[2*x]   = (uint8_t)(color >> 8);
            linebuf[2*x+1] = (uint8_t)(color & 0xFF);
            if ((tw & 0x3) == 0 && x + 1 < LCD_W) {
                linebuf[2*(x+1)]   = (uint8_t)(color >> 8);
                linebuf[2*(x+1)+1] = (uint8_t)(color & 0xFF);
            }
        }
        LCD_Address_Set(0, y, LCD_W-1, y);
        LCD_WriteBytes(linebuf, LCD_W * 2);
    }
}

void Effects_RunDemo(void)
{
    /* 快速轮换：每种效果渲染少量帧后立刻切换 */
    uint32_t t = 0;
    u8 effect = 0;
    u8 frames_left = 8; /* 每种效果 8 帧，可按需微调 4~12 */

    if (!SIN_LUT_READY) {
        for (u16 i = 0; i < 360; i++) SIN_LUT[i] = sin_fast_deg(i);
        SIN_LUT_READY = 1;
    }

    while (1) {
        switch (effect) {
            case 0:
                effect_color_waves(t);
                t = (t + 3) % 360;
                break;
            case 1: {
                /* 降负载：旋转渐变以 2 像素合并绘制（在内部行写已优化） */
                int16_t cx = (int16_t)(LCD_W / 2);
                int16_t cy = (int16_t)(LCD_H / 2);
                uint8_t linebuf[LCD_W * 2];
                for (u16 y = 0; y < LCD_H; y++) {
                    for (u16 x = 0; x < LCD_W; x += 2) {
                        int16_t dx = (int16_t)x - cx;
                        int16_t dy = (int16_t)y - cy;
                        u16 angle;
                        if (dx == 0 && dy == 0) angle = 0;
                        else if (dx >= 0 && dy >= 0) angle = (u16)((dy * 90) / (abs(dx) + abs(dy)));
                        else if (dx < 0 && dy >= 0)  angle = (u16)(90 + (abs(dx) * 90) / (abs(dx) + abs(dy)));
                        else if (dx < 0 && dy < 0)   angle = (u16)(180 + (abs(dy) * 90) / (abs(dx) + abs(dy)));
                        else                          angle = (u16)(270 + (dx * 90) / (abs(dx) + abs(dy)));
                        u16 hue = (u16)((angle + t) % 360);
                        u16 dist = (u16)(abs(dx) + abs(dy));
                        u8 v = (u8)(255 - (dist > 255 ? 255 : dist));
                        u16 color = hsv_to_rgb565(hue, 200, v);
                        linebuf[2*x]   = (uint8_t)(color >> 8);
                        linebuf[2*x+1] = (uint8_t)(color & 0xFF);
                        if (x + 1 < LCD_W) { linebuf[2*(x+1)] = linebuf[2*x]; linebuf[2*(x+1)+1] = linebuf[2*x+1]; }
                    }
                    LCD_Address_Set(0, y, LCD_W-1, y);
                    LCD_WriteBytes(linebuf, LCD_W * 2);
                }
                t = (t + 5) % 360;
                break;
            }
            case 2:
                effect_plasma(t);
                t = (t + 4) % 360;
                break;
            default:
                effect_starfield(t);
                t = (t + 7) % 360;
                break;
        }

        if (frames_left > 0) {
            frames_left--;
        }
        if (frames_left == 0) {
            effect = (u8)((effect + 1) % 4);
            frames_left = 8;
        }

        /* 可按需降低或移除延时以提升帧率 */
        // HAL_Delay(1);
    }
}


