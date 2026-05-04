#include "lcd_init.h"
//#include "delay.h"

#include "spi.h"


void LCD_GPIO_Init(void)
{
//	GPIO_InitTypeDef  GPIO_InitStructure;
// 	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	 //使能A端口时钟
//	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5;	 
// 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 //推挽输出
//	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//速度50MHz
// 	GPIO_Init(GPIOA, &GPIO_InitStructure);	  //初始化GPIOA
// 	GPIO_SetBits(GPIOA,GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5);

}


/******************************************************************************
      函数说明：LCD串行数据写入函数
      传入数据：dat  要写入的寄存器数据
      返回值：  无
******************************************************************************/
void LCD_Writ_Bus(u8 dat) 
{	
//	u8 i;
//	LCD_CS_Clr();
//	for(i=0;i<8;i++)
//	{			  
//		LCD_SCLK_Clr();
//		if(dat&0x80)
//		{
//		   LCD_MOSI_Set();
//		}
//		else
//		{
//		   LCD_MOSI_Clr();
//		}
//		LCD_SCLK_Set();
//		dat<<=1;
//	}	
//	LCD_CS_Set();
LCD_CS_Clr();
HAL_SPI_Transmit(&hspi1,&dat,1, 1000);  
LCD_CS_Set();
}


/******************************************************************************
      函数说明：LCD写入数据8位
      传入数据：dat 写入数据
      返回值：  无
******************************************************************************/
void LCD_WR_DATA8(u8 dat)
{
	LCD_Writ_Bus(dat);
}


/******************************************************************************
      函数说明：LCD写入数据16位
      传入数据：dat 写入数据
      返回值：  无
******************************************************************************/
void LCD_WR_DATA(u16 dat)
{
	LCD_Writ_Bus(dat>>8);
	LCD_Writ_Bus(dat);
}


/******************************************************************************
      函数说明：LCD写入寄存器
      传入数据：dat 写入寄存器
      返回值：  无
******************************************************************************/
void LCD_WR_REG(u8 dat)
{
	LCD_DC_Clr();//写寄存器
	LCD_Writ_Bus(dat);
	LCD_DC_Set();//写数据
}

void LCD_WriteBytes(const u8 *data, u32 len)
{
	if (data == NULL || len == 0) return;
	LCD_CS_Clr();
	HAL_SPI_Transmit(&hspi1, (uint8_t*)data, len, 100000);
	LCD_CS_Set();
}


/******************************************************************************
      函数说明：设置起始和结束显示地址
      传入数据：x1,x2 列的起始和结束显示地址
                y1,y2 行的起始和结束显示地址
      返回值：  无
******************************************************************************/
void LCD_Address_Set(u16 x1,u16 y1,u16 x2,u16 y2)
{
	if(USE_HORIZONTAL==0)
	{
		LCD_WR_REG(0x2a);//列地址设置
		LCD_WR_DATA(x1);
		LCD_WR_DATA(x2);
		LCD_WR_REG(0x2b);//行地址设置
		LCD_WR_DATA(y1);
		LCD_WR_DATA(y2);
		LCD_WR_REG(0x2c);//储存器写
	}
	else if(USE_HORIZONTAL==1)
	{
		LCD_WR_REG(0x2a);//列地址设置
		LCD_WR_DATA(x1);
		LCD_WR_DATA(x2);
		LCD_WR_REG(0x2b);//行地址设置
		LCD_WR_DATA(y1+80);
		LCD_WR_DATA(y2+80);
		LCD_WR_REG(0x2c);//储存器写
	}
	else if(USE_HORIZONTAL==2)
	{
		LCD_WR_REG(0x2a);//列地址设置
		LCD_WR_DATA(x1);
		LCD_WR_DATA(x2);
		LCD_WR_REG(0x2b);//行地址设置
		LCD_WR_DATA(y1);
		LCD_WR_DATA(y2);
		LCD_WR_REG(0x2c);//储存器写
	}
	else
	{
		LCD_WR_REG(0x2a);//列地址设置
		LCD_WR_DATA(x1+80);
		LCD_WR_DATA(x2+80);
		LCD_WR_REG(0x2b);//行地址设置
		LCD_WR_DATA(y1);
		LCD_WR_DATA(y2);
		LCD_WR_REG(0x2c);//储存器写
	}
}

