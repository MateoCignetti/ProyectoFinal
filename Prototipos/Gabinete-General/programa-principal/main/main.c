#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "module.h"
#include "module_manager.h"

static const char* TAG = "Main";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting programa-principal...");
    
    // Initialize module manager
    module_manager_init();
    
    // Initialize module state management
    initialize_module_state();
    module_manager_init();
    
    ESP_LOGI(TAG, "Application initialized successfully");
    
    // Main application loop - let FreeRTOS handle task scheduling
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay 1 second
    }
}