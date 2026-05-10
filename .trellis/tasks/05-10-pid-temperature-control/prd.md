# 引入 PID 调温

## Goal

在当前 `LED_F407` 工程里，基于已经落地的 `ADC2` 热电偶窗口采样链路，引入第一版自动调温闭环。

第一版目标不是一次性做完完整焊台固件，而是把下面几层连起来：

* 串口输入目标温度
* 根据热电偶实时温度计算 heater PWM
* 保留手动 duty 旁路
* 在屏幕和串口日志里可观察当前模式、设定温度、实测温度和输出占空比

## Background

当前仓库已经具备：

* `app/sensor/thermocouple_sampler.{c,h}`
  * 周期性关断加热
  * 稳定窗后启动 `ADC2 DMA`
  * 输出 `raw / adc_mv / tc_uv / temp_c_x10`
* `app/control/pwm_control.{c,h}`
  * 负责实际 heater duty 输出
  * 负责测温窗口期间强制关断和恢复
* `app/control/uart_cmd.c`
  * 当前仍以“输入 0..95 直接设 duty”为主
* `app/ui/ui_main_screen.c`
  * 当前只显示 `TMP / TC uV / PWM / INA`

上一个测温任务明确把“完整 PID 温控闭环”列为 out of scope，因此本任务独立拆出。

## Requirements

* 新增独立控温模块，不把 PID 逻辑塞进 `thermocouple_sampler.c`
* 不破坏当前热电偶窗口采样时序
* 不破坏当前呼吸灯行为，`TIM8` 继续只用于呼吸灯
* 默认支持三种模式：
  * `OFF`
  * `MANUAL`
  * `AUTO`
* 串口支持输入目标温度进入自动控温
* 同时保留手动 duty 旁路，便于调试和回退
* 自动控温第一版允许先使用 `PI`，`D` 项可先保留为零
* 自动控温必须具备：
  * 输出限幅
  * 积分限幅
  * 无有效温度样本时禁止加热
* 运行时至少能观测：
  * 当前模式
  * 设定温度
  * 实测温度
  * PWM 输出
  * 保留现有 `tc_uv`

## Command Model

第一版串口命令采用简单文本命令：

* `auto 300`
  * 切换到自动控温
  * 设定目标温度 `300 C`
* `man 35`
  * 切换到手动模式
  * 固定 duty 为 `35 %`
* `off`
  * 关闭加热输出
* `stat`
  * 打印模式、设定温度、实测温度、PWM

第一版不再把“裸数字输入”解释为 duty，避免和目标温度语义冲突。

## Control Strategy

第一版闭环策略按“最小可工作”落地：

* 控制层新增 `app/control/temperature_control.{c,h}`
* `SensorTask` 在拿到新的热电偶快照后调用控温更新
* 控温层根据最新 `temp_c_x10` 与 `setpoint_c_x10` 的偏差计算目标 duty
* 目标 duty 通过 `PwmControl_SetDutyX10()` 下发

第一版参数策略：

* 控制周期：
  * 直接跟随热电偶新样本更新，不额外新增软件时基
* 控制器形式：
  * 默认 `PI`
* 输出限幅：
  * `0 .. 95 %`
* 积分限幅：
  * 必须限制，防止 windup
* 样本保护：
  * 没有有效温度样本时强制输出 `0`

## Module Boundaries

* `app/control/temperature_control.{c,h}`
  * 模式状态
  * 设定温度
  * 手动 duty
  * PI 计算
  * 状态快照
* `app/control/pwm_control.{c,h}`
  * 继续只负责执行 PWM 输出与测温窗口关断/恢复
* `app/control/uart_cmd.c`
  * 负责命令解析并调用控温层接口
* `app/app_tasks/app_sensor_task.c`
  * 负责周期性触发控温更新
  * 输出调试日志
* `app/ui/ui_main_screen.{c,h}`
  * 显示模式、设定温度、测得温度和 PWM

## UI Changes

屏幕第一版至少增加：

* `MODE AUTO/MAN/OFF`
* `SET xxx.x C`

保留当前：

* `TMP`
* `TC uV`
* `PWM`
* `INA`

## Acceptance Criteria

* [ ] 新任务不破坏当前 `ADC2` 窗口采样
* [ ] 新任务不破坏当前呼吸灯行为
* [ ] 自动模式下可通过串口设置目标温度
* [ ] 手动模式下仍可固定输出 duty
* [ ] `OFF` 模式下 heater 输出为 `0`
* [ ] 没有有效热电偶样本时，自动模式不输出加热
* [ ] 屏幕和串口日志能看到 `mode / setpoint / measured temp / pwm`
* [ ] 新控温逻辑保持在独立模块，不与测温模块混写

## Risks

* 当前温度值仍是基于 AxxSolder 参考映射，不是最终标定真值
* 未经实机整定时，`PI` 参数只能先给保守初值
* 当前窗口采样频率和热惯性决定了第一版响应可能偏慢或过冲

## Out Of Scope

* 最终精确温度标定
* 冷端真实补偿
* 完整焊台状态机（待机、休眠、提手唤醒、boost）
* 自动整定 PID 参数
* 上板温度准确性背书
