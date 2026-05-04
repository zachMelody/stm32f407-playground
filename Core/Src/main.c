/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "BMP.h"
#include "effects.h"
#include "lcd.h"
#include "lcd_init.h"
#include "pic.h"
#include "usbd_cdc_if.h"
#include <math.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* DMA 收发缓冲区 */
#define DMA_BUF_SIZE      64
static volatile uint8_t  rx_buf[DMA_BUF_SIZE];   /* RX 环形缓冲（DMA 后台填充） */
static uint8_t           tx_buf[DMA_BUF_SIZE];   /* TX 线性缓冲（发送前拷贝） */
static uint32_t          rx_last_ndtr;           /* RX NDTR 快照 */
static volatile uint8_t  tx_busy;                /* TX DMA 是否正在发送 */
static volatile uint8_t  btn_pressed;            /* PA0 按键按下标志（ISR 置位） */
static uint8_t           pr_buf[DMA_BUF_SIZE];   /* printf 缓冲 */
static int               pr_buf_cnt;             /* 缓冲区已用字节数 */
static volatile uint8_t  tick_10ms;              /* TIM6 10ms 节拍标志 */

/* 呼吸灯状态机 */
static int      breath_dir = 1;   /* 1=渐亮, 0=渐暗 */
static int      breath_duty;      /* 当前亮度 0~100 */
static int      breath_repeat;    /* 当前步内重复计数 */
static int      breath_repeats;   /* 每步重复次数 */
static int      breath_speed;     /* 速度档位 0/1/2 */
static volatile uint16_t adc_val;    /* ADC DMA 目标（硬件自动刷新） */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

void App_MainLoop(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim6;
extern ADC_HandleTypeDef hadc1;

/* ================================================================
 * 微秒级延时（DWT 硬件周期计数器）
 * ================================================================ */
static void delay_us(uint32_t us)
{
  uint32_t start  = DWT->CYCCNT;
  uint32_t target = us * (SystemCoreClock / 1000000);
  while ((DWT->CYCCNT - start) < target);
}

/* ================================================================
 * printf 重定向到串口 DMA
 * ================================================================ */
int __io_putchar(int ch)
{
  pr_buf[pr_buf_cnt++] = (uint8_t)ch;
  if (pr_buf_cnt >= DMA_BUF_SIZE || ch == '\n')
  {
    while (tx_busy);
    __HAL_DMA_DISABLE(huart1.hdmatx);                 /* 清残留状态 */
    huart1.hdmatx->State = HAL_DMA_STATE_READY;       /* 解锁 DMA 句柄 */
    huart1.gState = HAL_UART_STATE_READY;             /* 解锁 UART 句柄 */
    tx_busy = 1;
    if (HAL_UART_Transmit_DMA(&huart1, pr_buf, pr_buf_cnt) != HAL_OK)
      tx_busy = 0;                                    /* 启动失败则解锁 */
    pr_buf_cnt = 0;
  }
  return ch;
}

/* ================================================================
 * VOFA 数据走 USB CDC（FireWater 协议，每 100ms 一条）
 * ================================================================ */
static void VOFA_Send(uint32_t mv)
{
  extern USBD_HandleTypeDef hUsbDeviceFS;
  /* 跳过未连接/未打开串口的情况，非阻塞发送 */
  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) {
    return;
  }
  char buf[16];
  int len = snprintf(buf, sizeof(buf), "%lu\r\n", mv);
  CDC_Transmit_FS((uint8_t *)buf, len);
}

/* ================================================================
 * 检查 DMA 是否收到了新数据，有则用 DMA 发送回显
 *
 * 数据流：USART → DMA2_Stream5 → rx_buf[环形] → 拷贝 → tx_buf → DMA2_Stream7 → USART
 *           (自动，CPU 不管)          检测 NDTR 变化     一次性 DMA 发送
 *
 * 原理：DMA 每收 1 字节，NDTR 寄存器减 1
 *       对比两次 NDTR 的差值 = 新收到的字节数
 * ================================================================ */
static void DMA_EchoCheck(void)
{
  if (tx_busy) return;  /* 上次 DMA 发送还没结束，等下次循环再检查 */

  uint32_t ndtr  = __HAL_DMA_GET_COUNTER(huart1.hdmarx);
  uint32_t bytes = (rx_last_ndtr - ndtr + DMA_BUF_SIZE) % DMA_BUF_SIZE;

  if (bytes > 0)
  {
    /* 从 RX 环形缓冲区拷贝到 TX 线性缓冲区 */
    uint32_t pos = (DMA_BUF_SIZE - rx_last_ndtr) % DMA_BUF_SIZE;
    for (uint32_t i = 0; i < bytes; i++)
    {
      tx_buf[i] = rx_buf[pos];
      pos = (pos + 1) % DMA_BUF_SIZE;
    }

    rx_last_ndtr = ndtr;
    __HAL_DMA_DISABLE(huart1.hdmatx);
    huart1.hdmatx->State = HAL_DMA_STATE_READY;
    huart1.gState = HAL_UART_STATE_READY;
    tx_busy = 1;
    HAL_UART_Transmit_DMA(&huart1, tx_buf, bytes);
  }
}

/* ================================================================
 * 呼吸灯步进（每次被 TIM6 10ms tick 驱动调用一次）
 * 执行一个完整 PWM 周期（10ms），然后推进状态机一格
 * ================================================================ */
