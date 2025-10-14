#include "idle_screen.h"

#include <sys/lock.h>
#include "esp_log.h"
#include "lvgl.h"
#include "ui_config.h"
#include "idle_ui.h"
// REVISAR TEMA DE ARCHIVOS A INCLUIR

static const char* TAG = "Idle screen";

// Mutex for LVGL API calls - defined in inverter_ui_config.c
extern _lock_t lvgl_api_lock;

// Keep track of whether idle screen is created
static bool idle_screen_created = false;

void start_idle_screen(){

    idle_ui_init();
    idle_screen_created = true;

    ESP_LOGI(TAG, "Idle screen started");
}


void stop_idle_screen(){
    
    // Only destroy if it was created
    if (idle_screen_created) {
        // Let LVGL finish any pending operations on the idle screen
        // by giving the timer handler a chance to run
        lv_refr_now(NULL);
        
        idle_ui_destroy();
        
        // Clean up the initial actions object to prevent memory leak
        if(idle_ui____initial_actions0) {
            lv_obj_del(idle_ui____initial_actions0);
            idle_ui____initial_actions0 = NULL;
        }
        idle_screen_created = false;
        
        ESP_LOGI(TAG, "Idle screen stopped and destroyed");
    }
}