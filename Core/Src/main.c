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
#include "lv_demos.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
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
/* 呼吸灯状态机 */
static int      breath_dir = 1;   /* 1=渐亮, 0=渐暗 */
static int      breath_duty;      /* 当前亮度 0~100 */
static int      breath_repeat;    /* 当前步内重复计数 */
static int      breath_repeats;   /* 每步重复次数 */
static int      breath_speed;     /* 速度档位 0/1/2 */
static volatile uint16_t adc_val;    /* ADC DMA 目标（硬件自动刷新） */
static uint8_t  breath_phase;     /* TIM8 ISR: 0=进入ON, 1=进入OFF */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

void App_EchoTask(void *argument);
void App_SensorTask(void *argument);
void App_LVGLTask(void *argument);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim8;
extern ADC_HandleTypeDef hadc1;

/* ================================================================
 * printf 重定向到串口 DMA
 * ================================================================ */
int __io_putchar(int ch)
{
  if (pr_buf_cnt == 0) {
    while (tx_busy);  /* 等上次 DMA 发完再复用 pr_buf */
  }
  pr_buf[pr_buf_cnt++] = (uint8_t)ch;
  if (pr_buf_cnt >= DMA_BUF_SIZE || ch == '\n')
  {
    while (tx_busy);
    __HAL_DMA_DISABLE(huart1.hdmatx);
    huart1.hdmatx->State = HAL_DMA_STATE_READY;
    huart1.gState = HAL_UART_STATE_READY;
    tx_busy = 1;
    if (HAL_UART_Transmit_DMA(&huart1, pr_buf, pr_buf_cnt) != HAL_OK)
      tx_busy = 0;
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
    if (HAL_UART_Transmit_DMA(&huart1, tx_buf, bytes) != HAL_OK)
      tx_busy = 0;
  }
}

/* ================================================================
 * 呼吸灯状态机推进（TIM8 ISR 调用，纯计算，无 busy-wait）
 * ================================================================ */
static void BreathAdvance(void)
{
  if (++breath_repeat >= breath_repeats)
  {
    breath_repeat = 0;
    if (breath_dir)
    {
      if (++breath_duty >= 100) breath_dir = 0;
    }
    else
    {
      if (--breath_duty == 0)
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
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_TIM8_Init();
  /* USER CODE BEGIN 2 */
  ADC1->CR2 |= ADC_CR2_DDS; /* 每次转换都触发 DMA（CubeMX 没开） */

  /* banner 用阻塞发送，绕开 DMA 排查乱码问题 */
  {
    const char *lines[] = {
      "\r\n========================================\r\n",
      "  STM32F407 USART1 DMA Test\r\n",
      "  Baud: 115200  8N1\r\n",
      "  Echo mode: type anything\r\n",
      "========================================\r\n\r\n",
    };
    for (int i = 0; i < 5; i++) {
      HAL_UART_Transmit(&huart1, (uint8_t *)lines[i], strlen(lines[i]), 1000);
    }
  }

  /* 初始化 TFT 显示屏（LVGL tick 未启动前用 HAL_Delay 替代） */
  lv_init();
  lv_delay_set_cb(HAL_Delay);
  lv_port_disp_init();
  lv_port_indev_init();

  /* 启动 DMA 接收 + 呼吸灯 TIM8 + ADC */
  rx_last_ndtr = DMA_BUF_SIZE;
  HAL_UART_Receive_DMA(&huart1, (uint8_t *)rx_buf, DMA_BUF_SIZE);
  HAL_TIM_Base_Start_IT(&htim8);   /* 呼吸灯 one-shot PWM（TIM6 由 HAL_InitTick 管理） */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&adc_val, 1);
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* LVGL task */
  {
    const osThreadAttr_t lvglTask_attributes = {
      .name = "LVGL",
      .stack_size = 1024 * 4,
      .priority = (osPriority_t) osPriorityNormal,
    };
    osThreadNew(App_LVGLTask, NULL, &lvglTask_attributes);
  }

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

/* PA0 按键中断 → 置标志，由 EchoTask 处理 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_0) btn_pressed = 1;
}

/* ================================================================
 * EchoTask — 普通优先级，DMA 回显 + 按键处理
 * ================================================================ */
void App_EchoTask(void *argument)
{
  (void)argument;
  printf("[RTOS] EchoTask started\r\n");

  for (;;) {
    DMA_EchoCheck();

    if (btn_pressed) {
      btn_pressed = 0;
      breath_speed = (breath_speed + 1) % 3;
      printf("[KEY] speed -> %s (%ds cycle)\r\n",
             (breath_speed == 0) ? "fast" : (breath_speed == 1) ? "medium" : "slow",
             (breath_speed == 0) ? 2 : (breath_speed == 1) ? 4 : 10);
    }

    osDelay(1);
  }
}

/* ================================================================
 * SensorTask — 低于普通优先级，ADC 采集 + VOFA 发送
 * ================================================================ */
void App_SensorTask(void *argument)
{
  (void)argument;
  printf("[RTOS] SensorTask started\r\n");

  for (;;) {
    osDelay(100);

    uint32_t mv = adc_val * 3300 / 4095;
    VOFA_Send(mv);

    static int tick = 0;
    if (++tick >= 5) {
      tick = 0;
      ReadADC();
    }
  }
}

void App_LVGLTask(void *argument)
{
  (void)argument;

  /* 运行 LVGL 官方 benchmark：自动跑 ~20 个渲染场景，结束后屏幕显示总表
   * 指标：CPU 使用率 / FPS / Render 时间 / Flush 时间 */
  lv_demo_benchmark();

  printf("[RTOS] LVGLTask started (benchmark)\r\n");

  for (;;) {
    lv_tick_inc(5);
    lv_timer_handler();
    osDelay(5);
  }
}

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  if (htim->Instance == TIM8)
  {
    uint32_t on_us = breath_duty * 100;
    uint32_t period;
    uint8_t  advance = 0;

    if (breath_phase == 0)
    {
      if (on_us > 0) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
        period = on_us;
        breath_phase = 1;
      } else {
        /* duty=0: 保持低电平，整个 10ms 周期不动 */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
        period = 10000;
        advance = 1;
      }
    }
    else
    {
      advance = 1;
      if (on_us < 10000) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
        period = 10000 - on_us;
        breath_phase = 0;
      } else {
        /* duty=100: 保持高电平，整个 10ms 周期不动 */
        period = 10000;
      }
    }

    __HAL_TIM_SET_AUTORELOAD(&htim8, period - 1);
    __HAL_TIM_SET_COUNTER(&htim8, 0);

    if (advance) {
      BreathAdvance();
    }
  }
  /* USER CODE END Callback 1 */
}

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
