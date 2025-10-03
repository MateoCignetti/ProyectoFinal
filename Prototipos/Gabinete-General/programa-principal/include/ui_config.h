#ifndef UI_CONFIG_H
#define UI_CONFIG_H

#include <sys/lock.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lvgl.h"

extern lv_indev_t *indev_encoder; // Input device for LVGL (encoder)
extern _lock_t lvgl_api_lock;     // Mutex for LVGL API calls

void setup_user_interface();

#endif