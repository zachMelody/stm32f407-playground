/**
 * @file lv_conf.h
 * LVGL 9.2.2 configuration for STM32F407
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 16

/*=========================
   STDLIB WRAPPER SETTINGS
 *=========================*/
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

#define LV_STDINT_INCLUDE       <stdint.h>
#define LV_STDDEF_INCLUDE       <stddef.h>
#define LV_STDBOOL_INCLUDE      <stdbool.h>
#define LV_INTTYPES_INCLUDE     <inttypes.h>
#define LV_LIMITS_INCLUDE       <limits.h>
#define LV_STDARG_INCLUDE       <stdarg.h>

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    /* LVGL 堆放在 CCMRAM（F407 的 CCMRAM 只能被 CPU 访问，不能被 DMA 访问，
     * 因此用于控件/字体/样式等内部对象，主 RAM 就腾出来给 DMA 可达的 draw buffer。
     * 实际堆体 lvgl_heap[] 定义在 port/lv_port_disp.c，放 .ccmram 段。
     *
     * 用 LV_MEM_POOL_ALLOC 方式接入（LV_MEM_ADR 必须是整数常量，不能用 cast 表达式）。 */
    #define LV_MEM_SIZE (48 * 1024U)
    #ifndef __ASSEMBLER__
        extern unsigned char lvgl_heap[];
        #define LV_MEM_POOL_ALLOC(size)  ((void *)lvgl_heap)
    #endif
#endif

/*====================
   HAL SETTINGS
 *====================*/
#define LV_DEF_REFR_PERIOD  33
#define LV_DPI_DEF 130

/*====================
   OPERATING SYSTEM
 *====================*/
#define LV_USE_OS   LV_OS_NONE

/*====================
   RENDERING
 *====================*/
#define LV_DRAW_BUF_ALIGN 4

/*====================
   FEATURE USAGE
 *====================*/
#define LV_USE_DRAW_SW     1
#define LV_USE_DRAW_VG_LITE 0

#define LV_USE_OBSERVER    1

/*====================
   DISPLAY DRIVERS
 *====================*/
#define LV_USE_GENERIC_MIPI 0
#define LV_USE_ST7789       0

/*====================
   LOG
 *====================*/
#define LV_USE_LOG      1
#define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN

/*====================
   OTHERS
 *====================*/
/* 性能监控：右下角显示 FPS / CPU 占用，左下角显示堆使用 / 碎片率。
 * 屏幕文字覆盖会轻微影响帧率测量（固定约 -2fps），调优完成后关闭。 */
#define LV_USE_SYSMON       1
#if LV_USE_SYSMON
    #define LV_SYSMON_GET_IDLE       lv_timer_get_idle
    #define LV_USE_PERF_MONITOR      1
    #define LV_USE_PERF_MONITOR_POS  LV_ALIGN_BOTTOM_RIGHT
    #define LV_USE_PERF_MONITOR_LOG_MODE 0
    #define LV_USE_MEM_MONITOR       1
    #define LV_USE_MEM_MONITOR_POS   LV_ALIGN_BOTTOM_LEFT
#endif

#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
   DEMOS
 *====================*/
/* benchmark 依赖 widgets demo 用到的字体/控件 */
#define LV_USE_DEMO_WIDGETS   1
#define LV_USE_DEMO_BENCHMARK 1

/* benchmark 需要以下字体（不同场景切换字号） */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1

#endif /* LV_CONF_H */
