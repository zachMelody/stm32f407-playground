#include "sensor/thermocouple_sampler.h"

#include "adc.h"
#include "control/pwm_control.h"
#include "tim.h"

#define THERMOCOUPLE_DMA_SAMPLE_N 16u
#define THERMOCOUPLE_TRIM_X 4u
#define THERMOCOUPLE_ADC_FULL_SCALE_MV 3300u
#define THERMOCOUPLE_ADC_MAX_CODE 4095u
#define THERMOCOUPLE_TC_UV_NUM 27u
#define THERMOCOUPLE_TC_UV_DEN 4u
/* AxxSolder thermocouple gain ~240, local gain=(100/2.7)*(30/10+1)=400/2.7, ratio = 81/50. */
#define THERMOCOUPLE_LOCAL_TO_AXX_RAW_NUM 81u
#define THERMOCOUPLE_LOCAL_TO_AXX_RAW_DEN 50u
#define THERMOCOUPLE_LOCAL_TO_AXX_RAW_SCALED(raw_)                                                       \
  ((((uint32_t)(raw_)) * THERMOCOUPLE_LOCAL_TO_AXX_RAW_NUM +                                             \
    (THERMOCOUPLE_LOCAL_TO_AXX_RAW_DEN / 2u)) /                                                          \
   THERMOCOUPLE_LOCAL_TO_AXX_RAW_DEN)
#define THERMOCOUPLE_LOCAL_TO_AXX_RAW(raw_)                                                              \
  ((THERMOCOUPLE_LOCAL_TO_AXX_RAW_SCALED(raw_) > THERMOCOUPLE_ADC_MAX_CODE) ?                           \
     THERMOCOUPLE_ADC_MAX_CODE :                                                                         \
     THERMOCOUPLE_LOCAL_TO_AXX_RAW_SCALED(raw_))

/*
 * T210 nominal fit from AxxSolder, converted to x10 deg C fixed-point:
 * temp_c = adc^2 * 4.223931712905644e-06 + adc * 0.31863796444354214 + 20.968033870812942
 */
#define THERMOCOUPLE_T210_TEMP_X10_SCALE 1000000000LL
#define THERMOCOUPLE_T210_TEMP_X10_X2_NUM 42239LL
#define THERMOCOUPLE_T210_TEMP_X10_X1_NUM 3186379644LL
#define THERMOCOUPLE_T210_TEMP_X10_X0_NUM 209680338708LL
#define THERMOCOUPLE_T210_TEMP_X10_FROM_RAW(raw_)                                                        \
  ((int32_t)(((((int64_t)(raw_) * (int64_t)(raw_) * THERMOCOUPLE_T210_TEMP_X10_X2_NUM) +                \
                ((int64_t)(raw_) * THERMOCOUPLE_T210_TEMP_X10_X1_NUM) +                                  \
                THERMOCOUPLE_T210_TEMP_X10_X0_NUM +                                                       \
                (THERMOCOUPLE_T210_TEMP_X10_SCALE / 2LL))) /                                              \
             THERMOCOUPLE_T210_TEMP_X10_SCALE))

_Static_assert(THERMOCOUPLE_T210_TEMP_X10_FROM_RAW(0u) == 210, "T210 fit mismatch at raw=0");
_Static_assert(THERMOCOUPLE_T210_TEMP_X10_FROM_RAW(500u) == 1813, "T210 fit mismatch at raw=500");
_Static_assert(THERMOCOUPLE_T210_TEMP_X10_FROM_RAW(1000u) == 3438, "T210 fit mismatch at raw=1000");
_Static_assert(THERMOCOUPLE_T210_TEMP_X10_FROM_RAW(1250u) == 4259, "T210 fit mismatch at raw=1250");
_Static_assert(THERMOCOUPLE_LOCAL_TO_AXX_RAW(500u) == 810u, "Axx raw scaling mismatch at raw=500");
_Static_assert(THERMOCOUPLE_LOCAL_TO_AXX_RAW(1000u) == 1620u, "Axx raw scaling mismatch at raw=1000");
_Static_assert(THERMOCOUPLE_LOCAL_TO_AXX_RAW(3000u) == 4095u, "Axx raw scaling clamp mismatch");

static volatile uint16_t s_dma_buf[THERMOCOUPLE_DMA_SAMPLE_N];
static volatile uint8_t s_sampling_active;
static thermocouple_snapshot_t s_snapshot;

static uint16_t ThermocoupleSampler_ConvertLocalRawToAxxRaw(uint16_t raw)
{
  return (uint16_t)THERMOCOUPLE_LOCAL_TO_AXX_RAW(raw);
}

static int32_t ThermocoupleSampler_ConvertRawToT210TempX10(uint16_t raw)
{
  return THERMOCOUPLE_T210_TEMP_X10_FROM_RAW(ThermocoupleSampler_ConvertLocalRawToAxxRaw(raw));
}

