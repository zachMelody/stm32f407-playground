#include "app_tasks/app_sensor_task.h"

#include "control/temperature_control.h"
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
    temperature_control_snapshot_t ctrl_snapshot;
    uint32_t temp_abs_x10;
    uint32_t set_abs_x10;

    osDelay(10);

    AdcSampler_UpdateFiltered();
    VOFA_Send(AdcSampler_GetVoltageMv());
    ThermocoupleSampler_GetSnapshot(&tc_snapshot);
    TemperatureControl_UpdateFromSample(&tc_snapshot);

    if (++tick >= 50) {
      tick = 0;
      InaMonitor_Poll();
      InaMonitor_GetSnapshot(&snapshot);
      ThermocoupleSampler_GetSnapshot(&tc_snapshot);
      TemperatureControl_GetSnapshot(&ctrl_snapshot);
      temp_abs_x10 = (ctrl_snapshot.measured_temp_c_x10 < 0) ? (uint32_t)(-ctrl_snapshot.measured_temp_c_x10) :
                                                                (uint32_t)ctrl_snapshot.measured_temp_c_x10;
      set_abs_x10 = (ctrl_snapshot.setpoint_c_x10 < 0) ? (uint32_t)(-ctrl_snapshot.setpoint_c_x10) :
                                                         (uint32_t)ctrl_snapshot.setpoint_c_x10;

      if (tc_snapshot.has_sample != 0u) {
        printf("[STAT] ADC=%lumV  TC:raw=%u adc=%lumV tc=%luuV tmp=%ld.%01ldC  CTRL:%s set=%s%lu.%01luC out=%u.%u%%  INA:V=%lumV I=%ldmA P=%lumW\r\n",
               (unsigned long)AdcSampler_GetVoltageMv(),
               (unsigned)tc_snapshot.raw,
               (unsigned long)tc_snapshot.adc_mv,
               (unsigned long)tc_snapshot.tc_uv,
               (long)(tc_snapshot.temp_c_x10 / 10),
               (long)(tc_snapshot.temp_c_x10 % 10),
               TemperatureControl_ModeName(ctrl_snapshot.mode),
               (ctrl_snapshot.setpoint_c_x10 < 0) ? "-" : "",
               (unsigned long)(set_abs_x10 / 10u),
               (unsigned long)(set_abs_x10 % 10u),
               (unsigned)(ctrl_snapshot.applied_duty_x10 / 10u),
               (unsigned)(ctrl_snapshot.applied_duty_x10 % 10u),
               (unsigned long)snapshot.bus_v_mv,
               (long)snapshot.current_ma,
               (unsigned long)snapshot.power_mw);
      } else {
        printf("[STAT] ADC=%lumV  TC:pending  CTRL:%s set=%s%lu.%01luC tmp=%s%lu.%01luC out=%u.%u%%  INA:V=%lumV I=%ldmA P=%lumW\r\n",
               (unsigned long)AdcSampler_GetVoltageMv(),
               TemperatureControl_ModeName(ctrl_snapshot.mode),
               (ctrl_snapshot.setpoint_c_x10 < 0) ? "-" : "",
               (unsigned long)(set_abs_x10 / 10u),
               (unsigned long)(set_abs_x10 % 10u),
               (ctrl_snapshot.measured_temp_c_x10 < 0) ? "-" : "",
               (unsigned long)(temp_abs_x10 / 10u),
               (unsigned long)(temp_abs_x10 % 10u),
               (unsigned)(ctrl_snapshot.applied_duty_x10 / 10u),
               (unsigned)(ctrl_snapshot.applied_duty_x10 % 10u),
               (unsigned long)snapshot.bus_v_mv,
               (long)snapshot.current_ma,
               (unsigned long)snapshot.power_mw);
      }
    }
  }
}
