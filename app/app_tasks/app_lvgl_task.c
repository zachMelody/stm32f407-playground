#include "app_tasks/app_lvgl_task.h"

#include "control/pwm_control.h"
#include "sensor/adc_sampler.h"
#include "sensor/ina_monitor.h"
#include "ui/ui_main_screen.h"

#include "cmsis_os.h"
#include "lvgl.h"

#include <stdio.h>

void App_LVGLTask(void *argument)
{
  uint32_t refresh_cnt = 0u;

  (void)argument;

  UiMainScreen_Init();
  printf("[RTOS] LVGLTask started\r\n");

  for (;;) {
    ina_monitor_snapshot_t snapshot;

    lv_timer_handler();

    if (++refresh_cnt >= 10u) {
      refresh_cnt = 0u;
      InaMonitor_GetSnapshot(&snapshot);
      UiMainScreen_Refresh(PwmControl_GetDutyX10(),
                           AdcSampler_GetVoltageMv(),
                           AdcSampler_GetRaw(),
                           &snapshot);
    }

    osDelay(5);
  }
}
