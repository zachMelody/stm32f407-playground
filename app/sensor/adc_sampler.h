#ifndef __ADC_SAMPLER_H__
#define __ADC_SAMPLER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

void AdcSampler_UpdateFiltered(void);
uint16_t AdcSampler_GetRaw(void);
uint32_t AdcSampler_GetVoltageMv(void);
uint16_t *AdcSampler_GetDmaBuffer(void);
uint32_t AdcSampler_GetDmaSampleCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_SAMPLER_H__ */
