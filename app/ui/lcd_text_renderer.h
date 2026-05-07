#ifndef __LCD_TEXT_RENDERER_H__
#define __LCD_TEXT_RENDERER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "display/lcd.h"

void LCD_ShowChinese(u16 x, u16 y, const char *s, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowChinese12x12(u16 x, u16 y, const char *s, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowChinese16x16(u16 x, u16 y, const char *s, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowChinese24x24(u16 x, u16 y, const char *s, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowChinese32x32(u16 x, u16 y, const char *s, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowChar(u16 x, u16 y, u8 num, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowString(u16 x, u16 y, const char *p, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowIntNum(u16 x, u16 y, u16 num, u8 len, u16 fc, u16 bc, u8 sizey);
void LCD_ShowFloatNum1(u16 x, u16 y, float num, u8 len, u16 fc, u16 bc, u8 sizey);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_TEXT_RENDERER_H__ */
