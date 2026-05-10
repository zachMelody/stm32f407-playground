#ifndef __UI_MAIN_SCREEN_H__
#define __UI_MAIN_SCREEN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sensor/ina_monitor.h"
#include "sensor/thermocouple_sampler.h"
#include <stdint.h>

void UiMainScreen_Init(void);
void UiMainScreen_Refresh(uint16_t duty_x10,
                          uint32_t voltage_mv,
                          uint16_t adc_raw,
                          const ina_monitor_snapshot_t *ina,
                          const thermocouple_snapshot_t *tc);

#ifdef __cplusplus
}
#endif

#endif /* __UI_MAIN_SCREEN_H__ */
