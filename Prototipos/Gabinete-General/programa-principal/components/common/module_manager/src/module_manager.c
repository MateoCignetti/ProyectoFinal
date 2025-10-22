#include "module_manager.h"

#include "module_connection.h"
#include "module_registry.h" // Central registry for all modules

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "gpio_definition.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define IDENT_SAMPLES_N 50
#define IDENT_SAMPLES_PERIOD_MS 10


static const char* MANAGER_TAG = "Module Manager"; // Module name for logging
static adc_oneshot_unit_handle_t adc1_unit_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;
static TaskHandle_t xTaskModuleManagerUpdate_handle = NULL; // Task handle for the module state update task
static TaskHandle_t xTaskAdcSampling_handle = NULL; // Task handle for ADC sampling

static module_manager_state_t module_manager_state = MANAGER_INIT;
static SemaphoreHandle_t xModuleManagerMutex = NULL;
static QueueHandle_t xAdcResultQueue = NULL; // Queue to receive ADC sampling results


// Public function declarations
module_manager_state_t get_module_manager_state(void);
void module_manager_init(void);
//

// Private function declarations
static void vTaskModuleManagerUpdate(void *pvParameters);
static void vTaskAdcSampling(void *pvParameters);
static void create_module_manager_tasks(void);
static const module_t* identify_module(int module_ident_mv);
static void setup_ident_adc(void);
static void delete_ident_adc(void);
static void handle_module_connected(bool connected);
static void set_module_manager_state(module_manager_state_t new_state);
//

/**
 * Module Registry
 * 
 * The list of modules is defined in module_registry.h using MODULE_REGISTRY_LIST.
 * To add a new module, edit module_registry.h
 */
static const module_t* module_list[] = {
    MODULE_REGISTRY_LIST
};

// Function to safely read module state
module_manager_state_t get_module_manager_state() {
    module_manager_state_t state;
    if (xModuleManagerMutex != NULL && xSemaphoreTake(xModuleManagerMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = module_manager_state;
        xSemaphoreGive(xModuleManagerMutex);
    } else {
        ESP_LOGE(MANAGER_TAG, "Failed to get module manager state mutex");
        state = MANAGER_FAULT; // Return FAULT if we can't get the mute
    }
    return state;
}

// Public Functions
void module_manager_init(void){
    create_module_manager_tasks();
    
    // Initialize module connection system
    initialize_module_connection();
    
    // Register callback for connection state changes
    register_connection_callback(handle_module_connected);
}
//

// Private Functions

// Function to safely set module state
static void set_module_manager_state(module_manager_state_t new_state) {
    if (xModuleManagerMutex != NULL && xSemaphoreTake(xModuleManagerMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        module_manager_state = new_state;
        xSemaphoreGive(xModuleManagerMutex);
    }
}

static void setup_ident_adc(){

    if(adc1_unit_handle == NULL && adc1_cali_handle == NULL){
        // Configure GPIO as INPUT just in case
        gpio_config_t ident_io_conf = {
            .pin_bit_mask = (1ULL << PIN_MODULE_IDENT),
            .mode = GPIO_MODE_INPUT,  // Disable digital I/O
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&ident_io_conf));

        adc_oneshot_unit_init_cfg_t adc1_init_cfg = {
            .unit_id = ADC_MODULE_IDENT_UNIT,
            .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc1_init_cfg, &adc1_unit_handle));
        
        adc_oneshot_chan_cfg_t adc1_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_unit_handle, ADC_MODULE_IDENT_CHANNEL, &adc1_config));
        
        adc_cali_curve_fitting_config_t adc1_cali_config = {
            .unit_id = ADC_MODULE_IDENT_UNIT,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&adc1_cali_config, &adc1_cali_handle));

        ESP_LOGI(MANAGER_TAG, "ADC setup on GPIO %d", PIN_MODULE_IDENT);

    } else{
        ESP_LOGE(MANAGER_TAG, "ADC already configured for module identification");
        set_module_manager_state(MANAGER_FAULT);
    }
}

static void delete_ident_adc(){
    if(adc1_cali_handle != NULL){
        ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(adc1_cali_handle));
        adc1_cali_handle = NULL;
    }
    if(adc1_unit_handle != NULL){
        ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_unit_handle));
        adc1_unit_handle = NULL;
    }
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_MODULE_IDENT),
        .mode = GPIO_MODE_DISABLE, 
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    ESP_LOGI(MANAGER_TAG, "ADC deleted and GPIO %d disabled", PIN_MODULE_IDENT);
}

