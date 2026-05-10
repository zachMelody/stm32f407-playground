# 实现 ADC2 热电偶窗口采样

## Goal

在当前 `LED_F407` 工程中，为 JBC210 烙铁头新增一条独立的 `ADC2` 热电偶采样链路。
该链路参考 `AxxSolder` 的窗口采样思路：先关断加热，再等待信号稳定，随后采样并恢复加热。

本任务的目标不是一次性完成完整温控系统，而是先把“可运行、可观测、边界清楚”的温度测量基础打通。

## Confirmed Inputs

* 热电偶前端为 `OPA2387`
* 供电为 `3V3/GND`
* 两级放大：
  * 一级增益：`100 / 2.7`
  * 二级增益：`30 / 10 + 1`
* 总增益约：`148.148`
* 当前 MCU 侧热电偶输入通道为 `ADC2 / PC4 / ADC_CHANNEL_14`

## Reference Design

参考 `.research/AxxSolder` 已确认的实现要点：

* 热电偶和加热器共路径时，测温前要先停止加热
* 停止加热后保留约 `0.5 ms` 的稳定窗口
* 在稳定窗口内启动 ADC 采样
* ADC 完成后恢复 heater PWM

本仓库不直接照搬其 `ADC1/ADC2` 分工，而是只借鉴“窗口时序”和“软件滤波/校准分层”。

## Requirements

* 新增独立热电偶采样模块，不复用当前 `app/sensor/adc_sampler.c`
* 保持当前 `ADC1` 母线电压采样链路不变
* 保持当前呼吸灯行为不被破坏，`TIM8` 继续保留给现有呼吸灯逻辑
* `ADC2` 不再只是“初始化存在”，而要真正进入运行时采样流程
* 采样流程必须体现窗口化时序：
  * 保存当前 heater duty
  * 关断加热
  * 等待固定稳定窗口
  * 启动 `ADC2 DMA`
  * 完成换算/滤波
  * 恢复 heater duty
* 第一阶段临时温度换算按固定冷端近似处理
* 运行时至少能输出：
  * ADC raw
  * ADC 输入端毫伏值
  * 反推热电偶输入微伏值
  * 临时温度值

## Temporary Temperature Model

第一阶段先使用临时模型，不宣称为最终真实温度：

* `gain_total = (100 / 2.7) * (30 / 10 + 1)`
* `adc_mv = raw * 3300 / 4095`
* `tc_uv = adc_mv * 1000 / gain_total`
* `temp_c_tmp = T_cj_fixed + tc_uv / TC_UV_PER_C`

第一阶段默认常量：

* `T_cj_fixed = 25.0 C`
* `TC_UV_PER_C = 41.0`

这些值必须以模块常量或配置常量形式集中定义，后续可替换为更真实的冷端补偿和标定模型。

## Timing Strategy

第一阶段不采用“任务上下文里直接 busy wait 500us”作为最终设计，因为它对调度时序过于敏感。

本任务默认采用独立硬件时序链，同时保留现有呼吸灯：

* `TIM8`
  * 保留当前呼吸灯用途，不挪作热电偶窗口调度
* 新增一个周期定时器
  * 负责按固定周期打开测温窗口，目标周期先按 `25 ms`
* 新增一个 one-pulse 定时器
  * 负责 `0.5 ms` 稳定延时
* `ADC2 DMA`
  * 在稳定延时结束后启动
* `ADC2` 完成回调
  * 负责更新热电偶状态并恢复 heater duty

如果实现中发现定时器资源或 CubeMX 生成约束需要微调，允许调整具体定时器型号，但不允许占用 `TIM8`。

## Timing Diagram

当前仓库第一阶段的窗口采样链，按下面的时序理解：

```text
time --------------------------------------------------------------->

TIM4 update      |---------------- 25 ms ----------------| update ...
                 ^                                       ^
                 |                                       |
                 |                                       +-- 打开下一次测温窗口
                 +-- 打开本次测温窗口

heater PWM       ========= normal PWM =========|OFF|===== normal PWM =====
                                               ^   ^
                                               |   |
                                               |   +-- ADC2 DMA 完成后恢复占空比
                                               +-- PwmControl_SuspendForMeasurement()

TIM5 one-pulse                               |---- 0.5 ms ----|
                                             ^                 ^
                                             |                 |
                                             +-- 关热后启动     +-- 稳定延时结束

ADC2 DMA                                                     [ sample x16 ]
                                                             ^           ^
                                                             |           |
                                                             |           +-- HAL_ADC_ConvCpltCallback()
                                                             +-- HAL_ADC_Start_DMA()

temperature update                                                   [filter/convert]
                                                                      ^
                                                                      |
                                                                      +-- raw -> mV -> uV -> temp_tmp
```

