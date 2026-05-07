#ifndef __PWM_CONTROL_H__
#define __PWM_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

void PwmControl_SetDutyX10(uint16_t duty_x10);
uint16_t PwmControl_GetDutyX10(void);
const char *PwmControl_CycleBreathSpeed(uint32_t *cycle_seconds);
void PwmControl_OnTim8Elapsed(void);

#ifdef __cplusplus
}
#endif

#endif /* __PWM_CONTROL_H__ */
