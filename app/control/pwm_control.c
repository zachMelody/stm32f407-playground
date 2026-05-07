#include "control/pwm_control.h"

#include "tim.h"

#define PWM_DUTY_MAX_X10 950u

static volatile uint16_t s_pwm_duty_x10;
static int s_breath_dir = 1;
static int s_breath_duty;
static int s_breath_repeat;
static int s_breath_repeats;
static int s_breath_speed;
static uint8_t s_breath_phase;

static void BreathAdvance(void)
{
  if (++s_breath_repeat >= s_breath_repeats) {
    s_breath_repeat = 0;
    if (s_breath_dir) {
      if (++s_breath_duty >= 100) {
        s_breath_dir = 0;
      }
    } else if (--s_breath_duty == 0) {
      s_breath_dir = 1;
      s_breath_repeats = (s_breath_speed == 0) ? 1 : (s_breath_speed == 1) ? 2 : 5;
    }
  }
}

void PwmControl_SetDutyX10(uint16_t duty_x10)
{
  if (duty_x10 > PWM_DUTY_MAX_X10) {
    duty_x10 = PWM_DUTY_MAX_X10;
  }

  s_pwm_duty_x10 = duty_x10;
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, duty_x10);
}

uint16_t PwmControl_GetDutyX10(void)
{
  return s_pwm_duty_x10;
}

const char *PwmControl_CycleBreathSpeed(uint32_t *cycle_seconds)
{
  s_breath_speed = (s_breath_speed + 1) % 3;

  if (cycle_seconds != NULL) {
    *cycle_seconds = (s_breath_speed == 0) ? 2u : (s_breath_speed == 1) ? 4u : 10u;
  }

  return (s_breath_speed == 0) ? "fast" : (s_breath_speed == 1) ? "medium" : "slow";
}

void PwmControl_OnTim8Elapsed(void)
{
  uint32_t on_us = (uint32_t)s_breath_duty * 100u;
  uint32_t period;
  uint8_t advance = 0;

  if (s_breath_phase == 0u) {
    if (on_us > 0u) {
      HAL_GPIO_WritePin(MY_LED_GPIO_Port, MY_LED_Pin, GPIO_PIN_SET);
      period = on_us;
      s_breath_phase = 1u;
    } else {
      HAL_GPIO_WritePin(MY_LED_GPIO_Port, MY_LED_Pin, GPIO_PIN_RESET);
      period = 10000u;
      advance = 1u;
    }
  } else {
    advance = 1u;
    if (on_us < 10000u) {
      HAL_GPIO_WritePin(MY_LED_GPIO_Port, MY_LED_Pin, GPIO_PIN_RESET);
      period = 10000u - on_us;
      s_breath_phase = 0u;
    } else {
      period = 10000u;
    }
  }

  __HAL_TIM_SET_AUTORELOAD(&htim8, period - 1u);
  __HAL_TIM_SET_COUNTER(&htim8, 0u);

  if (advance) {
    BreathAdvance();
  }
}
