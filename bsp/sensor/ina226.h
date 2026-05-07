#ifndef __INA226_H__
#define __INA226_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* I2C 地址（A0=GND, A1=GND） */
#define INA226_ADDR         (0x40u << 1)

/* 寄存器 */
#define INA226_REG_CONFIG   0x00u
#define INA226_REG_SHUNT_V  0x01u
#define INA226_REG_BUS_V    0x02u
#define INA226_REG_POWER    0x03u
#define INA226_REG_CURRENT  0x04u
#define INA226_REG_CALIB    0x05u
#define INA226_REG_MASK     0x06u

/* 默认配置：1 次平均, 1.1ms Vbus/Vsh 转换时间, 连续 shunt+bus 模式 */
#define INA226_CONFIG       0x4527u

/* 校准值：Rshunt=0.01Ω, Current_LSB=100μA, Max=3.2768A */
#define INA226_CALIB        5120u

/* 单位换算常量 */
#define INA226_BUSV_LSB_UV  1250u   /* Bus Voltage LSB = 1.25 mV */
#define INA226_SHUNTV_LSB_NV 2500u  /* Shunt Voltage LSB = 2.5 μV */

typedef struct {
  uint16_t bus_v_raw;      /* Bus Voltage 寄存器原始值 */
  uint16_t shunt_v_raw;    /* Shunt Voltage 寄存器原始值 */
  uint16_t current_raw;    /* Current 寄存器原始值 */
  uint16_t power_raw;      /* Power 寄存器原始值 */
} ina226_data_t;

/**
 * @brief 初始化 INA226（写配置 + 校准寄存器）
 * @param hi2c  I2C 句柄指针
 * @retval HAL_OK / HAL_ERROR / HAL_TIMEOUT
 */
HAL_StatusTypeDef INA226_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief 读取 INA226 全部测量数据（ShuntV, BusV, Current, Power）
 * @param hi2c  I2C 句柄指针
 * @param data  输出数据指针
 * @retval HAL_OK / HAL_ERROR / HAL_TIMEOUT
 */
HAL_StatusTypeDef INA226_ReadAll(I2C_HandleTypeDef *hi2c, ina226_data_t *data);

/**
 * @brief 将原始寄存器值换算为物理量
 * @param data    原始寄存器值
 * @param bus_v_mv  输出：总线电压 (mV)
 * @param current_ma 输出：电流 (mA)
 * @param power_mw   输出：功率 (mW)
 */
void INA226_Convert(const ina226_data_t *data,
                    uint32_t *bus_v_mv,
                    int32_t  *current_ma,
                    uint32_t *power_mw);

#ifdef __cplusplus
}
#endif

#endif /* __INA226_H__ */
