#include "inverter_interface.h"

#include <sys/lock.h>
#include "esp_log.h"
#include "lvgl.h"
#include "module_manager.h"
#include "inverter_ui.h"
#include "ui_config.h"
#include "idle_screen.h"
// REVISAR TEMA DE ARCHIVOS A INCLUIR

static const char* TAG = "Inverter Interface";

// Mutex for LVGL API calls - defined in inverter_ui_config.c
extern _lock_t lvgl_api_lock;

void start_inverter_interface(){

    _lock_acquire(&lvgl_api_lock);
    // CRITICAL: Initialize new UI BEFORE destroying old screen
    // This prevents deleting screens while their animations are still active
    inverter_ui_init();
    stop_idle_screen();  // Now safe to destroy idle screen
    _lock_release(&lvgl_api_lock);
}

void stop_inverter_interface(){
    
    _lock_acquire(&lvgl_api_lock);

    // CRITICAL: Create new screen BEFORE destroying old UI
    // This prevents deleting screens while their animations are still active
    start_idle_screen();

    // NOW safe to destroy the old UI
    inverter_ui_destroy();

    // Clean up the initial actions object to prevent memory leak
    if(inverter_ui____initial_actions0) {
        lv_obj_del(inverter_ui____initial_actions0);
        inverter_ui____initial_actions0 = NULL;
    }

    _lock_release(&lvgl_api_lock);

    ESP_LOGI(TAG, "Inverter interface stopped, blank screen loaded");
}