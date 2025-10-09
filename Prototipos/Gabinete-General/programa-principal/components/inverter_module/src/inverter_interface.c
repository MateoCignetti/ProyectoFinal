#include "inverter_interface.h"

#include <sys/lock.h>
#include "esp_log.h"
#include "lvgl.h"
#include "module_manager.h"
#include "inverter_ui.h"
#include "ui_config.h"
// REVISAR TEMA DE ARCHIVOS A INCLUIR

static const char* TAG = "Inverter Interface";

// Mutex for LVGL API calls - defined in inverter_ui_config.c
extern _lock_t lvgl_api_lock;

// Task handle for cleanup
static TaskHandle_t xTaskUpdateGroups_handle = NULL;

// Shutdown flag for graceful task termination
static volatile bool shutdown_requested = false;

// State for screen management
typedef enum {
    SCREEN_1,
    SCREEN_COUNT
} screen_state_t;

static lv_group_t *groups[SCREEN_COUNT];    // Array of LVGL groups for each screen
static lv_group_t *current_group; // Current LVGL group
static screen_state_t current_screen = SCREEN_1;    // Current screen state

void start_inverter_interface(){
    // Initialize shutdown flag
    shutdown_requested = false;

    _lock_acquire(&lvgl_api_lock);
    inverter_ui_init();
    _lock_release(&lvgl_api_lock);
}

void stop_inverter_interface(){
    
    _lock_acquire(&lvgl_api_lock);

    // CRITICAL FIX: Create and load a blank screen BEFORE destroying UI
    // This gives LVGL something safe to render during cleanup and after
    lv_obj_t *blank_screen = lv_obj_create(NULL);
    lv_screen_load(blank_screen);

    // NOW safe to destroy the old UI
    inverter_ui_destroy();

    // DON'T delete the blank screen - leave it loaded for LVGL to render
    // It will be cleaned up when the next module loads its UI or on system shutdown

    _lock_release(&lvgl_api_lock);

    ESP_LOGI(TAG, "Buck interface stopped, blank screen loaded");
}