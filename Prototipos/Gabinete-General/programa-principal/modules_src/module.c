#include "module.h"

#include "module_manager.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "gpio_definition.h"
#include "driver/gptimer.h"

#define ESP_INTR_FLAG_DEFAULT 0 // Default interrupt flag
//#define MODULE_CHECK_INTERVAL_MS 10

static const char* MODULE_TAG = "Module State"; // Module name for logging

static TaskHandle_t xTaskModuleStateUpdate_handle = NULL; // Task handle for the module state update task
extern TaskHandle_t xTaskModuleManagerUpdate_handle; // Task handle for the module manager update task
extern SemaphoreHandle_t xModuleManagerMutex; // Mutex for protecting module manager state access

SemaphoreHandle_t xModuleStateMutex = NULL; // Mutex for protecting module state access
static gptimer_handle_t debounce_timer = NULL; // Handle for the debounce timer

static bool gpio_isr_service_installed = false; // Flag to track if ISR service is installed
static module_state_t module_state = INIT; // Initial state of the module

static void hp_isr_handler(void* arg); // Forward declaration of the ISR handler

static void setup_ident_gpios(void);
static void delete_ident_gpios(void);
static void create_module_state_tasks(void);
static void vTaskModuleStateUpdate(void *pvParameters);
static void configure_timers(void);

// Public functions
void initialize_module_state(){
    create_module_state_tasks(); // Create the task to update the module state
    configure_timers(); // Configure the timers used for debouncing
}

// Function to safely read module state
module_state_t get_module_state() {
    module_state_t state;
    if (xModuleStateMutex != NULL && xSemaphoreTake(xModuleStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = module_state;
        xSemaphoreGive(xModuleStateMutex);
    } else {
        state = FAULT; // Return FAULT if we can't get the mutex
    }
    return state;
}

// Function to safely set module state
void set_module_state(module_state_t new_state) {
    if (xModuleStateMutex != NULL && xSemaphoreTake(xModuleStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        module_state = new_state;
        xSemaphoreGive(xModuleStateMutex);
    }
}
//

// Private functions

static bool debounce_timer_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    // Timer callback function to handle debounce timing
    // Notify the task to check the pin states after debounce period
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    module_manager_state_t current_module_manager_state = get_module_manager_state();
    if(current_module_manager_state == MANAGER_READY){
        // If the module manager is in READY state, notify the module state update task
        xTaskNotifyFromISR(xTaskModuleManagerUpdate_handle, 0, eSetBits, &xHigherPriorityTaskWoken);
    }
    return (xHigherPriorityTaskWoken == pdTRUE); // Return true if a higher priority task was woken
}

static void configure_timers(){

    // Configuration for both timers (counting up, and 1 us resolution)
    gptimer_config_t gptimer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000 * 1000, // 1 MHz resolution for debounce timing
    };
    // Apply the configuration to timer
    ESP_ERROR_CHECK(gptimer_new_timer(&gptimer_config, &debounce_timer)); 

    // Configure the alarm count and behaviour for the timer
    gptimer_alarm_config_t wait_alarm_config = {
        .alarm_count = 3 * 1000 * 1000, // Triggers the alarm event after 3s
        .flags.auto_reload_on_alarm = false,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(debounce_timer, &wait_alarm_config));

    // Configure callback functions for the timer on alarm event
    gptimer_event_callbacks_t wait_callbacks = {
        .on_alarm = debounce_timer_cb, 
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(debounce_timer, &wait_callbacks, NULL));


    // Enable timer. The timers are not started yet, but are now ready to be started
    ESP_ERROR_CHECK(gptimer_enable(debounce_timer));

}

static void setup_ident_gpios(){
    gpio_config_t hp_pin_conf = {
        .pin_bit_mask = (1ULL << PIN_HP_POWER) | (1ULL << PIN_HP_SIGNAL), // Configure both HP pins
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&hp_pin_conf)); // Apply the configuration

    // Only install ISR service once
    if (!gpio_isr_service_installed) {
        ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM)); // Install ISR service
        gpio_isr_service_installed = true;
    }
    
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_HP_POWER, hp_isr_handler, NULL)); // Add ISR handler for HP Power pin
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_HP_SIGNAL, hp_isr_handler, NULL)); // Add ISR handler for HP Signal pin
}

