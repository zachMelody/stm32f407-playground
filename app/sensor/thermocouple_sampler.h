#ifndef __THERMOCOUPLE_SAMPLER_H__
#define __THERMOCOUPLE_SAMPLER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

typedef struct {
  uint16_t raw;
  uint32_t adc_mv;
  uint32_t tc_uv;
  int32_t temp_c_x10;
  uint32_t sample_count;
  uint8_t has_sample;
  uint8_t sampling_active;
} thermocouple_snapshot_t;

void ThermocoupleSampler_Init(void);
HAL_StatusTypeDef ThermocoupleSampler_Start(void);
void ThermocoupleSampler_OnSamplingTimerElapsed(void);
void ThermocoupleSampler_OnSettleTimerElapsed(void);
void ThermocoupleSampler_OnAdcDmaComplete(void);
void ThermocoupleSampler_GetSnapshot(thermocouple_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif

#endif /* __THERMOCOUPLE_SAMPLER_H__ */
