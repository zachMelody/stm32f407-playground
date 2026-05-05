/**
 * @file lv_port_indev.c
 * LVGL 9.2 input device - encoder framework placeholder.
 * Actual encoder driver will be connected later.
 */

#include "lv_port_indev.h"

static void encoder_read(lv_indev_t * indev, lv_indev_data_t * data);

void lv_port_indev_init(void)
{
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev, encoder_read);
}

static void encoder_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    (void)indev;
    data->enc_diff = 0;
    data->state    = LV_INDEV_STATE_RELEASED;
}
