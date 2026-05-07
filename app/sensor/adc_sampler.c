#include "sensor/adc_sampler.h"

#define ADC_SAMPLE_N 16u
#define ADC_TRIM_X 4u

static volatile uint16_t s_adc_raw;
static volatile uint32_t s_voltage_mv;
static volatile uint16_t s_adc_buf[ADC_SAMPLE_N];

void AdcSampler_UpdateFiltered(void)
{
  uint16_t snap[ADC_SAMPLE_N];
  uint32_t sum = 0u;
  uint8_t n = ADC_SAMPLE_N - 2u * ADC_TRIM_X;

  for (uint8_t i = 0; i < ADC_SAMPLE_N; i++) {
    snap[i] = s_adc_buf[i];
  }

  for (uint8_t i = 0; i < ADC_SAMPLE_N - 1u; i++) {
    for (uint8_t j = i + 1u; j < ADC_SAMPLE_N; j++) {
      if (snap[i] > snap[j]) {
        uint16_t t = snap[i];
        snap[i] = snap[j];
        snap[j] = t;
      }
    }
  }

  for (uint8_t i = ADC_TRIM_X; i < ADC_SAMPLE_N - ADC_TRIM_X; i++) {
    sum += snap[i];
  }

  s_adc_raw = (uint16_t)(sum / n);
  s_voltage_mv = s_adc_raw * 3000u / 4095u * 11000u / 1000u;
}

uint16_t AdcSampler_GetRaw(void)
{
  return s_adc_raw;
}

uint32_t AdcSampler_GetVoltageMv(void)
{
  return s_voltage_mv;
}

uint16_t *AdcSampler_GetDmaBuffer(void)
{
  return (uint16_t *)s_adc_buf;
}

uint32_t AdcSampler_GetDmaSampleCount(void)
{
  return ADC_SAMPLE_N;
}