static void create_module_manager_tasks(void){
    // Create mutex for module manager state
    xModuleManagerMutex = xSemaphoreCreateMutex();
    if (xModuleManagerMutex == NULL) {
        ESP_LOGE(MANAGER_TAG, "Failed to create module manager state mutex");
        return;
    }
    
    // Create queue for ADC results (1 item, holds int voltage in mV)
    xAdcResultQueue = xQueueCreate(1, sizeof(int));
    if (xAdcResultQueue == NULL) {
        ESP_LOGE(MANAGER_TAG, "Failed to create ADC result queue");
        return;
    }
    
    // Create the ADC sampling task
    BaseType_t xReturned = xTaskCreate(vTaskAdcSampling,
                                       "ADC Sampling Task",
                                       4096,
                                       NULL,
                                       4,  // Lower priority than manager task
                                       &xTaskAdcSampling_handle);
    if (xReturned != pdPASS) {
        ESP_LOGE(MANAGER_TAG, "Failed to create ADC Sampling Task");
        set_module_manager_state(MANAGER_FAULT);
        return;
    }
    
    // Create the task to update the module state
    xReturned = xTaskCreate(vTaskModuleManagerUpdate,
                           "Module Manager Update Task",
                           4096,  // Increased stack size
                           NULL,
                           5,
                           &xTaskModuleManagerUpdate_handle);
    if (xReturned != pdPASS) {
        ESP_LOGE(MANAGER_TAG, "Failed to create Module Manager Update Task");
        set_module_manager_state(MANAGER_FAULT); // Set module state to FAULT if task creation failed
    }
}

/**
 * ADC Sampling Task
 * This task runs independently and performs ADC sampling when requested.
 * Results are sent back via queue to avoid blocking the main manager task.
 */
static void vTaskAdcSampling(void *pvParameters) {
    while (true) {
        // Wait for notification to start sampling
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Check if we're still supposed to be sampling (module could have disconnected)
        if (get_connection_state() != CONNECTION_CONNECTED) {
            ESP_LOGW(MANAGER_TAG, "Module disconnected before ADC sampling - aborting");
            continue;
        }
        
        int module_ident_mv = 0;
        int module_ident_avg = 0;
        bool sampling_completed = true;
        int last_sample_checked = 0;
        
        // Take 50 samples and average them
        for (int sample = 0; sample < IDENT_SAMPLES_N; sample++) {
            adc_oneshot_get_calibrated_result(adc1_unit_handle, adc1_cali_handle, ADC_MODULE_IDENT_CHANNEL, &module_ident_mv);

            module_ident_avg += module_ident_mv;

            // Delay between samples to allow other tasks to run
            vTaskDelay(pdMS_TO_TICKS(IDENT_SAMPLES_PERIOD_MS));
            
            // Check connection state only periodically (every 10 samples) to reduce mutex contention
            // Do NOT log inside the loop - it causes spinlock issues
            if ((sample + 1) % 10 == 0) {
                if (get_connection_state() != CONNECTION_CONNECTED) {
                    sampling_completed = false;
                    last_sample_checked = sample + 1;
                    break;
                }
            }
        }
        
        // All logging done AFTER the loop completes
        if (!sampling_completed) {
            ESP_LOGW(MANAGER_TAG, "Module disconnected during sampling after sample %d - aborting", last_sample_checked);
        } else {
            ESP_LOGI(MANAGER_TAG, "Completed %d samples", IDENT_SAMPLES_N);
        }
        
        // Always send a result to unblock the waiting task
        if (sampling_completed) {
            module_ident_avg /= IDENT_SAMPLES_N;
            module_ident_mv = module_ident_avg;
            ESP_LOGI(MANAGER_TAG, "Average module identification voltage: %d mV", module_ident_mv);
        } else {
            // Send sentinel value (-1) to indicate sampling was aborted
            module_ident_mv = -1;
            ESP_LOGW(MANAGER_TAG, "Sending failure indicator to manager task");
        }
        
        // Send result to queue (overwrite if full)
        xQueueOverwrite(xAdcResultQueue, &module_ident_mv);
    }
}

static const module_t* identify_module(int module_ident_mv) {
    for (int i = 0; module_list[i] != NULL; i++) {
        if (module_ident_mv >= module_list[i]->ident_mv_min && module_ident_mv <= module_list[i]->ident_mv_max) {
            return module_list[i];
        }
    }

    ESP_LOGW(MANAGER_TAG, "No module identified for voltage: %d mV", module_ident_mv);
    return NULL;
}

