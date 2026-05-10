#include "app_tasks/app_init.h"

#include "app_tasks/app_echo_task.h"
#include "control/pwm_control.h"
#include "control/uart_cmd.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "sensor/adc_sampler.h"
#include "sensor/ina_monitor.h"
#include "sensor/thermocouple_sampler.h"

#include "adc.h"
#include "tim.h"
#include "usart.h"

#include "lvgl.h"

#include <stdio.h>
#include <string.h>

void App_Init(void)
{
  ADC1->CR2 |= ADC_CR2_DDS;

  UartCmd_InitState();

  {
    const char *lines[] = {
      "\r\n========================================\r\n",
      "  STM32F407 PWM/ADC Controller\r\n",
      "  Baud: 115200  8N1   VREF: 3.0V\r\n",
      "  PWM: PB1 = TIM3_CH4 @ 1kHz\r\n",
      "  Cmd: type 0..95 + Enter -> set duty (EG2132 limit)\r\n",
      "========================================\r\n\r\n",
    };
    for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++) {
      HAL_UART_Transmit(&huart1, (uint8_t *)lines[i], strlen(lines[i]), 1000);
    }
  }

  {
    HAL_StatusTypeDef rc = InaMonitor_Init();
    if (rc != HAL_OK) {
      printf("[INA226] init FAILED, rc=%d\r\n", rc);
    } else {
      printf("[INA226] init OK\r\n");
    }
  }

  lv_init();
  lv_delay_set_cb(HAL_Delay);
  lv_port_disp_init();
  lv_port_indev_init();

  HAL_UART_Receive_DMA(&huart1, UartCmd_GetRxBuffer(), UartCmd_GetRxBufferSize());
  HAL_TIM_Base_Start_IT(&htim8);
  PwmControl_Init();
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
  HAL_ADC_Start_DMA(&hadc1,
                    (uint32_t *)AdcSampler_GetDmaBuffer(),
                    AdcSampler_GetDmaSampleCount());

  ThermocoupleSampler_Init();
  {
    HAL_StatusTypeDef rc = ThermocoupleSampler_Start();
    if (rc != HAL_OK) {
      printf("[TC] window sampler start FAILED, rc=%d\r\n", rc);
    } else {
      printf("[TC] window sampler start OK\r\n");
    }
  }
}
