#include "module_state.h"

#include "gpio_definition.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "soc/gpio_struct.h"
#include "soc/io_mux_reg.h"

#define ESP_INTR_FLAG_DEFAULT 0 // Default interrupt flag
#define MODULE_CHECK_INTERVAL_MS 10

static module_state_t module_state = INIT; // Initial state of the module
static const char* MODULE_TAG = "Module State"; // Module name for logging
static void hp_isr_handler(void* arg); // Forward declaration of the ISR handler

static TaskHandle_t xTaskModuleStateUpdate_handle = NULL; // Task handle for the module state update task

void initialize_module_state(){
    create_module_state_tasks(); // Create the task to update the module state
}

void setup_ident_gpios(){
    gpio_config_t hp_pin_conf = {
        .pin_bit_mask = (1ULL << PIN_HP_POWER) | (1ULL << PIN_HP_SIGNAL), // Configure both HP pins
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&hp_pin_conf)); // Apply the configuration

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM)); // Install ISR service
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_HP_POWER, hp_isr_handler, NULL)); // Add ISR handler for the ZCD pin
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_HP_SIGNAL, hp_isr_handler, NULL)); // Add ISR handler for the ZCD pin
}

void delete_ident_gpios(){
    ESP_ERROR_CHECK(gpio_isr_handler_remove(PIN_HP_POWER)); // Remove the ISR handler for the HP Power pin
    ESP_ERROR_CHECK(gpio_isr_handler_remove(PIN_HP_SIGNAL)); // Remove the ISR handler for the HP Signal pin
    gpio_uninstall_isr_service(); // Uninstall the ISR service

    // Reset the GPIOs to their default state
    gpio_reset_pin(PIN_HP_POWER);
    gpio_reset_pin(PIN_HP_SIGNAL);
}

// Low-level function to read the GPIO level, to avoid using gpio_get_level in ISR
static bool IRAM_ATTR get_gpio_level_low_level(gpio_num_t gpio_num) {
    return (GPIO.in >> gpio_num) & 1; // Devuelve 1 (alto) o 0 (bajo)
}


// Interrupt Service Routine (ISR) handler for HP Power and HP Signal pins
// This is maybe too much for a isr, check if it works properly!
static void IRAM_ATTR hp_isr_handler(void* arg) {


    BaseType_t xHigherPriorityTaskWoken = pdFALSE; // Variable to indicate if a higher priority task was woken
    uint32_t pin = (uint32_t) arg; // Get the pin number from the argument

    bool hp_power_level = get_gpio_level_low_level(PIN_HP_POWER); // Read the HP Power pin level
    bool hp_signal_level = get_gpio_level_low_level(PIN_HP_SIGNAL); // Read the HP Signal pin level

    if(!hp_power_level && !hp_signal_level) { // Both HP Power and HP Signal are high
        if (module_state == DISCONNECTED) {
            module_state = CONNECTED; // Set state to CONNECTED if both pins are high

            xTaskNotifyFromISR(xTaskModuleStateUpdate_handle, 0, eSetBits, &xHigherPriorityTaskWoken); // Notify the task to update the module state
        }
    } else{
        if (module_state != DISCONNECTED) {
            module_state = DISCONNECTED; // If either HP Power or HP Signal is low, set state to DISCONNECTED

            xTaskNotifyFromISR(xTaskModuleStateUpdate_handle, 0, eSetBits, &xHigherPriorityTaskWoken); // Notify the task to update the module state
        }
    }

    /*    // Alternative implementation to avoid using gpio_get_level in ISR. Not tested yet.
    static int hp_power_level = 1; // Variable to store previous HP Power level
    static int hp_signal_level = 1; // Variable to store previous HP Signal level
    if (pin == PIN_HP_POWER) {
        if(hp_power_level){
           hp_power_level = 0;
        } else{
            hp_power_level = 1;
        }
    } else if (pin == PIN_HP_SIGNAL) {
        if(hp_signal_level){
            hp_signal_level = 0;
        } else {
            hp_signal_level = 1;
        }
    } else {
        ESP_DRAM_LOGE(MODULE_TAG, "Invalid pin number in ISR handler: %d", pin);
        module_state = FAULT; // Set module state to FAULT if an invalid pin is detected
        return; // Invalid pin number, exit the ISR
    }

    if (!hp_power_level && !hp_signal_level) { // Both HP Power and HP Signal are low
        if (module_state == DISCONNECTED) {
            
        }
    } else {
        if (module_state != DISCONNECTED) {
            module_state = DISCONNECTED; // If either HP Power or HP Signal is high, set state to DISCONNECTED
        }      
    }
    */

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken); // Yield from ISR if a higher priority task was woken
}

void create_module_state_tasks() {
    // Create the task to update the module state
    BaseType_t xReturned = xTaskCreate(vTaskModuleStateUpdate,
                                       "Module State Update Task",
                                       2048,
                                       NULL,
                                       5,
                                       &xTaskModuleStateUpdate_handle);
    if (xReturned != pdPASS) {
        ESP_LOGE(MODULE_TAG, "Failed to create Module State Update Task");
        module_state = FAULT; // Set module state to FAULT if task creation failed
    }
}

void vTaskModuleStateUpdate(void *pvParameters) {
    while (true) {

        if(module_state == INIT) {
            ESP_LOGI(MODULE_TAG, "Module is initializing...");
            setup_ident_gpios(); // Setup GPIOs for module identification
            module_state = DISCONNECTED; // Move to DISCONNECTED state after initialization
        }

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Wait for notification to update module state

        // Check the new state and perform actions accordingly
        switch (module_state) {

            case DISCONNECTED:
                ESP_LOGI(MODULE_TAG, "Module is disconnected.");
                // Handle disconnected state tasks here
                break;

            case CONNECTED:
                ESP_LOGI(MODULE_TAG, "Module is connected.");
                // Handle connected state tasks here
                break;

            case RUNNING:
                ESP_LOGI(MODULE_TAG, "Module is running.");
                // Handle running state tasks here
                break;

            case STOPPING:
                ESP_LOGI(MODULE_TAG, "Module is stopping.");
                // Handle stopping state tasks here
                module_state = DISCONNECTED; // Move to DISCONNECTED state after stopping
                break;

            case FAULT:
                ESP_LOGE(MODULE_TAG, "Module is in fault state!");
                // Handle fault state tasks here
                break;

            default:
                ESP_LOGE(MODULE_TAG, "Unknown module state!");
                module_state = FAULT; // Set to FAULT if an unknown state is encountered
        }
    }
}
