#include "module_connection.h"

#include "module_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "gpio_definition.h"
#include "esp_log.h"

#define DEFAULT_CONNECTION_STATE CONNECTION_FAULT
#define CONNECTION_STATE_MUTEX_TIMEOUT_MS 100
#define CHECK_HOTPLUG_PERIOD_MS 50
#define STABLE_CONNECTION_TIME_MS 1000

static const char* CONNECTION_TAG = "Module Connection"; // Module name for logging
static SemaphoreHandle_t xConnectionStateMutex = NULL;
static TaskHandle_t xTaskUpdateConnectionState_handle = NULL;

extern TaskHandle_t xTaskModuleManagerUpdate_handle;

module_connection_state_t module_connection_state = DEFAULT_CONNECTION_STATE;

module_connection_state_t get_connection_state(void);
void set_connection_state(module_connection_state_t new_state);
void vTaskUpdateConnectionState(void *pvParameters);
void vTaskCheckHotplugs(void *pvParameters);
void setup_connnection_gpios(void);
void create_module_connection_tasks(void);
void initialize_module_connection(void);

module_connection_state_t get_connection_state(void){
    if(xConnectionStateMutex != NULL && xSemaphoreTake(xConnectionStateMutex, CONNECTION_STATE_MUTEX_TIMEOUT_MS) == pdTRUE){
        module_connection_state_t current_state = module_connection_state;
        xSemaphoreGive(xConnectionStateMutex);
        return current_state;
    } else{
        ESP_LOGE(CONNECTION_TAG, "Failed to get connection state mutex");
        return CONNECTION_FAULT;
    }
}

void set_connection_state(module_connection_state_t new_state){
    if(xConnectionStateMutex != NULL && xSemaphoreTake(xConnectionStateMutex, CONNECTION_STATE_MUTEX_TIMEOUT_MS) == pdTRUE){
        module_connection_state = new_state;
        xSemaphoreGive(xConnectionStateMutex);

        if(xTaskUpdateConnectionState_handle != NULL) {
            xTaskNotifyGive(xTaskUpdateConnectionState_handle);
        }
    } else{
        ESP_LOGE(CONNECTION_TAG, "Failed to get connection state mutex when setting new state!");
    }
}

void vTaskUpdateConnectionState(void *pvParameters){
    module_connection_state_t current_state = DEFAULT_CONNECTION_STATE;
    while (true) {
        // Wait for notification 
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        current_state = get_connection_state();

        switch (current_state){
            case CONNECTION_INIT:
                setup_connnection_gpios();
                break;

            case CONNECTION_DISCONNECTED:
                // Notify module manager about disconnection with task notifications, not the callback, with the value
                
                if(get_module_manager_state() == MANAGER_RUNNING){
                    ESP_LOGI(CONNECTION_TAG, "Notifying Module Manager about disconnection");
                    set_module_manager_state(MANAGER_STOPPING);
                } else{
                    ESP_LOGW(CONNECTION_TAG, "Module disconnected but Module Manager not in RUNNING state");
                }
                break;
            
            case CONNECTION_CONNECTED:
                ESP_LOGI(CONNECTION_TAG, "Notifying Module Manager about connection");
                if(get_module_manager_state() == MANAGER_READY){
                    set_module_manager_state(MANAGER_STARTING); // Trigger identification
                } else{
                    ESP_LOGW(CONNECTION_TAG, "Module connected but Module Manager not in READY state");
                }
                break;

            case CONNECTION_FAULT:
                /* code */
                break;
            
        }
        
    }
}

void vTaskCheckHotplugs(void *pvParameters){
    int hp_signal_level = 1;
    int hp_power_level = 1;
    uint16_t hp_stable_count = 0;
    uint16_t hp_stable_count_threshold = STABLE_CONNECTION_TIME_MS / CHECK_HOTPLUG_PERIOD_MS;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xCheckPeriod = pdMS_TO_TICKS(CHECK_HOTPLUG_PERIOD_MS);
    
    module_connection_state_t current_state = DEFAULT_CONNECTION_STATE;

    while(true){
        hp_signal_level = gpio_get_level(PIN_HP_SIGNAL);
        hp_power_level = gpio_get_level(PIN_HP_POWER);

        current_state = get_connection_state();
        if(hp_signal_level == 0 && hp_power_level == 0){
            hp_stable_count++;

            if(hp_stable_count >= hp_stable_count_threshold){
                hp_stable_count = hp_stable_count_threshold; // Cap the count to avoid overflow
                if(current_state != CONNECTION_CONNECTED){
                    ESP_LOGI(CONNECTION_TAG, "Stable connection detected: Module Connected");
                    set_connection_state(CONNECTION_CONNECTED);
                }
            }
        } else {
            hp_stable_count = 0;
            
            if(current_state != CONNECTION_DISCONNECTED){
                ESP_LOGI(CONNECTION_TAG, "Module Disconnected");
                set_connection_state(CONNECTION_DISCONNECTED);
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xCheckPeriod);
    }
}

void setup_connnection_gpios(){
    // Configuration for the relay to change the load
    gpio_config_t hp_gpios_conf = {
        .pin_bit_mask = (1ULL << PIN_HP_SIGNAL) | (1ULL << PIN_HP_POWER),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&hp_gpios_conf));
}

void create_module_connection_tasks(void){
    // Create mutex for connection state
    xConnectionStateMutex = xSemaphoreCreateMutex();
    if (xConnectionStateMutex == NULL) {
        ESP_LOGE(CONNECTION_TAG, "Failed to create connection state mutex");
        return;
    }

    // Create task to update connection state
    BaseType_t task_created = xTaskCreate(
        vTaskUpdateConnectionState,
        "UpdateConnectionState",
        2048,
        NULL,
        tskIDLE_PRIORITY + 2,
        &xTaskUpdateConnectionState_handle
    );

    if (task_created != pdPASS) {
        ESP_LOGE(CONNECTION_TAG, "Failed to create UpdateConnectionState task");
        return;
    }

    // Create task to check hotplug status
    task_created = xTaskCreate(
        vTaskCheckHotplugs,
        "CheckHotplugs",
        2048,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    if (task_created != pdPASS) {
        ESP_LOGE(CONNECTION_TAG, "Failed to create CheckHotplugs task");
        return;
    }
}

void initialize_module_connection(void){

    // Create module connection tasks
    create_module_connection_tasks();
    set_connection_state(CONNECTION_INIT);
}