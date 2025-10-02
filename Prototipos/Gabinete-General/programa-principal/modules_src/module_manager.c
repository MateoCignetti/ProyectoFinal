#include "module_manager.h"
#include "module_connection.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "dimmer_control.h"
#include "gpio_definition.h"
#include "esp_log.h"
#include "driver/gpio.h"


static const char* MANAGER_TAG = "Module Manager"; // Module name for logging
static adc_oneshot_unit_handle_t adc1_unit_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;
TaskHandle_t xTaskModuleManagerUpdate_handle = NULL; // Task handle for the module state update task

static module_manager_state_t module_manager_state = MANAGER_INIT;
SemaphoreHandle_t xModuleManagerMutex = NULL;

// Private function declarations
static void vTaskModuleManagerUpdate(void *pvParameters);
static void create_module_manager_tasks(void);
static const module_t* identify_module(void);
static void setup_ident_adc(void);
static void delete_ident_adc(void);
static void handle_module_connected(bool connected);
//

module_t inverter_module = {
    .name = "Inverter Module", // Name of the module
    .start_function = NULL, // Function pointer to start the module
    .stop_function = NULL, // Function pointer to stop the module
};

module_t buck_module = {
    .name = "Buck Converter Module", // Name of the module
    .start_function = NULL, // Function pointer to start the module
    .stop_function = NULL, // Function pointer to stop the module
};

static const module_ident_t module_ident_list[] = {
    // Add module identification entries here
    // Example:
    // { .module_ident_mv_min = 1000, .module_ident_mv_max = 2000, .module = &my_module },
    {1600, 1700, &inverter_module},
    {1900, 2000, &buck_module},
    
    {500, 600, &dimmer_module},

    {0, 0, NULL} // Sentinel value to mark the end of the list
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

// Function to safely set module state
void set_module_manager_state(module_manager_state_t new_state) {
    if (xModuleManagerMutex != NULL && xSemaphoreTake(xModuleManagerMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        module_manager_state = new_state;
        xSemaphoreGive(xModuleManagerMutex);
    }
}

//

void setup_ident_adc(){

    if(adc1_unit_handle == NULL && adc1_cali_handle == NULL){
        // First, ensure the GPIO pin is properly configured as analog input
        // This prevents the pin from being driven as output
        gpio_config_t ident_io_conf = {
            .pin_bit_mask = (1ULL << PIN_MODULE_IDENT),
            .mode = GPIO_MODE_DISABLE,  // Disable digital I/O
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&ident_io_conf);

        adc_oneshot_unit_init_cfg_t adc1_init_cfg = {
            .unit_id = ADC_MODULE_IDENT_UNIT,
            .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        adc_oneshot_new_unit(&adc1_init_cfg, &adc1_unit_handle);
        
        adc_oneshot_chan_cfg_t adc1_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        adc_oneshot_config_channel(adc1_unit_handle, ADC_MODULE_IDENT_CHANNEL, &adc1_config);
        
        adc_cali_curve_fitting_config_t adc1_cali_config = {
            .unit_id = ADC_MODULE_IDENT_UNIT,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        adc_cali_create_scheme_curve_fitting(&adc1_cali_config, &adc1_cali_handle);

    } else{
        ESP_LOGE(MANAGER_TAG, "ADC already configured for module identification");
        set_module_manager_state(MANAGER_FAULT);
    }
}

void delete_ident_adc(){
    if(adc1_cali_handle != NULL){
        adc_cali_delete_scheme_curve_fitting(adc1_cali_handle);
        adc1_cali_handle = NULL;
    }
    if(adc1_unit_handle != NULL){
        adc_oneshot_del_unit(adc1_unit_handle);
        adc1_unit_handle = NULL;
    }
    
    // CRITICAL: Reset the GPIO to high-impedance input to prevent voltage injection
    // This ensures the module identification pin is not driven by the microcontroller
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_MODULE_IDENT),
        .mode = GPIO_MODE_INPUT,  // Set as input (high impedance)
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    ESP_LOGI(MANAGER_TAG, "ADC deleted and GPIO %d set to high-impedance input", PIN_MODULE_IDENT);
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
    create_module_manager_tasks();
    
    // Initialize module connection system
    initialize_module_connection();
    
    // Register callback for connection state changes
    register_connection_callback(handle_module_connected);
}


static const module_t* identify_module() {
    int module_ident_mv = 0;
    int module_ident_avg = 0;
    
    // Take 5 samples and average them
    for (int sample = 0; sample < 50; sample++) {
        adc_oneshot_get_calibrated_result(adc1_unit_handle, adc1_cali_handle, ADC_MODULE_IDENT_CHANNEL, &module_ident_mv);
        ESP_LOGI(MANAGER_TAG, "Module identification sample %d: %d mV", sample + 1, module_ident_mv);
        module_ident_avg += module_ident_mv;
    }
    module_ident_avg /= 50;
    module_ident_mv = module_ident_avg;
    ESP_LOGI(MANAGER_TAG, "Average module identification voltage: %d mV", module_ident_mv);

    for (int i = 0; module_ident_list[i].module != NULL; i++) {
        if (module_ident_mv >= module_ident_list[i].module_ident_mv_min && module_ident_mv <= module_ident_list[i].module_ident_mv_max) {
            return module_ident_list[i].module;
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
                    const module_t* identified_module = identify_module();
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

