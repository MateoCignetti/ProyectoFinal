#include "module_manager.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "dimmer_control.h"
#include "gpio_definition.h"
#include "esp_log.h"


static const char* MANAGER_TAG = "Module Manager"; // Module name for logging
static adc_oneshot_unit_handle_t adc_unit_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;
TaskHandle_t xTaskModuleManagerUpdate_handle = NULL; // Task handle for the module state update task

static module_state_t module_manager_state = INIT; // Initial state of the module
SemaphoreHandle_t xModuleManagerMutex = NULL; // Mutex for protecting module state access

// Private function declarations
static void vTaskModuleManagerUpdate(void *pvParameters);
static void create_module_manager_tasks(void);
static const module_t* identify_module(void);
static void configure_adc(void);
//

static const module_ident_t module_ident_list[] = {
    // Add module identification entries here
    // Example:
    // { .module_ident_mv_min = 1000, .module_ident_mv_max = 2000, .module = &my_module },
    {1500, 1700, &dimmer_module},

    {0, 0, NULL} // Sentinel value to mark the end of the list
};

// Function to safely read module state
module_manager_state_t get_module_manager_state() {
    module_manager_state_t state;
    if (xModuleManagerMutex != NULL && xSemaphoreTake(xModuleManagerMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = module_manager_state;
        xSemaphoreGive(xModuleManagerMutex);
    } else {
        state = FAULT; // Return FAULT if we can't get the mutex
    }
    return state;
}

// Function to safely set module state
void set_module_manager_state(module_manager_state_t new_state) {
    if (xModuleManagerMutex != NULL && xSemaphoreTake(xModuleManagerMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        module_manager_state = new_state;
        xSemaphoreGive(xModuleManagerMutex);
    }
}

//

void configure_adc(){

    if(adc_unit_handle != NULL){
        return; // ADC already configured
    }

    adc_oneshot_unit_init_cfg_t adc_init_cfg = {
        .unit_id = ADC_MODULE_IDENT_UNIT,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    adc_oneshot_new_unit(&adc_init_cfg, &adc_unit_handle);
    
    adc_oneshot_chan_cfg_t adc1_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(adc_unit_handle, ADC_MODULE_IDENT_CHANNEL, &adc1_config);
    
    adc_cali_curve_fitting_config_t adc_cali_config = {
        .unit_id = ADC_MODULE_IDENT_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_cali_create_scheme_curve_fitting(&adc_cali_config, &adc_cali_handle);
}


static void create_module_manager_tasks(void){
    // Create mutex for module manager state
    xModuleManagerMutex = xSemaphoreCreateMutex();
    if (xModuleManagerMutex == NULL) {
        ESP_LOGE(MANAGER_TAG, "Failed to create module manager state mutex");
        return;
    }
    
    // Create the task to update the module state
    BaseType_t xReturned = xTaskCreate(vTaskModuleManagerUpdate,
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

void module_manager_init(void){
    create_module_manager_tasks(); // Create the task to update the module state
}


static const module_t* identify_module() {
    int module_ident_mv = 0;
    adc_oneshot_get_calibrated_result(adc_unit_handle, adc_cali_handle, ADC_MODULE_IDENT_CHANNEL, &module_ident_mv);

    for (int i = 0; module_ident_list[i].module != NULL; i++) {
        if (module_ident_mv >= module_ident_list[i].module_ident_mv_min && module_ident_mv <= module_ident_list[i].module_ident_mv_max) {
            return module_ident_list[i].module; // Return the identified module
        }
    }

    ESP_LOGW(MANAGER_TAG, "No module identified for voltage: %d mV", module_ident_mv); // Log warning if no module is identified
    return NULL; // No module identified
}

static void vTaskModuleManagerUpdate(void *pvParameters) {
    while (true) {

        if(get_module_manager_state() == MANAGER_INIT) {
            ESP_LOGI(MANAGER_TAG, "Module Manager is initializing...");
            configure_adc();
            set_module_manager_state(MANAGER_READY); // Move to READY state after initialization
        }

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Wait for notification to update module state
        
        // Get current state and update if changed
        module_manager_state_t current_state = get_module_manager_state();
        
        switch (current_state){
            case MANAGER_READY:
                const module_t* identified_module = identify_module();
                if (identified_module != NULL) {
                    ESP_LOGI(MANAGER_TAG, "Identified module: %s", identified_module->name);
                    if (identified_module->start_function != NULL) {
                        identified_module->start_function(); // Start the identified module
                        set_module_manager_state(MANAGER_RUNNING); // Update state to RUNNING
                    } else {
                        ESP_LOGE(MANAGER_TAG, "No start function defined for module: %s", identified_module->name);
                        set_module_manager_state(MANAGER_FAULT); // Set to FAULT if no start function
                    }
                }
                
                break;
            case MANAGER_RUNNING:
                // Monitor running module or handle stop requests
                break;
            case MANAGER_STOPPING:
                // Handle stopping the module
                break;

            case MANAGER_FAULT:
                ESP_LOGE(MANAGER_TAG, "Module Manager is in FAULT state!");
                // Handle fault state tasks here
                break;

            default:
                break;
            }

    }
}

