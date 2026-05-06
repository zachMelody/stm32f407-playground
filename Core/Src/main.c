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
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "pic.h"
#include "usbd_cdc_if.h"

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

/* ----------------------------------------------------------------
 * EG2132 栅极驱动 PWM (PB1 = TIM3_CH4) + JBC C210 焊咀
 * 由于 EG2132 自举电容（VB-VS）需要低边周期性导通来补充电，
 * 占空比硬上限 95.0%（CCR4 ≤ 950）。
 * ---------------------------------------------------------------- */
#define PWM_DUTY_MAX_X10   950u   /* 95.0% (EG2132 bootstrap limit) */
static volatile uint16_t g_pwm_duty_x10;     /* 0..950 (0.0~95.0%) */
static volatile uint32_t g_voltage_mv;       /* 0..3000 mV (滤波后, VREF=3V) */

/* UART 命令行缓冲（接收 "<int>\r" 或 "<int>\n" 设置占空比） */
#define CMD_LINE_MAX  16
static char     cmd_line[CMD_LINE_MAX];
static uint8_t  cmd_len;

/* ADC 8 点滑动平均环形缓冲 */
#define ADC_AVG_N  8                    /* 必须是 2 的幂 */
static uint16_t adc_ring[ADC_AVG_N];
static uint8_t  adc_ring_idx;
static uint8_t  adc_ring_filled;
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
extern TIM_HandleTypeDef htim3;
extern ADC_HandleTypeDef hadc1;

/* ================================================================
 * PWM (PB1 = TIM3_CH4) — 驱动 EG2132 / JBC C210
 *   占空比单位为 0.1%，输入 0..950 对应 0.0~95.0%
 *   超过 950 的输入会被 clamp（EG2132 自举电容硬上限）。
 * ================================================================ */
static void pwm_set_duty(uint16_t duty_x10)
{
  if (duty_x10 > PWM_DUTY_MAX_X10) duty_x10 = PWM_DUTY_MAX_X10;
  g_pwm_duty_x10 = duty_x10;
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, duty_x10);
}

/* ================================================================
 * UART 命令解析 — 行缓冲式
 *   接受 "<整数>\r" 或 "<整数>\n"，整数 0..95 → 设置占空比
 *   非数字 / 超长 / 越界 → 打印错误，缓冲清零
 * ================================================================ */
static void Cmd_Process(const char *line)
{
  uint32_t val    = 0;
  int      digits = 0;

  for (const char *p = line; *p; p++) {
    if (*p < '0' || *p > '9') {
      printf("[ERR] expect integer 0..95\r\n");
      return;
    }
    val = val * 10u + (uint32_t)(*p - '0');
    if (++digits > 3) {
      printf("[ERR] expect integer 0..95\r\n");
      return;
    }
  }
  if (digits == 0) {
    printf("[ERR] expect integer 0..95\r\n");
    return;
  }
  if (val > 95) {
    printf("[ERR] duty must be 0..95 (EG2132 bootstrap)\r\n");
    return;
  }
  pwm_set_duty((uint16_t)(val * 10u));
  printf("[OK] duty=%lu%%\r\n", (unsigned long)val);
}

/* 将一个 RX 字节喂给行缓冲；遇 \r 或 \n 触发解析 */
static void Cmd_FeedByte(uint8_t ch)
{
  if (ch == '\r' || ch == '\n') {
    if (cmd_len > 0) {
      cmd_line[cmd_len] = '\0';
      Cmd_Process(cmd_line);
      cmd_len = 0;
    }
    return;
  }
  if (cmd_len < CMD_LINE_MAX - 1) {
    cmd_line[cmd_len++] = (char)ch;
  } else {
    /* 溢出：清空缓冲并告警，避免被卡住 */
    cmd_len = 0;
    printf("[ERR] line too long (max %d)\r\n", CMD_LINE_MAX - 1);
  }
}

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
      uint8_t ch = rx_buf[pos];
      tx_buf[i] = ch;
      Cmd_FeedByte(ch);   /* 同步喂给命令解析器 */
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
 * ADC 采集 + 8 点滑动平均，更新 g_voltage_mv
 *
 * 硬件：PC1 → ADC1_IN11，VREF = 3.0 V（外部基准）
 *   V_mv = raw × 3000 / 4095
 *
 * DMA 已在后台连续填充 adc_val（CIRC 模式），调用者只需周期性
 * 触发本函数即可推进滤波环形缓冲。
 * ================================================================ */