对应当前代码的实际映射：

* `TIM4`
  * 周期性触发 `ThermocoupleSampler_OnSamplingTimerElapsed()`
* `PwmControl_SuspendForMeasurement()`
  * 立即把 heater 输出拉到 `0`
  * 这里要求“立即生效”，不能等下一个 `TIM3` PWM 更新周期
* `TIM5`
  * 作为单次 `0.5 ms` 稳定延时
  * 到期后进入 `ThermocoupleSampler_OnSettleTimerElapsed()`
* `ADC2 DMA`
  * 在稳定窗结束后启动，采集 `16` 个样本
* `HAL_ADC_ConvCpltCallback()`
  * 转入 `ThermocoupleSampler_OnAdcDmaComplete()`
  * 完成裁剪平均、临时温度换算、恢复 heater duty

这个时序里最关键的一点是：

* `heater OFF` 必须在窗口开始时立刻生效
* 如果只是把 `TIM3 CCR` 改成 `0`，但输出仍受 compare preload 约束，那么真正关断可能延后到下一个 `1 kHz` PWM 周期
* 这样 `0.5 ms` 稳定窗就会失真，所以当前实现显式关闭了 `TIM3 CH4` 的 OC preload

## Target Architecture

建议按以下边界落地：

* `Core/`
  * 保持 CubeMX 外设初始化与中断胶水
* `app/sensor/thermocouple_sampler.{c,h}`
  * 热电偶 DMA buffer
  * 窗口采样状态
  * 滤波
  * 临时温度换算
* `app/control/`
  * 如需补充 heater duty 的获取/暂停/恢复接口，应放在控制层
* `app/app_tasks/app_init.c`
  * 启动窗口采样调度
* `app/app_tasks/app_sensor_task.c`
  * 输出调试观测值

## Initial Implementation Strategy

第一阶段优先做最小闭环：

1. 新增热电偶采样模块
2. 用独立状态和缓冲承接 `ADC2 DMA`
3. 新增独立定时器调度，且不占用 `TIM8`
4. ADC 完成后更新热电偶状态并恢复加热
5. 在传感器任务中输出观测值

## Acceptance Criteria

* [ ] 新任务不改坏当前 `ADC1` 电压采样功能
* [ ] 新任务不改坏当前呼吸灯行为
* [ ] `ADC2` 在运行时被真实启动，而不是只有 `MX_ADC2_Init()`
* [ ] 热电偶测量使用“关断加热 -> 延时稳定 -> 采样 -> 恢复”窗口流程
* [ ] 新增独立热电偶模块，未把母线电压采样和热电偶采样混在一个文件
* [ ] 代码中能明确看到总增益和临时温度换算公式
* [ ] `SensorTask` 或等效路径能输出 `raw / mV / uV / temp_tmp`
* [ ] 静态检查后 include 路径和模块边界符合当前仓库目录规范

## Out Of Scope

* 完整 PID 温控闭环重构
* 最终热电偶多项式标定
* 冷端真实补偿
* 电流测量窗口
* 重新设计整套 heater PWM 频率与功率级
* 上板结果背书

## Risks

* 当前工程尚未为热电偶窗口采样预留独立定时器，需要新增 CubeMX/HAL 配置
* 当前 heater PWM 频率仅 `1 kHz`，即使窗口采样先跑通，也不代表已达到最终抗干扰质量
* 固定冷端温度模型只适合 bring-up，不适合当成最终温度真值

## Definition Of Done

* PRD 中定义的窗口采样链路已落成代码
* `ADC2` 热电偶测量具备独立模块和运行时输出
* 现有 `ADC1` 链路未被混改
* 结果报告中明确标注：当前温度值为临时模型结果，尚未完成最终标定
