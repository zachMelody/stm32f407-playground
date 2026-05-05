#ifndef __LCD_INIT_H
#define __LCD_INIT_H

//#include "sys.h"
#include "main.h"
#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t

#define USE_HORIZONTAL 2  //设置横屏或竖屏显示 0,1为横屏 2,3为竖屏


#define LCD_W 240
#define LCD_H 320

//-----------------LCD端口定义---------------- 

//#define LCD_SCLK_Clr() GPIO_ResetBits(GPIOA,GPIO_Pin_0)//SCL=SCLK
//#define LCD_SCLK_Set() GPIO_SetBits(GPIOA,GPIO_Pin_0)

//#define LCD_MOSI_Clr() GPIO_ResetBits(GPIOA,GPIO_Pin_1)//SDA=MOSI
//#define LCD_MOSI_Set() GPIO_SetBits(GPIOA,GPIO_Pin_1)

//#define LCD_RES_Clr()  GPIO_ResetBits(GPIOA,GPIO_Pin_2)//RES
//#define LCD_RES_Set()  GPIO_SetBits(GPIOA,GPIO_Pin_2)

//#define LCD_DC_Clr()   GPIO_ResetBits(GPIOA,GPIO_Pin_3)//DC
//#define LCD_DC_Set()   GPIO_SetBits(GPIOA,GPIO_Pin_3)

//#define LCD_CS_Clr()  GPIO_ResetBits(GPIOA,GPIO_Pin_4)//CS
//#define LCD_CS_Set()  GPIO_SetBits(GPIOA,GPIO_Pin_4)

//#define LCD_BLK_Clr()  GPIO_ResetBits(GPIOA,GPIO_Pin_5)//BLK
//#define LCD_BLK_Set()  GPIO_SetBits(GPIOA,GPIO_Pin_5)


#define LCD_RES_Clr()  HAL_GPIO_WritePin(TFT_RES_GPIO_Port,TFT_RES_Pin, GPIO_PIN_RESET)//RES
#define LCD_RES_Set()  HAL_GPIO_WritePin(TFT_RES_GPIO_Port,TFT_RES_Pin, GPIO_PIN_SET)
 
#define LCD_DC_Clr()   HAL_GPIO_WritePin(TFT_DC_GPIO_Port,TFT_DC_Pin, GPIO_PIN_RESET)//DC
#define LCD_DC_Set()   HAL_GPIO_WritePin(TFT_DC_GPIO_Port,TFT_DC_Pin, GPIO_PIN_SET)
             
#define LCD_CS_Clr()   HAL_GPIO_WritePin(TFT_CS_GPIO_Port,TFT_CS_Pin, GPIO_PIN_RESET)//CS
#define LCD_CS_Set()   HAL_GPIO_WritePin(TFT_CS_GPIO_Port,TFT_CS_Pin, GPIO_PIN_SET)
 
#define LCD_BLK_Clr()  HAL_GPIO_WritePin(TFT_BL_GPIO_Port,TFT_BL_Pin, GPIO_PIN_RESET)//BLK
#define LCD_BLK_Set()  HAL_GPIO_WritePin(TFT_BL_GPIO_Port,TFT_BL_Pin, GPIO_PIN_SET)

void LCD_GPIO_Init(void);//初始化GPIO
void LCD_Writ_Bus(u8 dat);//模拟SPI时序
void LCD_WR_DATA8(u8 dat);//写入一个字节
void LCD_WR_DATA(u16 dat);//写入两个字节
void LCD_WR_REG(u8 dat);//写入一个指令
void LCD_Address_Set(u16 x1,u16 y1,u16 x2,u16 y2);//设置坐标函数
void LCD_Init(void);//LCD初始化

/* 高速批量写接口（CS 只拉一次，提升带宽） */
void LCD_WriteBytes(const u8 *data, u32 len);
#endif




