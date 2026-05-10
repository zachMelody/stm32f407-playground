#include "control/temperature_control.h"

#include "control/pwm_control.h"

#define TEMP_CTRL_DEFAULT_SETPOINT_C_X10 3000
#define TEMP_CTRL_KP_NUM 10
#define TEMP_CTRL_KP_DEN 10
#define TEMP_CTRL_KI_POS_NUM 1
#define TEMP_CTRL_KI_POS_DEN 80
#define TEMP_CTRL_KI_NEG_NUM 1
#define TEMP_CTRL_KI_NEG_DEN 20
#define TEMP_CTRL_KD_NUM 6
#define TEMP_CTRL_KD_DEN 5
#define TEMP_CTRL_ERROR_DEADBAND_C_X10 20
#define TEMP_CTRL_I_ACTIVE_WINDOW_C_X10 750
#define TEMP_CTRL_BOOST_ENTER_ERROR_C_X10 1200
#define TEMP_CTRL_BOOST_EXIT_ERROR_C_X10 700
#define TEMP_CTRL_OVERSHOOT_CUTOFF_C_X10 (-100)
#define TEMP_CTRL_FILTER_SHIFT 2
#define TEMP_CTRL_D_TERM_MIN_X10 (-300)
#define TEMP_CTRL_D_TERM_MAX_X10 300
#define TEMP_CTRL_I_TERM_MIN_X10 (-250)
#define TEMP_CTRL_I_TERM_MAX_X10 350

static temperature_control_snapshot_t s_snapshot;
static uint32_t s_last_processed_sample_count;
static uint8_t s_auto_recompute_pending;
static uint8_t s_auto_boost_active;
static uint8_t s_filtered_temp_valid;
static int32_t s_filtered_temp_c_x10;
static int32_t s_last_control_temp_c_x10;

