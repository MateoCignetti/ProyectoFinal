#ifndef UI_CONFIG_H
#define UI_CONFIG_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lvgl.h"

extern lv_indev_t *indev_encoder; // Input device for LVGL (encoder)

void setup_user_interface();

#endif