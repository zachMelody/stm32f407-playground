#include "ui/ui_main_screen.h"

#include "lvgl.h"

static lv_obj_t *s_lbl_ina_vi;
static lv_obj_t *s_lbl_ina_p;
static lv_obj_t *s_lbl_pwm;
static lv_obj_t *s_bar_pwm;
static lv_obj_t *s_lbl_raw;
static lv_obj_t *s_lbl_v;
static uint8_t s_initialized;

void UiMainScreen_Init(void)
{
  lv_obj_t *scr;

  if (s_initialized) {
    return;
  }

  scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  s_lbl_ina_vi = lv_label_create(scr);
  lv_obj_set_style_text_color(s_lbl_ina_vi, lv_color_hex(0x00C0FF), 0);
  lv_obj_set_style_text_font(s_lbl_ina_vi, &lv_font_montserrat_14, 0);
  lv_label_set_text(s_lbl_ina_vi, "INA:  --.-- V / --- mA");
  lv_obj_align(s_lbl_ina_vi, LV_ALIGN_TOP_MID, 0, 8);

  s_lbl_ina_p = lv_label_create(scr);
  lv_obj_set_style_text_color(s_lbl_ina_p, lv_color_hex(0x00C0FF), 0);
  lv_obj_set_style_text_font(s_lbl_ina_p, &lv_font_montserrat_14, 0);
  lv_label_set_text(s_lbl_ina_p, "PWR:  --.-- W");
  lv_obj_align(s_lbl_ina_p, LV_ALIGN_TOP_MID, 0, 28);

  s_lbl_pwm = lv_label_create(scr);
  lv_obj_set_style_text_color(s_lbl_pwm, lv_color_white(), 0);
  lv_obj_set_style_text_font(s_lbl_pwm, &lv_font_montserrat_28, 0);
  lv_label_set_text(s_lbl_pwm, "PWM   0.0 %");
  lv_obj_align(s_lbl_pwm, LV_ALIGN_TOP_MID, 0, 58);

  s_bar_pwm = lv_bar_create(scr);
  lv_obj_set_size(s_bar_pwm, 260, 18);
  lv_bar_set_range(s_bar_pwm, 0, 1000);
  lv_bar_set_value(s_bar_pwm, 0, LV_ANIM_OFF);
  lv_obj_align(s_bar_pwm, LV_ALIGN_CENTER, 0, -10);

  s_lbl_raw = lv_label_create(scr);
  lv_obj_set_style_text_color(s_lbl_raw, lv_color_hex(0x888888), 0);
  lv_obj_set_style_text_font(s_lbl_raw, &lv_font_montserrat_14, 0);
  lv_label_set_text(s_lbl_raw, "RAW  0 / 4095");
  lv_obj_align(s_lbl_raw, LV_ALIGN_BOTTOM_MID, 0, -60);

  s_lbl_v = lv_label_create(scr);
  lv_obj_set_style_text_color(s_lbl_v, lv_color_white(), 0);
  lv_obj_set_style_text_font(s_lbl_v, &lv_font_montserrat_28, 0);
  lv_label_set_text(s_lbl_v, "V    0.000 V");
  lv_obj_align(s_lbl_v, LV_ALIGN_BOTTOM_MID, 0, -30);

  s_initialized = 1u;
}

void UiMainScreen_Refresh(uint16_t duty_x10,
                          uint32_t voltage_mv,
                          uint16_t adc_raw,
                          const ina_monitor_snapshot_t *ina)
{
  if (!s_initialized || ina == NULL) {
    return;
  }

  if (ina->valid) {
    lv_label_set_text_fmt(s_lbl_ina_vi, "INA: %u.%02u V / %ld mA",
                          (unsigned)(ina->bus_v_mv / 1000u),
                          (unsigned)((ina->bus_v_mv % 1000u) / 10u),
                          (long)ina->current_ma);
    lv_label_set_text_fmt(s_lbl_ina_p, "PWR: %u.%02u W",
                          (unsigned)(ina->power_mw / 1000u),
                          (unsigned)((ina->power_mw % 1000u) / 10u));
  } else {
    lv_label_set_text(s_lbl_ina_vi, "INA:  --.-- V / --- mA");
    lv_label_set_text(s_lbl_ina_p, "PWR:  --.-- W");
  }

  lv_label_set_text_fmt(s_lbl_pwm, "PWM  %u.%u %%",
                        (unsigned)(duty_x10 / 10u),
                        (unsigned)(duty_x10 % 10u));
  lv_bar_set_value(s_bar_pwm, duty_x10, LV_ANIM_OFF);

  lv_label_set_text_fmt(s_lbl_raw, "RAW  %u / 4095", (unsigned)adc_raw);
  lv_label_set_text_fmt(s_lbl_v, "V   %u.%03u V",
                        (unsigned)(voltage_mv / 1000u),
                        (unsigned)(voltage_mv % 1000u));
}
