#include "ui/lcd_text_renderer.h"

#include "display/lcd_init.h"
#include "font/lcdfont.h"

static u32 mypow(u8 m, u8 n)
{
  u32 result = 1;
  while (n--) {
    result *= m;
  }
  return result;
}

void LCD_ShowChinese(u16 x, u16 y, const char *s, u16 fc, u16 bc, u8 sizey, u8 mode)
{
  while (*s != 0) {
    if (sizey == 12) {
      LCD_ShowChinese12x12(x, y, s, fc, bc, sizey, mode);
    } else if (sizey == 16) {
      LCD_ShowChinese16x16(x, y, s, fc, bc, sizey, mode);
    } else if (sizey == 24) {
      LCD_ShowChinese24x24(x, y, s, fc, bc, sizey, mode);
    } else if (sizey == 32) {
      LCD_ShowChinese32x32(x, y, s, fc, bc, sizey, mode);
    } else {
      return;
    }
    s += 2;
    x += sizey;
  }
}

void LCD_ShowChinese12x12(u16 x, u16 y, const char *s, u16 fc, u16 bc, u8 sizey, u8 mode)
{
  u8 i, j, m = 0;
  u16 k;
  u16 hznum;
  u16 typeface_num;
  u16 x0 = x;

  typeface_num = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
  hznum = sizeof(tfont12) / sizeof(typFNT_GB12);
  for (k = 0; k < hznum; k++) {
    if ((tfont12[k].Index[0] == *(s)) && (tfont12[k].Index[1] == *(s + 1))) {
      LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
      for (i = 0; i < typeface_num; i++) {
        for (j = 0; j < 8; j++) {
          if (!mode) {
            if (tfont12[k].Msk[i] & (0x01 << j)) {
              LCD_WR_DATA(fc);
            } else {
              LCD_WR_DATA(bc);
            }
            m++;
            if (m % sizey == 0) {
              m = 0;
              break;
            }
          } else {
            if (tfont12[k].Msk[i] & (0x01 << j)) {
              LCD_DrawPoint(x, y, fc);
            }
            x++;
            if ((x - x0) == sizey) {
              x = x0;
              y++;
              break;
            }
          }
        }
      }
    }
  }
}

void LCD_ShowChinese16x16(u16 x, u16 y, const char *s, u16 fc, u16 bc, u8 sizey, u8 mode)
{
  u8 i, j, m = 0;
  u16 k;
  u16 hznum;
  u16 typeface_num;
  u16 x0 = x;

  typeface_num = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
  hznum = sizeof(tfont16) / sizeof(typFNT_GB16);
  for (k = 0; k < hznum; k++) {
    if ((tfont16[k].Index[0] == *(s)) && (tfont16[k].Index[1] == *(s + 1))) {
      LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
      for (i = 0; i < typeface_num; i++) {
        for (j = 0; j < 8; j++) {
          if (!mode) {
            if (tfont16[k].Msk[i] & (0x01 << j)) {
              LCD_WR_DATA(fc);
            } else {
              LCD_WR_DATA(bc);
            }
            m++;
            if (m % sizey == 0) {
              m = 0;
              break;
            }
          } else {
            if (tfont16[k].Msk[i] & (0x01 << j)) {
              LCD_DrawPoint(x, y, fc);
            }
            x++;
            if ((x - x0) == sizey) {
              x = x0;
              y++;
              break;
            }
          }
        }
      }
    }
  }
}

void LCD_ShowChinese24x24(u16 x, u16 y, const char *s, u16 fc, u16 bc, u8 sizey, u8 mode)
{
  u8 i, j, m = 0;
  u16 k;
  u16 hznum;
  u16 typeface_num;
  u16 x0 = x;

  typeface_num = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
  hznum = sizeof(tfont24) / sizeof(typFNT_GB24);
  for (k = 0; k < hznum; k++) {
    if ((tfont24[k].Index[0] == *(s)) && (tfont24[k].Index[1] == *(s + 1))) {
      LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
      for (i = 0; i < typeface_num; i++) {
        for (j = 0; j < 8; j++) {
          if (!mode) {
            if (tfont24[k].Msk[i] & (0x01 << j)) {
              LCD_WR_DATA(fc);
            } else {
              LCD_WR_DATA(bc);
            }
            m++;
            if (m % sizey == 0) {
              m = 0;
              break;
            }
          } else {
            if (tfont24[k].Msk[i] & (0x01 << j)) {
              LCD_DrawPoint(x, y, fc);
            }
            x++;
            if ((x - x0) == sizey) {
              x = x0;
              y++;
              break;
            }
          }
        }
      }
    }
  }
}

