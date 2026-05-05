# PWM/ADC 显示与串口控制

> 状态：**需求已确认，待最终签字**

## Goal

在 STM32F407 + LVGL 9.2 项目上，通过 PB1 输出 PWM 驱动 JBC C210 焊咀，PC1 ADC 采集分压电压，LCD 实时显示占空比和电压，UART 命令调节占空比。

## Requirements

### 硬件配置

| 引脚 | 功能 | 配置 |
|------|------|------|
| **PB1** | PWM → EG2132 栅极驱动 → JBC C210（新增） | TIM3_CH4 (AF2)，1 kHz，分辨率 0.1%，**duty ≤ 95%**（EG2132 自举电容限制） |
| **PC1** | ADC1_IN11 → 电压采样（已有，VREF=3V） | DMA 循环连续采样 |
| **PA9/PA10** | USART1 命令接收 + 回显 | 115200 8N1（已有 DMA） |
| **LCD** | LVGL 9.2 显示 | 320×240 |
| **PB2** | 呼吸灯（软件 PWM via TIM8） | **保留不动** |

### PWM（PB1）

- TIM3_CH4，**通过 .ioc 配置**（PSC=84-1, ARR=1000-1 → 1 kHz）
- 占空比分辨率：0.1%（CCR4 = 0..1000，对应 0~100%）
- **硬上限：duty ≤ 95.0%（CCR4 ≤ 950）** — EG2132 自举电容补充电要求低边周期性导通
- **上电初值：0%**（安全优先，避免误烫）
- 提供 `pwm_set_duty(uint16_t duty_x10)` 接口：参数 0~1000，**内部 clamp 到 ≤ 950**
- 实施策略：
  - **同时改 .ioc + 手工同步 tim.c/h**，使代码立即可编译；后续 CubeMX 重新生成不会丢失

### ADC（PC1）

- 修正现有错误：`raw * 3300 / 4095` → `raw * 3000 / 4095`（VREF=3V）
- **8 点滑动平均**滤波（在 SensorTask 里维护 ring buffer）
- 提供全局 `g_voltage_mv`（uint32_t，单字读写，无需锁）

### UART 命令

- **格式：纯数字 + 回车/换行**
  - 例：`75\r` 或 `75\n` → 占空比设为 75%
  - **范围 0~95**（受 EG2132 自举电容限制），**96~100 拒绝**并打印 `[ERR] duty must be 0..95 (EG2132 bootstrap)`
  - 非数字 / 空行 → 打印 `[ERR] expect integer 0..95`
  - 只接受整数（不接受小数，避免分辨率歧义）
- 在现有 EchoTask 的 DMA RX 处理流程上扩展：
  - 累积字符到行缓冲，遇 `\r` 或 `\n` 触发解析
  - 解析成功 → 调用 `pwm_set_duty(value * 10)` + 打印 ack（如 `[OK] duty=75%`）
  - 解析失败 → 打印对应错误
  - 现有"echo 回显"功能保留（用户输入回显）
- 上电 banner 增加命令使用说明（含 95% 上限说明）

### LVGL 界面

布局（320×240）：

```
┌─────────────────────────────────┐
│                                 │
│     PWM:  75.0 %    (大字)       │
│  ┌─────────────────────────────┐ │
│  │█████████████░░░░░░░░░░░░░░░░│ │ ← lv_bar
│  └─────────────────────────────┘ │
│                                 │
│     V:    1.234 V    (大字)      │
│                                 │
└─────────────────────────────────┘
```

- 字体：lv_font_montserrat_28（标题数值），需要在 lv_conf.h 启用
- 进度条：lv_bar，0-1000 映射到 0-100%
- 刷新率：~5 fps（每 200 ms 更新一次，避免抢 CPU）
- 移除现有红色测试方块和 "LVGL 9.2 OK" 文字

## Acceptance Criteria

- [ ] PB1 输出 1 kHz PWM，示波器可测可调
- [ ] 串口输入 `0\r`、`50\r`、`95\r` 都能正确设置占空比
- [ ] 串口输入 `96\r` ~ `100\r` 被拒绝并提示 EG2132 限制
- [ ] LCD 实时显示占空比（百分比+进度条）和电压（V，3 位小数）
- [ ] ADC 用 VREF=3V 公式，PC1 短接 GND 时显示 0V，短接 3V 时接近 3V
- [ ] PB2 呼吸灯仍正常工作
- [ ] 串口启动 banner 显示新命令用法（含 95% 上限说明）
- [ ] 编译无 warning，lint 通过

## Technical Notes

### 任务分工

| 任务 | 职责 |
|------|------|
| `App_LVGLTask` | 200ms 周期读 `g_pwm_duty_x10` / `g_voltage_mv`，刷新 lv_label / lv_bar |
| `App_SensorTask` | 100ms 读 ADC 原始 + 8点滑动平均 + 写 `g_voltage_mv` + VOFA 推送 |
| `App_EchoTask` | 1ms 轮询 DMA RX；行缓冲累积；遇行尾解析数字 → `pwm_set_duty()` |
| `MX_TIM3_Init` (新) | TIM3_CH4 PWM 模式，PB1 AF2，CCR4=0 启动 |

### 共享变量（无需锁，单字读写）

```c
#define PWM_DUTY_MAX_X10   950u   /* EG2132 自举电容硬上限：95.0% */

static volatile uint16_t g_pwm_duty_x10;   // 0..950 (0.0~95.0%)
static volatile uint32_t g_voltage_mv;     // 0..3000 mV
```

### 文件改动清单

- `LED_F407.ioc`：新增 TIM3 + PB1 PWM Generation CH4 + VP_TIM3 时钟源
- `Core/Src/tim.c`、`Core/Inc/tim.h`：手工同步 `htim3` + `MX_TIM3_Init()` + `HAL_TIM_MspPostInit`（PB1 AF2）
- `Core/Src/main.c`：调用 `MX_TIM3_Init()` + 启动 PWM；新增 PWM 模块、UART 命令解析、ADC 公式修正、LVGL 数据驱动
- `port/lv_conf.h`：启用 montserrat_28 字体
