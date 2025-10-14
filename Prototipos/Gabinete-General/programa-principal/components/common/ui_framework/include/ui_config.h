/**
 * @file ui_config.h
 * @brief UI Framework - LVGL configuration and display management
 * 
 * This component provides:
 * - LVGL initialization and configuration
 * - Display (LCD) setup and management  
 * - Input device (encoder) integration
 * - Common UI utilities
 */

#ifndef UI_CONFIG_H
#define UI_CONFIG_H

#include <sys/lock.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LVGL input device (encoder)
 * 
 * This is the rotary encoder configured as an LVGL input device.
 * Modules can use this to interact with LVGL groups.
 */
extern lv_indev_t *indev_encoder;

/**
 * @brief LVGL API lock
 * 
 * Use this lock when calling LVGL APIs from outside the LVGL task.
 * This prevents race conditions and ensures thread safety.
 * 
 * Example:
 * @code
 * __lock_acquire(lvgl_api_lock);
 * lv_obj_set_x(obj, 10);
 * __lock_release(lvgl_api_lock);
 * @endcode
 */
extern _lock_t lvgl_api_lock;

/**
 * @brief Initialize the user interface
 * 
 * This function:
 * - Initializes the LCD display
 * - Sets up LVGL
 * - Configures the rotary encoder as input device
 * - Creates the LVGL task
 * 
 * Call this once during application startup.
 */
void setup_user_interface(void);

/**
 * @brief Get the LVGL display handle
 * 
 * @return LVGL display pointer
 */
lv_display_t* get_lvgl_display(void);

/**
 * @brief Get the encoder input device
 * 
 * @return LVGL input device pointer
 */
lv_indev_t* get_encoder_indev(void);

/**
 * @brief Lock LVGL API for thread-safe access
 * 
 * Use this when calling LVGL functions from other tasks.
 * Always pair with ui_unlock().
 */
void ui_lock(void);

/**
 * @brief Unlock LVGL API after thread-safe access
 * 
 * Always call this after ui_lock() to release the lock.
 */
void ui_unlock(void);

#ifdef __cplusplus
}
#endif

#endif // UI_CONFIG_H
