#include "app_tasks/app_echo_task.h"

#include "control/pwm_control.h"
#include "control/uart_cmd.h"

#include "cmsis_os.h"

#include <stdio.h>

static volatile uint8_t s_btn_pressed;

void App_EchoTask(void *argument)
{
  (void)argument;
  printf("[RTOS] EchoTask started\r\n");

  for (;;) {
    UartCmd_EchoCheck();

    if (s_btn_pressed) {
      uint32_t cycle_seconds;
      const char *speed_name;

      s_btn_pressed = 0u;
      speed_name = PwmControl_CycleBreathSpeed(&cycle_seconds);
      printf("[KEY] speed -> %s (%lus cycle)\r\n",
             speed_name,
             (unsigned long)cycle_seconds);
    }

    osDelay(1);
  }
}

void App_EchoTask_OnButtonPressed(void)
{
  s_btn_pressed = 1u;
}