static void ADC_UpdateFiltered(void)
{
  /* 推入新样本 */
  adc_ring[adc_ring_idx] = adc_val;
  adc_ring_idx = (adc_ring_idx + 1u) & (ADC_AVG_N - 1u);
  if (adc_ring_idx == 0u) adc_ring_filled = 1u;

  /* 计算窗口内的平均（首轮使用已填充的部分） */
  uint8_t  n   = adc_ring_filled ? ADC_AVG_N : adc_ring_idx;
  uint32_t sum = 0;
  for (uint8_t i = 0; i < n; i++) sum += adc_ring[i];
  uint32_t raw_avg = (n == 0u) ? 0u : (sum / n);

  /* VREF=3V, divider (68k+2.7k)/2.7k = 70700/2700 */
  g_voltage_mv = raw_avg * 3000u / 4095u * 70700u / 2700u;
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
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  ADC1->CR2 |= ADC_CR2_DDS; /* 每次转换都触发 DMA（CubeMX 没开） */

  /* banner 用阻塞发送，绕开 DMA 排查乱码问题 */
  {
    const char *lines[] = {
      "\r\n========================================\r\n",
      "  STM32F407 PWM/ADC Controller\r\n",
      "  Baud: 115200  8N1   VREF: 3.0V\r\n",
      "  PWM: PB1 = TIM3_CH4 @ 1kHz\r\n",
      "  Cmd: type 0..95 + Enter -> set duty (EG2132 limit)\r\n",
      "========================================\r\n\r\n",
    };
    for (int i = 0; i < (int)(sizeof(lines)/sizeof(lines[0])); i++) {
      HAL_UART_Transmit(&huart1, (uint8_t *)lines[i], strlen(lines[i]), 1000);
    }
  }

  /* 初始化 TFT 显示屏（LVGL tick 未启动前用 HAL_Delay 替代） */
  lv_init();
  lv_delay_set_cb(HAL_Delay);
  lv_port_disp_init();
  lv_port_indev_init();

  /* 启动 DMA 接收 + 呼吸灯 TIM8 + ADC + PB1 PWM */
  rx_last_ndtr = DMA_BUF_SIZE;
  HAL_UART_Receive_DMA(&huart1, (uint8_t *)rx_buf, DMA_BUF_SIZE);
  HAL_TIM_Base_Start_IT(&htim8);   /* 呼吸灯 one-shot PWM（TIM6 由 HAL_InitTick 管理） */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&adc_val, 1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);   /* PB1 PWM 启动，初始 CCR4=0 → 0% */
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
 * SensorTask — 低于普通优先级，ADC 采集 + 滤波 + VOFA 推送
 *   每 100 ms 推进一次滤波；每 500 ms 打印一次状态
 * ================================================================ */
void App_SensorTask(void *argument)
{
  (void)argument;
  printf("[RTOS] SensorTask started\r\n");

  for (;;) {
    osDelay(100);

    ADC_UpdateFiltered();
    VOFA_Send(g_voltage_mv);

    static int tick = 0;
    if (++tick >= 5) {
      tick = 0;
      printf("[STAT] V=%lumV  PWM=%u.%u%%\r\n",
             (unsigned long)g_voltage_mv,
             (unsigned)(g_pwm_duty_x10 / 10u),
             (unsigned)(g_pwm_duty_x10 % 10u));
    }
  }
}

/* ================================================================
 * LVGLTask — 周期渲染主界面：
 *   PWM xx.x %  (大字)
 *   [████████░░░░░░░░░]  进度条 (0..1000 → 0..100%)
 *   V xx.xxx V  (大字)
 * 更新频率 ~5 fps，避免抢占 CPU
 * ================================================================ */
void App_LVGLTask(void *argument)
{
  (void)argument;

  /* 主屏：黑底 */
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  /* PWM 数值（大字） */
  lv_obj_t *lbl_pwm = lv_label_create(scr);
  lv_obj_set_style_text_color(lbl_pwm, lv_color_white(), 0);
  lv_obj_set_style_text_font(lbl_pwm, &lv_font_montserrat_28, 0);
  lv_label_set_text(lbl_pwm, "PWM   0.0 %");
  lv_obj_align(lbl_pwm, LV_ALIGN_TOP_MID, 0, 30);

  /* 进度条（PWM 0..1000） */
  lv_obj_t *bar = lv_bar_create(scr);
  lv_obj_set_size(bar, 260, 18);
  lv_bar_set_range(bar, 0, 1000);
  lv_bar_set_value(bar, 0, LV_ANIM_OFF);
  lv_obj_align(bar, LV_ALIGN_CENTER, 0, -10);

  /* 电压数值（大字） */
  lv_obj_t *lbl_v = lv_label_create(scr);
  lv_obj_set_style_text_color(lbl_v, lv_color_white(), 0);
  lv_obj_set_style_text_font(lbl_v, &lv_font_montserrat_28, 0);
  lv_label_set_text(lbl_v, "V    0.000 V");
  lv_obj_align(lbl_v, LV_ALIGN_BOTTOM_MID, 0, -30);

  printf("[RTOS] LVGLTask started\r\n");

  uint32_t refresh_cnt = 0;
  for (;;) {
    lv_timer_handler();

    /* 每 ~200ms (40 × 5ms) 刷新一次显示数据 */
    if (++refresh_cnt >= 40u) {
      refresh_cnt = 0;
      uint16_t duty = g_pwm_duty_x10;        /* 单字读取 */
      uint32_t mv   = g_voltage_mv;          /* 单字读取 */

      lv_label_set_text_fmt(lbl_pwm, "PWM  %u.%u %%",
                            (unsigned)(duty / 10u),
                            (unsigned)(duty % 10u));
      lv_bar_set_value(bar, duty, LV_ANIM_OFF);

      lv_label_set_text_fmt(lbl_v, "V   %u.%03u V",
                            (unsigned)(mv / 1000u),
                            (unsigned)(mv % 1000u));
    }

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
    lv_tick_inc(1);   /* LVGL 时基：1ms 固定步进，独立于 LVGL 任务调度。
                       * 必须独立，否则 lv_timer_get_idle() 永远算成 100% idle，
                       * perf monitor 上 CPU 占用一直显示 0/不显示。 */
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