static void BreathingStep(void)
{
  int on_us  = breath_duty * 100;
  int off_us = 10000 - on_us;

  /* 一个 PWM 周期（10ms） */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
  delay_us(on_us);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
  delay_us(off_us);

  /* 推进状态机 */
  if (++breath_repeat >= breath_repeats)
  {
    breath_repeat = 0;
    if (breath_dir)
    {
      if (++breath_duty >= 100) breath_dir = 0;   /* 渐亮 → 渐暗 */
    }
    else
    {
      if (--breath_duty == 0)                      /* 渐暗 → 渐亮，刷新速度 */
      {
        breath_dir = 1;
        breath_repeats = (breath_speed == 0) ? 1 : (breath_speed == 1) ? 2 : 5;
      }
    }
  }
}

/* ================================================================
 * ADC 采集 NTC 温度（每 500ms 调用一次）
 *
 * 硬件：3.0V ── 10kΩ ──┬── PA5 ── NTC ── GND
 *                      │
 *       ADC 参考 = VDDA = 3.3V（与 3V 供电不同！）
 *
 * 计算：
 *   V_pin   = raw × 3.3V / 4095           (ADC 读数 → 引脚电压)
 *   R_ntc   = 10kΩ × V_pin / (3.0V - V_pin)  (分压 → NTC 电阻)
 *   T       = B 参数公式                    (B=3950, R25=10kΩ)
 *
 * ※ 不用 %f（newlib-nano 不支持），全部转成整数打印
 * ================================================================ */
static void ReadADC(void)
{
  uint32_t raw = adc_val;                        /* DMA 一直在搬，读内存即得 */
  uint32_t mv    = raw * 3300 / 4095;               /* ADC 参考 = 3.3V */

  /* 分压：V_pin = 3.0V × R_ntc/(10k+R_ntc) → R_ntc = 10k × V_pin/(3.0V-V_pin) */
  uint32_t r_ntc = (mv < 3000) ? (10000u * mv / (3000 - mv)) : 999999;

  /* B 参数计算温度（内部浮点运算，输出转整数） */
  float  r_f  = (float)r_ntc;
  float  t_k  = 1.0f / (1.0f / 298.15f + logf(r_f / 10000.0f) / 3950.0f);
  float  t_c  = t_k - 273.15f;
  int    ti   = (int)t_c;
  int    td   = (int)((t_c - ti) * 10.0f);
  if (td < 0) td = -td;

  printf("[ADC] raw=%4lu  V=%lumV  R=%luOhm  T=%d.%dC\r\n",
         raw, mv, r_ntc, ti, td);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_TIM6_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  ADC1->CR2 |= ADC_CR2_DDS; /* 每次转换都触发 DMA（CubeMX 没开） */

  printf("\r\n========================================\r\n");
  printf("  STM32F407 USART1 DMA Test\r\n");
  printf("  Baud: 115200  8N1\r\n");
  printf("  Echo mode: type anything\r\n");
  printf("========================================\r\n\r\n");

  /* 初始化 TFT 显示屏 */
  LCD_Init();
  LCD_Fill(0, 0, 240, 240, WHITE);
  LCD_ShowChinese(0, 0, "你好", BLUE, WHITE, 32, 0);
  LCD_ShowString(0, 40, "LCD_W:", RED, WHITE, 16, 0);
  LCD_ShowIntNum(48, 40, LCD_W, 3, RED, WHITE, 16);
  LCD_ShowString(80, 40, "LCD_H:", RED, WHITE, 16, 0);
  LCD_ShowIntNum(128, 40, LCD_H, 3, RED, WHITE, 16);
  LCD_ShowString(0, 70, "Increaseing Nun:", RED, WHITE, 16, 0);
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 6; i++) {
      LCD_ShowPicture(40 * i, 120 + j * 40, 40, 40, gImage_1);
    }
  }

  /* 启动 DMA 循环接收 + 定时器 + ADC */
  rx_last_ndtr = DMA_BUF_SIZE;
  HAL_UART_Receive_DMA(&huart1, (uint8_t *)rx_buf, DMA_BUF_SIZE);
  HAL_TIM_Base_Start_IT(&htim6);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&adc_val, 1); /* DMA 循环搬运 DR */
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* 调度器启动后不会到达这里，应用逻辑在 App_MainLoop() 中 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* TX DMA 发送完成 → 解锁 echo */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) tx_busy = 0;
}

/* PA0 按键中断 → 通知主循环 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_0) btn_pressed = 1;
}

/* TIM6 每 10ms 触发 → 驱动呼吸灯步进 */
/* TIM8 为 FreeRTOS 时基 (1ms)，负责 HAL_IncTick() */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6) {
    tick_10ms = 1;
  }
  if (htim->Instance == TIM8) {
    HAL_IncTick();
  }
}

/* ================================================================
 * App_MainLoop — 原 while(1) 循环体，供 FreeRTOS 任务调用
 * ================================================================ */
void App_MainLoop(void)
{
  DMA_EchoCheck();

  if (btn_pressed)
  {
    btn_pressed = 0;
    breath_speed = (breath_speed + 1) % 3;
    printf("[KEY] speed -> %s (%ds cycle)\r\n",
           (breath_speed == 0) ? "fast" : (breath_speed == 1) ? "medium" : "slow",
           (breath_speed == 0) ? 2 : (breath_speed == 1) ? 4 : 10);
  }

  if (tick_10ms)
  {
    tick_10ms = 0;
    BreathingStep();

    static int adc_tick  = 0;
    static int vofa_tick = 0;

    if (++vofa_tick >= 10) {
      vofa_tick = 0;
      uint32_t mv = adc_val * 3300 / 4095;
      VOFA_Send(mv);
    }

    if (++adc_tick >= 50) {
      adc_tick = 0;
      ReadADC();
    }
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
