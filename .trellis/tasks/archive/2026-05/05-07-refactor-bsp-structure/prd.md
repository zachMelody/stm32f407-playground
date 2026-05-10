# 整理 STM32CubeMX 项目的 BSP 目录结构

## Goal

在不改变现有硬件行为的前提下，整理当前 STM32F407 工程的用户代码目录边界，
把 `bsp/`、`port/`、应用逻辑、资源文件的职责拆清，形成一个适合后续继续接
入传感器、显示、输入设备和 UI 页面扩展的 STM32CubeMX 项目结构。

## What I already know

* 当前 `Core/` 基本保持 CubeMX 生成结构，方向正确。
* 当前 `port/` 仅承载 LVGL 适配层，边界比较清晰。
* 当前 `bsp/` 同时混放了：
  * 设备驱动：`ina226.c/.h`
  * 显示驱动：`lcd_init.c/.h`、`lcd.c/.h`
  * demo 逻辑：`effects.c/.h`
  * 资源头文件：`BMP.h`、`pic.h`、`lcdfont.h`
* `CMakeLists.txt` 当前按平铺方式直接加入 `bsp/*.c` 和 `port/*.c`。
* `Core/Src/main.c` 当前直接并列引用了传感器、屏驱、LVGL port 和资源头文件，
  主入口对多个层次都有感知。

## Assumptions (temporary)

* 本任务优先做“结构整理 + 构建与引用修正”，不主动引入新功能。
* 本任务以最小行为风险为原则，重构后程序外部表现应保持一致。
* `port/` 继续只承载第三方中间件适配层，不吸收业务逻辑。
* `Core/` 继续优先保留 CubeMX 生成文件，用户自定义逻辑尽量外移。

## Open Questions

* 是否在本任务内同步引入 `app/` 与 `assets/` 目录，并完成相关文件迁移。
* 是否只重构目录与 include 路径，还是顺手收敛 `main.c` 中的初始化和显示逻辑。

## Requirements (evolving)

* 明确各目录职责边界：
  * `Core/`：CubeMX 生成代码与少量 USER CODE
  * `bsp/`：板级/外设驱动
  * `port/`：LVGL 等第三方中间件适配
  * `app/`：业务任务、界面组织、演示逻辑
  * `assets/`：图片、字库、静态资源
* 给出适用于本仓库的推荐目录树，而不是泛化模板。
* 如进入实施，迁移后的构建系统需要同步更新，保证 CMake 仍可编译。
* 如进入实施，代码 include 路径需要同步更新，避免出现层次反转和循环依赖。
* 整理后 `main.c` 不应继续直接感知过多资源层细节。

## Acceptance Criteria (evolving)

* [ ] 能给出当前结构中“不合理混层点”的明确清单。
* [ ] 能产出一版适用于本仓库的目标目录结构方案。
* [ ] 若执行重构，`CMakeLists.txt` 和 include 路径同步完成，工程可继续编译。
* [ ] 若执行重构，`bsp/` 内不再混放 demo 逻辑和资源头文件。
* [ ] 若执行重构，`port/` 仍保持为 LVGL 适配层，不承载业务逻辑。
* [ ] 若执行重构，外部功能行为无预期外变化。

## Definition of Done

* 目录职责定义清晰，并记录到任务 PRD 中。
* 若落代码，相关移动、重命名、include 修正、CMake 修正全部完成。
* 至少完成一次构建验证，确认没有因路径调整导致的编译失败。
* 如沉淀出稳定规则，需要同步更新 `.trellis/spec/backend/directory-structure.md`。

## Out of Scope

* 新增传感器能力或显示功能。
* 重写 LVGL 页面交互。
* 改造 HAL、CMSIS、CubeMX 生成目录本身。
* 大规模重构 FreeRTOS 任务模型。

## Technical Approach

建议优先按“最小侵入重构”推进：

1. 先定义目标目录边界与迁移规则。
2. 再迁移资源与 demo 代码，降低 `bsp/` 语义混乱。
3. 再按需迁移显示/传感器驱动到 `bsp` 子目录。
4. 最后修正 `CMakeLists.txt`、include 路径和主入口引用。

## Decision (ADR-lite)

**Context**：当前项目已经从单一 LCD 演示演进为含 LVGL、PWM、ADC、INA226 的综合
固件，原先“把用户代码都塞进 `bsp/`”的方式开始失去可维护性。

**Decision**：创建独立任务，专门收敛 STM32CubeMX 项目中的目录边界，而不是在后
续功能开发时继续临时堆放。

**Consequences**：

* 短期会引入一次目录调整成本。
* 中长期可降低继续接入硬件和 UI 时的耦合度。
* 后续若再新增设备，模块落点会更明确。

## Technical Notes

* 当前观测到的关键文件：
  * `CMakeLists.txt`
  * `Core/Src/main.c`
  * `bsp/ina226.c`
  * `bsp/lcd_init.c`
  * `bsp/lcd.c`
  * `bsp/effects.c`
  * `port/lv_port_disp.c`
  * `.trellis/spec/backend/directory-structure.md`
* 当前目录现状：
  * `Core/`：CubeMX 生成
  * `bsp/`：驱动、demo、资源混放
  * `port/`：LVGL 适配层
* 推荐目标方向：
  * `bsp/display/`
  * `bsp/sensor/`
  * `app/demo/` 或 `app/ui/`
  * `assets/`
