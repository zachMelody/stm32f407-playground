#ifndef __TEMPERATURE_CONTROL_H__
#define __TEMPERATURE_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "sensor/thermocouple_sampler.h"
#include <stdint.h>

#define TEMP_CTRL_SETPOINT_MIN_C_X10 1000
#define TEMP_CTRL_SETPOINT_MAX_C_X10 4500
#define TEMP_CTRL_DUTY_MAX_X10 950u

typedef enum {
  TEMP_CTRL_MODE_OFF = 0,
  TEMP_CTRL_MODE_MANUAL = 1,
  TEMP_CTRL_MODE_AUTO = 2,
} temperature_control_mode_t;

typedef struct {
  uint8_t mode;
  uint8_t has_temperature;
  uint16_t manual_duty_x10;
  uint16_t applied_duty_x10;
  uint32_t last_sample_count;
  uint32_t tc_uv;
  int32_t setpoint_c_x10;
  int32_t measured_temp_c_x10;
  int32_t error_c_x10;
  int32_t integral_term_x10;
} temperature_control_snapshot_t;

void TemperatureControl_Init(void);
void TemperatureControl_SetOff(void);
uint8_t TemperatureControl_SetManualDutyX10(uint16_t duty_x10);
uint8_t TemperatureControl_SetAutoSetpointCX10(int32_t setpoint_c_x10);
void TemperatureControl_UpdateFromSample(const thermocouple_snapshot_t *sample);
void TemperatureControl_GetSnapshot(temperature_control_snapshot_t *snapshot);
const char *TemperatureControl_ModeName(uint8_t mode);

#ifdef __cplusplus
}
#endif

#endif /* __TEMPERATURE_CONTROL_H__ */