void LCD_Init(void)
{
	LCD_GPIO_Init();//初始化GPIO
	
	LCD_RES_Clr();//复位
	HAL_Delay(100);
	LCD_RES_Set();
	HAL_Delay(100);
	
	LCD_BLK_Set();//打开背光
  HAL_Delay(100);
	
	//************* Start Initial Sequence **********//
	LCD_WR_REG(0x01); //SWRESET
	HAL_Delay(150);
	LCD_WR_REG(0x11); //Sleep out 
	HAL_Delay(120);              //Delay 120ms 
	//************* Start Initial Sequence **********// 
	LCD_WR_REG(0x36);
	if(USE_HORIZONTAL==0)LCD_WR_DATA8(0x00);
	else if(USE_HORIZONTAL==1)LCD_WR_DATA8(0xC0);
	else if(USE_HORIZONTAL==2)LCD_WR_DATA8(0x70);
	else LCD_WR_DATA8(0xA0);

	LCD_WR_REG(0x3A);     
	LCD_WR_DATA8(0x55);   

	LCD_WR_REG(0xB2);     
	LCD_WR_DATA8(0x1F);   
	LCD_WR_DATA8(0x1F);   
	LCD_WR_DATA8(0x00);   
	LCD_WR_DATA8(0x33);   
	LCD_WR_DATA8(0x33);   

	LCD_WR_REG(0xB7);     
	LCD_WR_DATA8(0x35);   

	LCD_WR_REG(0xBB);     
	LCD_WR_DATA8(0x2B);   //2b

	LCD_WR_REG(0xC0);     
	LCD_WR_DATA8(0x2C);   

	LCD_WR_REG(0xC2);     
	LCD_WR_DATA8(0x01);   

	LCD_WR_REG(0xC3);     
	LCD_WR_DATA8(0x0F);   

	LCD_WR_REG(0xC4);     
	LCD_WR_DATA8(0x20);   //VDV, 0x20:0v

	LCD_WR_REG(0xC6);     
	LCD_WR_DATA8(0x13);   //0x13:60Hz   

	LCD_WR_REG(0xD0);     
	LCD_WR_DATA8(0xA4);   
	LCD_WR_DATA8(0xA1);   

	LCD_WR_REG(0xD6);     
	LCD_WR_DATA8(0xA1);   //sleep in时gate电压设为GND

	LCD_WR_REG(0xE0);     
	LCD_WR_DATA8(0xF0);   
	LCD_WR_DATA8(0x04);   
	LCD_WR_DATA8(0x07);   
	LCD_WR_DATA8(0x04);   
	LCD_WR_DATA8(0x04);   
	LCD_WR_DATA8(0x04);   
	LCD_WR_DATA8(0x25);   
	LCD_WR_DATA8(0x33);   
	LCD_WR_DATA8(0x3C);   
	LCD_WR_DATA8(0x36);   
	LCD_WR_DATA8(0x14);   
	LCD_WR_DATA8(0x12);   
	LCD_WR_DATA8(0x29);   
	LCD_WR_DATA8(0x30);   

	LCD_WR_REG(0xE1);     
	LCD_WR_DATA8(0xF0);   
	LCD_WR_DATA8(0x02);   
	LCD_WR_DATA8(0x04);   
	LCD_WR_DATA8(0x05);   
	LCD_WR_DATA8(0x05);   
	LCD_WR_DATA8(0x21);   
	LCD_WR_DATA8(0x25);   
	LCD_WR_DATA8(0x32);   
	LCD_WR_DATA8(0x3B);   
	LCD_WR_DATA8(0x38);   
	LCD_WR_DATA8(0x12);   
	LCD_WR_DATA8(0x14);   
	LCD_WR_DATA8(0x27);   
	LCD_WR_DATA8(0x31);   

	LCD_WR_REG(0xE4);     
	LCD_WR_DATA8(0x1D);   //使用240行gate  (N+1)*8
	LCD_WR_DATA8(0x00);   //设定gate起始位置
	LCD_WR_DATA8(0x00);   //当gate没使用时，bit4(TMG)设为0

	LCD_WR_REG(0x21);     

	LCD_WR_REG(0x29);     
}