static uint32_t TemperatureControl_EnterCritical(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

static void TemperatureControl_ExitCritical(uint32_t primask)
{
  if (primask == 0u) {
    __enable_irq();
  }
}

static uint16_t TemperatureControl_ClampDutyX10(int32_t duty_x10)
{
  if (duty_x10 <= 0) {
    return 0u;
  }
  if (duty_x10 >= (int32_t)TEMP_CTRL_DUTY_MAX_X10) {
    return TEMP_CTRL_DUTY_MAX_X10;
  }
  return (uint16_t)duty_x10;
}

static int32_t TemperatureControl_ClampIntegralTermX10(int32_t integral_term_x10)
{
  if (integral_term_x10 < TEMP_CTRL_I_TERM_MIN_X10) {
    return TEMP_CTRL_I_TERM_MIN_X10;
  }
  if (integral_term_x10 > TEMP_CTRL_I_TERM_MAX_X10) {
    return TEMP_CTRL_I_TERM_MAX_X10;
  }
  return integral_term_x10;
}

static int32_t TemperatureControl_ClampSignedTermX10(int32_t term_x10, int32_t min_x10, int32_t max_x10)
{
  if (term_x10 < min_x10) {
    return min_x10;
  }
  if (term_x10 > max_x10) {
    return max_x10;
  }
  return term_x10;
}

static int32_t TemperatureControl_FilterStepX10(int32_t current_x10, int32_t target_x10, int32_t shift)
{
  int32_t delta_x10 = target_x10 - current_x10;
  int32_t step_x10;

  if (delta_x10 == 0) {
    return current_x10;
  }

  if (delta_x10 > 0) {
    step_x10 = (delta_x10 + ((1 << shift) - 1)) >> shift;
  } else {
    step_x10 = -(((-delta_x10) + ((1 << shift) - 1)) >> shift);
  }

  return current_x10 + step_x10;
}

static int32_t TemperatureControl_UpdateFilteredTempX10(int32_t measured_temp_c_x10, int32_t *delta_temp_c_x10)
{
  int32_t filtered_temp_c_x10;

  if (s_filtered_temp_valid == 0u) {
    s_filtered_temp_valid = 1u;
    s_filtered_temp_c_x10 = measured_temp_c_x10;
    s_last_control_temp_c_x10 = measured_temp_c_x10;
    if (delta_temp_c_x10 != NULL) {
      *delta_temp_c_x10 = 0;
    }
    return measured_temp_c_x10;
  }

  filtered_temp_c_x10 =
    TemperatureControl_FilterStepX10(s_filtered_temp_c_x10, measured_temp_c_x10, TEMP_CTRL_FILTER_SHIFT);
  if (delta_temp_c_x10 != NULL) {
    *delta_temp_c_x10 = filtered_temp_c_x10 - s_last_control_temp_c_x10;
  }

  s_filtered_temp_c_x10 = filtered_temp_c_x10;
  s_last_control_temp_c_x10 = filtered_temp_c_x10;
  return filtered_temp_c_x10;
}

static void TemperatureControl_ApplyDutyX10(uint16_t duty_x10)
{
  s_snapshot.applied_duty_x10 = duty_x10;
  PwmControl_SetDutyX10(duty_x10);
}

void TemperatureControl_Init(void)
{
  uint32_t primask = TemperatureControl_EnterCritical();

  s_snapshot.mode = TEMP_CTRL_MODE_OFF;
  s_snapshot.has_temperature = 0u;
  s_snapshot.manual_duty_x10 = 0u;
  s_snapshot.applied_duty_x10 = 0u;
  s_snapshot.last_sample_count = 0u;
  s_snapshot.tc_uv = 0u;
  s_snapshot.setpoint_c_x10 = TEMP_CTRL_DEFAULT_SETPOINT_C_X10;
  s_snapshot.measured_temp_c_x10 = 0;
  s_snapshot.error_c_x10 = 0;
  s_snapshot.integral_term_x10 = 0;
  s_last_processed_sample_count = 0u;
  s_auto_recompute_pending = 0u;
  s_auto_boost_active = 0u;
  s_filtered_temp_valid = 0u;
  s_filtered_temp_c_x10 = 0;
  s_last_control_temp_c_x10 = 0;
  TemperatureControl_ApplyDutyX10(0u);

  TemperatureControl_ExitCritical(primask);
}

void TemperatureControl_SetOff(void)
{
  uint32_t primask = TemperatureControl_EnterCritical();

  s_snapshot.mode = TEMP_CTRL_MODE_OFF;
  s_snapshot.error_c_x10 = 0;
  s_snapshot.integral_term_x10 = 0;
  s_auto_recompute_pending = 0u;
  s_auto_boost_active = 0u;
  TemperatureControl_ApplyDutyX10(0u);

  TemperatureControl_ExitCritical(primask);
}

uint8_t TemperatureControl_SetManualDutyX10(uint16_t duty_x10)
{
  uint32_t primask;

  if (duty_x10 > TEMP_CTRL_DUTY_MAX_X10) {
    return 0u;
  }

  primask = TemperatureControl_EnterCritical();

  s_snapshot.mode = TEMP_CTRL_MODE_MANUAL;
  s_snapshot.manual_duty_x10 = duty_x10;
  s_snapshot.error_c_x10 = 0;
  s_snapshot.integral_term_x10 = 0;
  s_auto_recompute_pending = 0u;
  s_auto_boost_active = 0u;
  TemperatureControl_ApplyDutyX10(duty_x10);

  TemperatureControl_ExitCritical(primask);
  return 1u;
}

uint8_t TemperatureControl_SetAutoSetpointCX10(int32_t setpoint_c_x10)
{
  uint32_t primask;

  if (setpoint_c_x10 < TEMP_CTRL_SETPOINT_MIN_C_X10 || setpoint_c_x10 > TEMP_CTRL_SETPOINT_MAX_C_X10) {
    return 0u;
  }

  primask = TemperatureControl_EnterCritical();

  s_snapshot.mode = TEMP_CTRL_MODE_AUTO;
  s_snapshot.setpoint_c_x10 = setpoint_c_x10;
  s_snapshot.error_c_x10 = 0;
  s_snapshot.integral_term_x10 = 0;
  s_auto_recompute_pending = 1u;
  s_auto_boost_active = 0u;
  if (s_snapshot.has_temperature == 0u) {
    TemperatureControl_ApplyDutyX10(0u);
  }

  TemperatureControl_ExitCritical(primask);
  return 1u;
}

void TemperatureControl_UpdateFromSample(const thermocouple_snapshot_t *sample)
{
  uint32_t primask;
  uint8_t has_new_sample = 0u;
  int32_t control_temp_c_x10 = 0;
  int32_t delta_temp_c_x10 = 0;
  int32_t error_c_x10;
  int32_t p_term_x10;
  int32_t d_term_x10;
  int32_t integral_term_x10;
  int32_t candidate_integral_term_x10;
  int32_t duty_x10;

  if (sample == NULL) {
    return;
  }

  primask = TemperatureControl_EnterCritical();

  if (sample->has_sample != 0u) {
    s_snapshot.has_temperature = 1u;
    s_snapshot.last_sample_count = sample->sample_count;
    s_snapshot.tc_uv = sample->tc_uv;
    has_new_sample = (sample->sample_count != s_last_processed_sample_count) || (s_filtered_temp_valid == 0u);
    if (has_new_sample != 0u) {
      control_temp_c_x10 = TemperatureControl_UpdateFilteredTempX10(sample->temp_c_x10, &delta_temp_c_x10);
    } else {
      control_temp_c_x10 = s_snapshot.measured_temp_c_x10;
      delta_temp_c_x10 = 0;
    }
    s_snapshot.measured_temp_c_x10 = control_temp_c_x10;
  } else {
    s_snapshot.has_temperature = 0u;
    s_snapshot.tc_uv = 0u;
    s_snapshot.measured_temp_c_x10 = 0;
    s_filtered_temp_valid = 0u;
    s_auto_boost_active = 0u;
  }

  if (s_snapshot.mode == TEMP_CTRL_MODE_OFF) {
    TemperatureControl_ApplyDutyX10(0u);
    TemperatureControl_ExitCritical(primask);
    return;
  }

  if (s_snapshot.mode == TEMP_CTRL_MODE_MANUAL) {
    TemperatureControl_ApplyDutyX10(s_snapshot.manual_duty_x10);
    TemperatureControl_ExitCritical(primask);
    return;
  }

  if (sample->has_sample == 0u) {
    s_snapshot.error_c_x10 = 0;
    s_snapshot.integral_term_x10 = 0;
    TemperatureControl_ApplyDutyX10(0u);
    TemperatureControl_ExitCritical(primask);
    return;
  }

  if (s_auto_recompute_pending == 0u && has_new_sample == 0u) {
    TemperatureControl_ExitCritical(primask);
    return;
  }

  if (has_new_sample != 0u) {
    s_last_processed_sample_count = sample->sample_count;
  }
  s_auto_recompute_pending = 0u;

  error_c_x10 = s_snapshot.setpoint_c_x10 - control_temp_c_x10;
  if (error_c_x10 >= TEMP_CTRL_BOOST_ENTER_ERROR_C_X10) {
    s_auto_boost_active = 1u;
  } else if (error_c_x10 <= TEMP_CTRL_BOOST_EXIT_ERROR_C_X10) {
    s_auto_boost_active = 0u;
  }

  /* Large overshoot should cut heater immediately instead of waiting for PI to unwind. */
  if (error_c_x10 <= TEMP_CTRL_OVERSHOOT_CUTOFF_C_X10) {
    s_snapshot.error_c_x10 = error_c_x10;
    if (s_snapshot.integral_term_x10 > 0) {
      s_snapshot.integral_term_x10 = 0;
    }
    TemperatureControl_ApplyDutyX10(0u);
    TemperatureControl_ExitCritical(primask);
    return;
  }

  if (s_auto_boost_active != 0u) {
    s_snapshot.error_c_x10 = error_c_x10;
    s_snapshot.integral_term_x10 = 0;
    TemperatureControl_ApplyDutyX10(TEMP_CTRL_DUTY_MAX_X10);
    TemperatureControl_ExitCritical(primask);
    return;
  }

  p_term_x10 = (error_c_x10 * TEMP_CTRL_KP_NUM) / TEMP_CTRL_KP_DEN;
  d_term_x10 = -((delta_temp_c_x10 * TEMP_CTRL_KD_NUM) / TEMP_CTRL_KD_DEN);
  d_term_x10 = TemperatureControl_ClampSignedTermX10(d_term_x10,
                                                     TEMP_CTRL_D_TERM_MIN_X10,
                                                     TEMP_CTRL_D_TERM_MAX_X10);
  integral_term_x10 = s_snapshot.integral_term_x10;
  candidate_integral_term_x10 = integral_term_x10;

  /* Only let I dominate near the setpoint; far away the loop should behave mostly like boost + PD. */
  if (error_c_x10 > TEMP_CTRL_I_ACTIVE_WINDOW_C_X10 || error_c_x10 < -TEMP_CTRL_I_ACTIVE_WINDOW_C_X10) {
    if (candidate_integral_term_x10 > 0) {
      candidate_integral_term_x10 -= 1;
    } else if (candidate_integral_term_x10 < 0) {
      candidate_integral_term_x10 += 1;
    }
  } else if (error_c_x10 > TEMP_CTRL_ERROR_DEADBAND_C_X10) {
    int32_t delta_i_x10 = (error_c_x10 * TEMP_CTRL_KI_POS_NUM) / TEMP_CTRL_KI_POS_DEN;
    int32_t predicted_output_x10 = p_term_x10 + d_term_x10 + candidate_integral_term_x10;

    if (predicted_output_x10 < (int32_t)TEMP_CTRL_DUTY_MAX_X10) {
      candidate_integral_term_x10 += delta_i_x10;
    }
  } else if (error_c_x10 < -TEMP_CTRL_ERROR_DEADBAND_C_X10) {
    candidate_integral_term_x10 += (error_c_x10 * TEMP_CTRL_KI_NEG_NUM) / TEMP_CTRL_KI_NEG_DEN;
  } else if (candidate_integral_term_x10 > 0) {
    candidate_integral_term_x10 -= 1;
  } else if (candidate_integral_term_x10 < 0) {
    candidate_integral_term_x10 += 1;
  }

  integral_term_x10 = TemperatureControl_ClampIntegralTermX10(candidate_integral_term_x10);
  duty_x10 = p_term_x10 + d_term_x10 + integral_term_x10;

  s_snapshot.error_c_x10 = error_c_x10;
  s_snapshot.integral_term_x10 = integral_term_x10;
  TemperatureControl_ApplyDutyX10(TemperatureControl_ClampDutyX10(duty_x10));

  TemperatureControl_ExitCritical(primask);
}

void TemperatureControl_GetSnapshot(temperature_control_snapshot_t *snapshot)
{
  uint32_t primask;

  if (snapshot == NULL) {
    return;
  }

  primask = TemperatureControl_EnterCritical();
  *snapshot = s_snapshot;
  TemperatureControl_ExitCritical(primask);
}

const char *TemperatureControl_ModeName(uint8_t mode)
{
  switch ((temperature_control_mode_t)mode) {
  case TEMP_CTRL_MODE_MANUAL:
    return "MAN";
  case TEMP_CTRL_MODE_AUTO:
    return "AUTO";
  case TEMP_CTRL_MODE_OFF:
  default:
    return "OFF";
  }
}