static uint16_t ThermocoupleSampler_ComputeTrimmedMean(void)
{
  uint16_t snap[THERMOCOUPLE_DMA_SAMPLE_N];
  uint32_t sum = 0u;
  uint32_t valid_n = THERMOCOUPLE_DMA_SAMPLE_N - (2u * THERMOCOUPLE_TRIM_X);

  for (uint32_t i = 0; i < THERMOCOUPLE_DMA_SAMPLE_N; i++) {
    snap[i] = s_dma_buf[i];
  }

  for (uint32_t i = 0; i < THERMOCOUPLE_DMA_SAMPLE_N - 1u; i++) {
    for (uint32_t j = i + 1u; j < THERMOCOUPLE_DMA_SAMPLE_N; j++) {
      if (snap[i] > snap[j]) {
        uint16_t tmp = snap[i];
        snap[i] = snap[j];
        snap[j] = tmp;
      }
    }
  }

  for (uint32_t i = THERMOCOUPLE_TRIM_X; i < (THERMOCOUPLE_DMA_SAMPLE_N - THERMOCOUPLE_TRIM_X); i++) {
    sum += snap[i];
  }

  return (uint16_t)(sum / valid_n);
}

static void ThermocoupleSampler_FinishWindow(void)
{
  s_sampling_active = 0u;
  s_snapshot.sampling_active = 0u;
  PwmControl_ResumeAfterMeasurement();
}

void ThermocoupleSampler_Init(void)
{
  for (uint32_t i = 0; i < THERMOCOUPLE_DMA_SAMPLE_N; i++) {
    s_dma_buf[i] = 0u;
  }

  s_sampling_active = 0u;
  s_snapshot.raw = 0u;
  s_snapshot.adc_mv = 0u;
  s_snapshot.tc_uv = 0u;
  s_snapshot.temp_c_x10 = 0;
  s_snapshot.sample_count = 0u;
  s_snapshot.has_sample = 0u;
  s_snapshot.sampling_active = 0u;

  __HAL_TIM_DISABLE(&htim5);
  __HAL_TIM_CLEAR_FLAG(&htim5, TIM_FLAG_UPDATE);
  __HAL_TIM_ENABLE_IT(&htim5, TIM_IT_UPDATE);
}

HAL_StatusTypeDef ThermocoupleSampler_Start(void)
{
  return HAL_TIM_Base_Start_IT(&htim4);
}

void ThermocoupleSampler_OnSamplingTimerElapsed(void)
{
  if (s_sampling_active != 0u) {
    return;
  }

  s_sampling_active = 1u;
  s_snapshot.sampling_active = 1u;
  PwmControl_SuspendForMeasurement();

  __HAL_TIM_SET_COUNTER(&htim5, 0u);
  __HAL_TIM_CLEAR_FLAG(&htim5, TIM_FLAG_UPDATE);
  __HAL_TIM_ENABLE(&htim5);
}

void ThermocoupleSampler_OnSettleTimerElapsed(void)
{
  if (s_sampling_active == 0u) {
    return;
  }

  if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)s_dma_buf, THERMOCOUPLE_DMA_SAMPLE_N) != HAL_OK) {
    ThermocoupleSampler_FinishWindow();
  }
}

void ThermocoupleSampler_OnAdcDmaComplete(void)
{
  uint16_t raw;
  uint32_t adc_mv;
  uint32_t tc_uv;

  if (s_sampling_active == 0u) {
    return;
  }

  (void)HAL_ADC_Stop_DMA(&hadc2);

  raw = ThermocoupleSampler_ComputeTrimmedMean();
  adc_mv = ((uint32_t)raw * THERMOCOUPLE_ADC_FULL_SCALE_MV) / THERMOCOUPLE_ADC_MAX_CODE;

  /* Keep reconstructed thermocouple uV for debug/UI, but drive temperature from the T210 ADC fit. */
  tc_uv = (adc_mv * THERMOCOUPLE_TC_UV_NUM + (THERMOCOUPLE_TC_UV_DEN / 2u)) / THERMOCOUPLE_TC_UV_DEN;

  s_snapshot.raw = raw;
  s_snapshot.adc_mv = adc_mv;
  s_snapshot.tc_uv = tc_uv;
  s_snapshot.temp_c_x10 = ThermocoupleSampler_ConvertRawToT210TempX10(raw);
  s_snapshot.sample_count += 1u;
  s_snapshot.has_sample = 1u;

  ThermocoupleSampler_FinishWindow();
}

void ThermocoupleSampler_GetSnapshot(thermocouple_snapshot_t *snapshot)
{
  uint32_t primask;

  if (snapshot == NULL) {
    return;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  *snapshot = s_snapshot;
  if (primask == 0u) {
    __enable_irq();
  }
}