static void handle_module_connected(bool connected) {
    // This function is called when module connection state changes
    /*
    if (connected) {
        ESP_LOGI(MANAGER_TAG, "Module connection confirmed - triggering identification");
        xTaskNotifyGive(xTaskModuleManagerUpdate_handle);
    } else {
        ESP_LOGI(MANAGER_TAG, "Module disconnected - stopping current module");
        // Handle disconnection - stop current module if running
        if (get_module_manager_state() == MANAGER_RUNNING) {
            set_module_manager_state(MANAGER_STOPPING);
            xTaskNotifyGive(xTaskModuleManagerUpdate_handle);
        }
    }*/
    // Notify the module manager task of the connection state change
    xTaskNotifyGive(xTaskModuleManagerUpdate_handle);
}

static void vTaskModuleManagerUpdate(void *pvParameters) {
    const module_t* current_module = NULL;
    while (true) {
        if(get_module_manager_state() == MANAGER_INIT) {
            ESP_LOGI(MANAGER_TAG, "Module Manager is initializing...");
            setup_ident_adc();
            set_module_manager_state(MANAGER_READY);
        }

        // Wait for notification from connection callback
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        module_manager_state_t current_state = get_module_manager_state();
        
        // I don't know if this will work after module_running to module_stopping. Test.
        switch (current_state){
            case MANAGER_READY:
                // Only try to identify if module is actually connected
                if (get_connection_state() == CONNECTION_CONNECTED) {
                    // Request ADC sampling from the dedicated task
                    ESP_LOGI(MANAGER_TAG, "Starting ADC sampling for module identification");
                    xTaskNotifyGive(xTaskAdcSampling_handle);
                    
                    // Wait for ADC result (no timeout needed since sampling task always sends a value)
                    int module_ident_mv = 0;
                    if (xQueueReceive(xAdcResultQueue, &module_ident_mv, portMAX_DELAY) == pdTRUE) {
                        // Check for sentinel value indicating sampling failure
                        if (module_ident_mv < 0) {
                            ESP_LOGW(MANAGER_TAG, "ADC sampling was aborted (module disconnected)");
                            break;
                        }
                        
                        // Double-check module is still connected after sampling
                        if (get_connection_state() != CONNECTION_CONNECTED) {
                            ESP_LOGW(MANAGER_TAG, "Module disconnected after ADC sampling completed");
                            break;
                        }
                        
                        const module_t* identified_module = identify_module(module_ident_mv);
                        if (identified_module != NULL) {
                            ESP_LOGI(MANAGER_TAG, "Identified module: %s", identified_module->name);
                            if (identified_module->start_function != NULL) {
                                delete_ident_adc();
                                identified_module->start_function();
                                current_module = identified_module;
                                set_module_manager_state(MANAGER_RUNNING);
                            } else {
                                ESP_LOGE(MANAGER_TAG, "No start function defined for module: %s", identified_module->name);
                                set_module_manager_state(MANAGER_READY); //CHANGE LATER TO FAULT
                            }
                        } else {
                            ESP_LOGW(MANAGER_TAG, "Failed to identify module");
                            set_module_manager_state(MANAGER_READY); //CHANGE LATER TO FAULT
                        }
                    } else {
                        // This should never happen since we use portMAX_DELAY
                        ESP_LOGE(MANAGER_TAG, "Failed to receive ADC result from queue");
                        set_module_manager_state(MANAGER_FAULT);
                    }
                }
                break;
                
            case MANAGER_RUNNING:
                // Module is running - handle any state changes or disconnection
                if (get_connection_state() != CONNECTION_CONNECTED) {
                    ESP_LOGI(MANAGER_TAG, "Module disconnected while running - stopping");
                    set_module_manager_state(MANAGER_STOPPING);
                    if (current_module != NULL && current_module->stop_function != NULL) {
                        current_module->stop_function(); // Stop current module
                    }
                    current_module = NULL;
                    
                    setup_ident_adc(); // Re-setup ADC for identification
                    set_module_manager_state(MANAGER_READY);
                    ESP_LOGI(MANAGER_TAG, "Module stopped and ADC re-configured for identification");
                }
                break;
                
            case MANAGER_STOPPING:
                // Handle stopping the module
                ESP_LOGI(MANAGER_TAG, "Stopping module...");
                // TODO: Add stop function call if needed
                set_module_manager_state(MANAGER_READY);
                break;

            case MANAGER_FAULT:
                ESP_LOGE(MANAGER_TAG, "Module Manager is in FAULT state!");
                // Reset to ready state after some time or condition
                //set_module_manager_state(MANAGER_READY);
                break;

            default:
                break;
        }
    }
}

