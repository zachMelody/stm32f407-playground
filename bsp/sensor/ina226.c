#include "sensor/ina226.h"

static HAL_StatusTypeDef INA226_ReadReg(I2C_HandleTypeDef *hi2c,
                                        uint8_t reg, uint16_t *val)
{
  uint8_t data[2];
  HAL_StatusTypeDef rc;

  rc = HAL_I2C_Mem_Read(hi2c, INA226_ADDR, reg,
                        I2C_MEMADD_SIZE_8BIT, data, 2, 100);
  if (rc != HAL_OK) return rc;

  *val = ((uint16_t)data[0] << 8) | data[1];
  return HAL_OK;
}

static HAL_StatusTypeDef INA226_WriteReg(I2C_HandleTypeDef *hi2c,
                                         uint8_t reg, uint16_t val)
{
  uint8_t data[2];
  data[0] = (uint8_t)(val >> 8);
  data[1] = (uint8_t)(val & 0xFF);

  return HAL_I2C_Mem_Write(hi2c, INA226_ADDR, reg,
                           I2C_MEMADD_SIZE_8BIT, data, 2, 100);
}

HAL_StatusTypeDef INA226_Init(I2C_HandleTypeDef *hi2c)
{
  HAL_StatusTypeDef rc;

  /* 写配置寄存器 */
  rc = INA226_WriteReg(hi2c, INA226_REG_CONFIG, INA226_CONFIG);
  if (rc != HAL_OK) return rc;

  /* 写校准寄存器 */
  rc = INA226_WriteReg(hi2c, INA226_REG_CALIB, INA226_CALIB);
  return rc;
}

HAL_StatusTypeDef INA226_ReadAll(I2C_HandleTypeDef *hi2c, ina226_data_t *data)
{
  HAL_StatusTypeDef rc;

  rc = INA226_ReadReg(hi2c, INA226_REG_SHUNT_V, &data->shunt_v_raw);
  if (rc != HAL_OK) return rc;
  rc = INA226_ReadReg(hi2c, INA226_REG_BUS_V,   &data->bus_v_raw);
  if (rc != HAL_OK) return rc;
  rc = INA226_ReadReg(hi2c, INA226_REG_CURRENT, &data->current_raw);
  if (rc != HAL_OK) return rc;
  rc = INA226_ReadReg(hi2c, INA226_REG_POWER,   &data->power_raw);
  return rc;
}

void INA226_Convert(const ina226_data_t *d,
                    uint32_t *bus_v_mv,
                    int32_t  *current_ma,
                    uint32_t *power_mw)
{
  /* Bus Voltage: 1.25 mV/LSB */
  *bus_v_mv = (uint32_t)d->bus_v_raw * INA226_BUSV_LSB_UV / 1000u;

  /* Current: Shunt Voltage / Rshunt(0.01Ω), Shunt V LSB = 2.5μV */
  {
    int32_t shunt_uv = (int32_t)((int16_t)d->shunt_v_raw)
                     * (int32_t)INA226_SHUNTV_LSB_NV / 1000;
    /* Rshunt = 0.01Ω → I(A) = Vshunt/Rshunt = Vshunt × 100 */
    int32_t current_a_x1000 = shunt_uv * 100;
    *current_ma = current_a_x1000 / 1000;
  }

  /* Power: 25 × Current_LSB(W/bit), 用 Current 计算更直观 */
  {
    int32_t cur_ma = *current_ma;
    int32_t v_mv   = (int32_t)*bus_v_mv;
    *power_mw = (uint32_t)((cur_ma > 0 ? cur_ma : -cur_ma)
                           * (v_mv > 0 ? v_mv : -v_mv) / 1000);
  }
}
