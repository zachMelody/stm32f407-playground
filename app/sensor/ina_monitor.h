#ifndef __INA_MONITOR_H__
#define __INA_MONITOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

typedef struct {
  uint32_t bus_v_mv;
  int32_t current_ma;
  uint32_t power_mw;
  uint8_t valid;
} ina_monitor_snapshot_t;

HAL_StatusTypeDef InaMonitor_Init(void);
void InaMonitor_Poll(void);
void InaMonitor_GetSnapshot(ina_monitor_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif

#endif /* __INA_MONITOR_H__ */
