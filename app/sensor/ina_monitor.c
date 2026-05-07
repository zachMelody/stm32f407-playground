#include "sensor/ina_monitor.h"

#include "sensor/ina226.h"

#include "i2c.h"

static ina_monitor_snapshot_t s_snapshot;

HAL_StatusTypeDef InaMonitor_Init(void)
{
  HAL_StatusTypeDef rc = INA226_Init(&hi2c1);

  s_snapshot.valid = (rc == HAL_OK) ? 1u : 0u;
  if (rc != HAL_OK) {
    s_snapshot.bus_v_mv = 0u;
    s_snapshot.current_ma = 0;
    s_snapshot.power_mw = 0u;
  }

  return rc;
}

void InaMonitor_Poll(void)
{
  ina226_data_t ina;

  if (INA226_ReadAll(&hi2c1, &ina) == HAL_OK) {
    INA226_Convert(&ina,
                   &s_snapshot.bus_v_mv,
                   &s_snapshot.current_ma,
                   &s_snapshot.power_mw);
    s_snapshot.valid = 1u;
  } else {
    s_snapshot.valid = 0u;
  }
}

void InaMonitor_GetSnapshot(ina_monitor_snapshot_t *snapshot)
{
  if (snapshot != NULL) {
    *snapshot = s_snapshot;
  }
}
