#include "app_tasks/app_sensor_task.h"

#include "control/pwm_control.h"
#include "sensor/adc_sampler.h"
#include "sensor/ina_monitor.h"

#include "cmsis_os.h"
#include "usbd_cdc_if.h"

#include <stdio.h>

static void VOFA_Send(uint32_t mv)
{
  extern USBD_HandleTypeDef hUsbDeviceFS;
  char buf[16];
  int len;

  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) {
    return;
  }

  len = snprintf(buf, sizeof(buf), "%lu\r\n", (unsigned long)mv);
  CDC_Transmit_FS((uint8_t *)buf, (uint16_t)len);
}

void App_SensorTask(void *argument)
{
  (void)argument;
  printf("[RTOS] SensorTask started\r\n");

  for (;;) {
    static int tick = 0;
    ina_monitor_snapshot_t snapshot;

    osDelay(10);

    AdcSampler_UpdateFiltered();
    VOFA_Send(AdcSampler_GetVoltageMv());

    if (++tick >= 50) {
      tick = 0;
      InaMonitor_Poll();
      InaMonitor_GetSnapshot(&snapshot);

      printf("[STAT] ADC=%lumV  INA:V=%lumV I=%ldmA P=%lumW  PWM=%u.%u%%\r\n",
             (unsigned long)AdcSampler_GetVoltageMv(),
             (unsigned long)snapshot.bus_v_mv,
             (long)snapshot.current_ma,
             (unsigned long)snapshot.power_mw,
             (unsigned)(PwmControl_GetDutyX10() / 10u),
             (unsigned)(PwmControl_GetDutyX10() % 10u));
    }
  }
}
