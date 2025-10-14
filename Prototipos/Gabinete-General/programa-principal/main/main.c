#include "esp_log.h"
#include "module_manager.h"
#include "ui_config.h"

static const char* TAG = "Main";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting programa-principal...");
    
    setup_user_interface(); // Initialize user interface
    // Initialize module manager (this will also initialize module connection)
    module_manager_init();
    
    ESP_LOGI(TAG, "Application initialized successfully");
    
    // app_main() can return - other tasks will continue running
    // The module_manager and module_connection tasks handle everything from here
}