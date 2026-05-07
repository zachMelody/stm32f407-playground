#include "display/lcd.h"
#include "display/lcd_init.h"

#include "spi.h"

static u16 line_buf[240];

void LCD_Fill(u16 xsta, u16 ysta, u16 xend, u16 yend, u16 color)
{
  u16 i;
  u16 w = xend - xsta + 1;
  u16 h = yend - ysta + 1;

  if (w == 0 || h == 0) {
    return;
  }

  LCD_Address_Set(xsta, ysta, xend, yend);

  for (i = 0; i < w; i++) {
    line_buf[i] = color;
  }

  for (i = 0; i < h; i++) {
    LCD_WriteBytes((const u8 *)line_buf, w * 2);
  }
}

void LCD_DrawPoint(u16 x, u16 y, u16 color)
{
  LCD_Address_Set(x, y, x, y);
  LCD_WR_DATA(color);
}

void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
  u16 t;
  int xerr = 0;
  int yerr = 0;
  int delta_x;
  int delta_y;
  int distance;
  int incx;
  int incy;
  int uRow;
  int uCol;

  delta_x = x2 - x1;
  delta_y = y2 - y1;
  uRow = x1;
  uCol = y1;

  if (delta_x > 0) {
    incx = 1;
  } else if (delta_x == 0) {
    incx = 0;
  } else {
    incx = -1;
    delta_x = -delta_x;
  }

  if (delta_y > 0) {
    incy = 1;
  } else if (delta_y == 0) {
    incy = 0;
  } else {
    incy = -1;
    delta_y = -delta_y;
  }

  distance = (delta_x > delta_y) ? delta_x : delta_y;
  for (t = 0; t < distance + 1; t++) {
    LCD_DrawPoint((u16)uRow, (u16)uCol, color);
    xerr += delta_x;
    yerr += delta_y;
    if (xerr > distance) {
      xerr -= distance;
      uRow += incx;
    }
    if (yerr > distance) {
      yerr -= distance;
      uCol += incy;
    }
  }
}

void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
  LCD_DrawLine(x1, y1, x2, y1, color);
  LCD_DrawLine(x1, y1, x1, y2, color);
  LCD_DrawLine(x1, y2, x2, y2, color);
  LCD_DrawLine(x2, y1, x2, y2, color);
}

void Draw_Circle(u16 x0, u16 y0, u8 r, u16 color)
{
  int a = 0;
  int b = r;

  while (a <= b) {
    LCD_DrawPoint(x0 - b, y0 - a, color);
    LCD_DrawPoint(x0 + b, y0 - a, color);
    LCD_DrawPoint(x0 - a, y0 + b, color);
    LCD_DrawPoint(x0 - a, y0 - b, color);
    LCD_DrawPoint(x0 + b, y0 + a, color);
    LCD_DrawPoint(x0 + a, y0 - b, color);
    LCD_DrawPoint(x0 + a, y0 + b, color);
    LCD_DrawPoint(x0 - b, y0 + a, color);
    a++;
    if ((a * a + b * b) > (r * r)) {
      b--;
    }
  }
}

void LCD_ShowPicture(u16 x, u16 y, u16 length, u16 width, const u8 pic[])
{
  LCD_Address_Set(x, y, x + length - 1, y + width - 1);
  LCD_WriteBytes(pic, (u32)length * width * 2);
}