static void delete_ident_gpios(){
    // Only remove handlers if ISR service is installed
    if (gpio_isr_service_installed) {
        ESP_ERROR_CHECK(gpio_isr_handler_remove(PIN_HP_POWER)); // Remove the ISR handler for the HP Power pin
        ESP_ERROR_CHECK(gpio_isr_handler_remove(PIN_HP_SIGNAL)); // Remove the ISR handler for the HP Signal pin
        gpio_uninstall_isr_service(); // Uninstall the ISR service
        gpio_isr_service_installed = false; // Update the flag to reflect service is uninstalled
    }

    // Reset the GPIOs to their default state
    gpio_reset_pin(PIN_HP_POWER);
    gpio_reset_pin(PIN_HP_SIGNAL);
}

// Interrupt Service Routine (ISR) handler for HP Power and HP Signal pins
// Keep ISR simple - just notify the task to check state
static void IRAM_ATTR hp_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; // Variable to indicate if a higher priority task was woken
    
    // Simply notify the task that pins changed - let the task handle state logic
    xTaskNotifyFromISR(xTaskModuleStateUpdate_handle, 0, eSetBits, &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken); // Yield from ISR if a higher priority task was woken
}

void create_module_state_tasks() {
    // Create mutex for module state protection
    xModuleStateMutex = xSemaphoreCreateMutex();
    if (xModuleStateMutex == NULL) {
        ESP_LOGE(MODULE_TAG, "Failed to create module state mutex");
        return;
    }
    
    // Create the task to update the module state
    BaseType_t xReturned = xTaskCreate(vTaskModuleStateUpdate,
                                       "Module State Update Task",
                                       4096,  // Increased stack size
                                       NULL,
                                       5,
                                       &xTaskModuleStateUpdate_handle);
    if (xReturned != pdPASS) {
        ESP_LOGE(MODULE_TAG, "Failed to create Module State Update Task");
        set_module_state(FAULT); // Set module state to FAULT if task creation failed
    }
}

static void vTaskModuleStateUpdate(void *pvParameters) {
    while (true) {

        if(get_module_state() == INIT) {
            ESP_LOGI(MODULE_TAG, "Module is initializing...");
            setup_ident_gpios(); // Setup GPIOs for module identification
            set_module_state(DISCONNECTED); // Move to DISCONNECTED state after initialization
        }

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Wait for notification to update module state

        // Read GPIO states and determine new module state
        bool hp_power_level = gpio_get_level(PIN_HP_POWER);
        bool hp_signal_level = gpio_get_level(PIN_HP_SIGNAL);
        
        module_state_t new_state;
        
        // Determine new state based on pin levels
        if (!hp_power_level && !hp_signal_level) { // Both pins are low (active)
            new_state = CONNECTED;
        } else {
            new_state = DISCONNECTED;
        }
        
        // Get current state and update if changed
        module_state_t current_state = get_module_state();
        
        // Only update and log if state actually changed
        if (current_state != new_state) {
            set_module_state(new_state);
            
            // Check the new state and perform actions accordingly
            switch (new_state) {
                case DISCONNECTED:
                    ESP_LOGI(MODULE_TAG, "Module is disconnected.");
                    gptimer_stop(debounce_timer); // Stop debounce timer if running
                    if (get_module_manager_state() == MANAGER_RUNNING){
                        // If the module manager is in RUNNING state, stop the module
                        xTaskNotifyGive(xTaskModuleManagerUpdate_handle); // Notify the module manager to update state (stop)
                    }
                    break;

                case CONNECTED:
                    ESP_LOGI(MODULE_TAG, "Module is connected.");
                    gptimer_start(debounce_timer); // Start debounce timer to confirm stable connection
                    // Handle connected state tasks here
                    break;

                case FAULT:
                    ESP_LOGE(MODULE_TAG, "Module is in FAULT state!");
                    // Handle fault state tasks here
                    break;

                default:
                    ESP_LOGE(MODULE_TAG, "Unknown module state!");
                    set_module_state(FAULT); // Set to FAULT if an unknown state is encountered
            }
        }
    }
}
