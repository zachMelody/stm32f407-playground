# 添加 INA226 电流监测并在显示屏上显示

## Goal
通过 I2C1 读取 INA226 电流/电压/功率监测芯片数据，并在 LVGL 显示屏上实时显示。

## Requirements
- 启用 I2C1 (PB6=SCL, PB7=SDA)，频率 100kHz (标准模式)
- 创建 INA226 驱动，支持读取总线电压、电流、功率
- 使用默认配置：分流电阻 0.01Ω，最大电流 3.2A
- 在 LVGL 屏幕上增加电流(A)、功率(W)显示
- 保留现有 PWM 占空比和 ADC 电压显示

## Acceptance Criteria
- [ ] 编译 0 错误
- [ ] INA226 初始化成功（I2C 通信正常）
- [ ] LVGL 屏幕正确显示电压(V)、电流(A)、功率(W)
- [ ] I2C 通信异常时不影响其他任务正常运行
- [ ] 数据更新周期 ~500ms

## Technical Notes
- INA226 I2C 地址: 0x40 (A0=GND, A1=GND)
- 分流电阻: 0.01Ω, Current_LSB = 100μA, Calibration = 5120
- 数据流: INA226 → I2C1 阻塞读取 → SensorTask 全局变量 → LVGLTask 刷新
- 遵循项目现有模式：BSP 驱动放在 bsp/，外设初始化放在 Core/
