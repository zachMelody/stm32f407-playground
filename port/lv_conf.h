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
    #define LV_MEM_SIZE (64 * 1024U)
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
#define LV_USE_SYSMON   0
#define LV_USE_PERF_MONITOR 0

#define LV_FONT_DEFAULT &lv_font_montserrat_14

#endif /* LV_CONF_H */