void LCD_ShowChinese32x32(u16 x, u16 y, const char *s, u16 fc, u16 bc, u8 sizey, u8 mode)
{
  u8 i, j, m = 0;
  u16 k;
  u16 hznum;
  u16 typeface_num;
  u16 x0 = x;

  typeface_num = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
  hznum = sizeof(tfont32) / sizeof(typFNT_GB32);
  for (k = 0; k < hznum; k++) {
    if ((tfont32[k].Index[0] == *(s)) && (tfont32[k].Index[1] == *(s + 1))) {
      LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
      for (i = 0; i < typeface_num; i++) {
        for (j = 0; j < 8; j++) {
          if (!mode) {
            if (tfont32[k].Msk[i] & (0x01 << j)) {
              LCD_WR_DATA(fc);
            } else {
              LCD_WR_DATA(bc);
            }
            m++;
            if (m % sizey == 0) {
              m = 0;
              break;
            }
          } else {
            if (tfont32[k].Msk[i] & (0x01 << j)) {
              LCD_DrawPoint(x, y, fc);
            }
            x++;
            if ((x - x0) == sizey) {
              x = x0;
              y++;
              break;
            }
          }
        }
      }
    }
  }
}

void LCD_ShowChar(u16 x, u16 y, u8 num, u16 fc, u16 bc, u8 sizey, u8 mode)
{
  u8 temp, sizex, t, m = 0;
  u16 i, typeface_num;
  u16 x0 = x;

  sizex = sizey / 2;
  typeface_num = (sizex / 8 + ((sizex % 8) ? 1 : 0)) * sizey;
  num = num - ' ';
  LCD_Address_Set(x, y, x + sizex - 1, y + sizey - 1);
  for (i = 0; i < typeface_num; i++) {
    if (sizey == 12) {
      temp = ascii_1206[num][i];
    } else if (sizey == 16) {
      temp = ascii_1608[num][i];
    } else if (sizey == 24) {
      temp = ascii_2412[num][i];
    } else if (sizey == 32) {
      temp = ascii_3216[num][i];
    } else {
      return;
    }
    for (t = 0; t < 8; t++) {
      if (!mode) {
        if (temp & (0x01 << t)) {
          LCD_WR_DATA(fc);
        } else {
          LCD_WR_DATA(bc);
        }
        m++;
        if (m % sizex == 0) {
          m = 0;
          break;
        }
      } else {
        if (temp & (0x01 << t)) {
          LCD_DrawPoint(x, y, fc);
        }
        x++;
        if ((x - x0) == sizex) {
          x = x0;
          y++;
          break;
        }
      }
    }
  }
}

void LCD_ShowString(u16 x, u16 y, const char *p, u16 fc, u16 bc, u8 sizey, u8 mode)
{
  while (*p != '\0') {
    LCD_ShowChar(x, y, *p, fc, bc, sizey, mode);
    x += sizey / 2;
    p++;
  }
}

void LCD_ShowIntNum(u16 x, u16 y, u16 num, u8 len, u16 fc, u16 bc, u8 sizey)
{
  u8 t, temp;
  u8 enshow = 0;
  u8 sizex = sizey / 2;

  for (t = 0; t < len; t++) {
    temp = (num / mypow(10, len - t - 1)) % 10;
    if (enshow == 0u && t < (len - 1u)) {
      if (temp == 0u) {
        LCD_ShowChar(x + t * sizex, y, ' ', fc, bc, sizey, 0);
        continue;
      }
      enshow = 1u;
    }
    LCD_ShowChar(x + t * sizex, y, temp + 48u, fc, bc, sizey, 0);
  }
}

void LCD_ShowFloatNum1(u16 x, u16 y, float num, u8 len, u16 fc, u16 bc, u8 sizey)
{
  u8 t, temp, sizex;
  u16 num1;

  sizex = sizey / 2;
  num1 = (u16)(num * 100);
  for (t = 0; t < len; t++) {
    temp = (num1 / mypow(10, len - t - 1)) % 10;
    if (t == (len - 2u)) {
      LCD_ShowChar(x + (len - 2u) * sizex, y, '.', fc, bc, sizey, 0);
      t++;
      len += 1u;
    }
    LCD_ShowChar(x + t * sizex, y, temp + 48u, fc, bc, sizey, 0);
  }
}
