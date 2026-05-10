#include "app_tasks/app_sensor_task.h"

#include "control/pwm_control.h"
#include "sensor/adc_sampler.h"
#include "sensor/ina_monitor.h"
#include "sensor/thermocouple_sampler.h"

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
    thermocouple_snapshot_t tc_snapshot;

    osDelay(10);

    AdcSampler_UpdateFiltered();
    VOFA_Send(AdcSampler_GetVoltageMv());

    if (++tick >= 50) {
      tick = 0;
      InaMonitor_Poll();
      InaMonitor_GetSnapshot(&snapshot);
      ThermocoupleSampler_GetSnapshot(&tc_snapshot);

      if (tc_snapshot.has_sample != 0u) {
        printf("[STAT] ADC=%lumV  TC:raw=%u adc=%lumV tc=%luuV tmp=%ld.%01ldC  INA:V=%lumV I=%ldmA P=%lumW  PWM=%u.%u%%\r\n",
               (unsigned long)AdcSampler_GetVoltageMv(),
               (unsigned)tc_snapshot.raw,
               (unsigned long)tc_snapshot.adc_mv,
               (unsigned long)tc_snapshot.tc_uv,
               (long)(tc_snapshot.temp_c_x10 / 10),
               (long)(tc_snapshot.temp_c_x10 % 10),
               (unsigned long)snapshot.bus_v_mv,
               (long)snapshot.current_ma,
               (unsigned long)snapshot.power_mw,
               (unsigned)(PwmControl_GetDutyX10() / 10u),
               (unsigned)(PwmControl_GetDutyX10() % 10u));
      } else {
        printf("[STAT] ADC=%lumV  TC:pending  INA:V=%lumV I=%ldmA P=%lumW  PWM=%u.%u%%\r\n",
               (unsigned long)AdcSampler_GetVoltageMv(),
               (unsigned long)snapshot.bus_v_mv,
               (long)snapshot.current_ma,
               (unsigned long)snapshot.power_mw,
               (unsigned)(PwmControl_GetDutyX10() / 10u),
               (unsigned)(PwmControl_GetDutyX10() % 10u));
      }
    }
  }
}
