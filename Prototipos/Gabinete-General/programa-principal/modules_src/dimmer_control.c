/**
 * @file dimmer.c
 * @author Mateo Antonio Cignetti (mateo@cignetti.ar)
 * @brief Control program for the dimmer module.
 * @version 0.1
 * @date 2025-07-25
 * 
 * @copyright Copyright (c) 2025
 * 
*/

#include "dimmer_control.h"
#include "gpio_definition.h"

// IMPORTANT: The prototype's relay pin must be shorted to 3.3 V for the dimmer to be set in digital control mode.
//            This was not included in this program to keep test components to a minimum.

#include "driver/gpio.h" // GPIO driver
#include "driver/gptimer.h" // Driver for the gptimers
#include "esp_attr.h" // Needed for IRAM_ATTR (needed for interrupt handler)
#include "esp_log.h" // ESP-IDF logging library
#include "freertos/FreeRTOS.h" // FreeRTOS general library
#include "freertos/task.h" // FreeRTOS task library

#define PIN_RELAY_OUT PIN_S1
#define ESP_INTR_FLAG_DEFAULT 0 // Default interrupt flag
#define PIN_ZCD_IN PIN_S16 // Zero crossing detector input pin
#define PIN_TRIAC_OUT PIN_S15 // TRIAC output pin
#define PULSE_WIDTH_US 25 // Pulse width for driving the triac, in microseconds
#define DIMMER_TIMER_COUNT_DEFAULT 4000 // Turns on TRIAC at 9,9 ms after zero crossing
                                        // (basically starts the dimmer off)
#define ALARM_COUNT_MIN 100 // Minimum alarm count for the dimmer
#define ALARM_COUNT_MAX 9900 // Maximum alarm count for the dimmer

static gptimer_handle_t dimmer_wait_timer = NULL; // Handle for the wait timer
static gptimer_handle_t dimmer_pulse_timer = NULL; // Handle for the pulse timer

static volatile bool timer_is_running = false; // Aux flag to ensure proper timer operation, additional
                                      // safety measure in case an interrupt happens mid timer
                                      // (shouldn't happen in normal operation)
static uint16_t dimmer_wait_alarm_count = DIMMER_TIMER_COUNT_DEFAULT; // Static variable to store the alarm count

static bool isModuleRunning = false;
static const char* MODULE_TAG = "Dimmer Module"; // Module name for logging


// Private function prototypes
static bool dimmer_wait_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);
static bool dimmer_pulse_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);
static void zcd_isr_handler();
static void configure_gpios();
static void delete_gpios();
static void configure_timers();
static void delete_timers();
static void vTaskStartDimmerModule(void *pvParameters);
static void vTaskStopDimmerModule(void *pvParameters);

// Public functions
void start_dimmer_module() {
    // Create the task to start the dimmer module
    BaseType_t xReturned = xTaskCreate(vTaskStartDimmerModule,
                                       "Start Dimmer Module Task",
                                       5*2048,
                                       NULL,
                                       5,
                                       NULL);
    if (xReturned != pdPASS) {
        ESP_LOGE(MODULE_TAG, "Failed to start dimmer module");
        //return ESP_FAIL; // Return error if task creation failed
    }
}

void stop_dimmer_module() {
    // Create the task to stop the dimmer module
    BaseType_t xReturned = xTaskCreate(vTaskStopDimmerModule,
                                       "Stop Dimmer Module Task",
                                       5*2048,
                                       NULL,
                                       5,
                                       NULL);
    if (xReturned != pdPASS) {
        ESP_LOGE(MODULE_TAG, "Failed to create the Stop Dimmer Module Task");
        //return ESP_FAIL; // Return error if task creation failed
    }
}
//

// Private functions

// Callback of the timer that starts waiting after the zero crossing to turn on the TRIAC
static bool dimmer_wait_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    
    gptimer_stop(timer); // Stop the timer as soon as possible
    gptimer_set_raw_count(timer, 0); // Set the timer count to 0. Very important, if not reset
                                     // the timer will execute the alarm event-31.41762112683969, -62.10131892041877 immediately

    gpio_set_level(PIN_TRIAC_OUT, 1); // Set TRIAC pin high to start the pulse
    
    gptimer_start(dimmer_pulse_timer); // Start the pulse timer
    
    // ESP_DRAM_LOGI("GPTimer", "Alarm callback executed"); // Uncomment this line when needed for debugging

    return true;
}

// Callback of the pulse timer that waits for the pulse to end to turn off the TRIAC.
// The width of the pulse is not very accurate, but it doesn't matter, the TRIAC will turn on
// anyways. I tried using low level functions for the GPIO set level, but it didn't make any difference.
// For example: 25 us pulse is actually 29 us, accuracy gets worse when the pulse is shorter.
static bool dimmer_pulse_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {

    gpio_set_level(PIN_TRIAC_OUT, 0); // Set TRIAC pin low to stop the pulse

    gptimer_stop(timer); // Stop the pulse timer
    gptimer_set_raw_count(timer, 0); // Reset the count, again, important. See callback above
    
    timer_is_running = false; // Reset the timer running flag

    return true;
}

// Handler for the zero crossing detector interrupt.
static void IRAM_ATTR zcd_isr_handler(){
    static gptimer_alarm_config_t wait_alarm_config = {
        .flags.auto_reload_on_alarm = false, // Set the auto reload flag to false
    };
    // Checks that the timer is not running, just in case
    if(!timer_is_running) {
        timer_is_running = true; // Set the timer running flag
        
        wait_alarm_config.alarm_count = dimmer_wait_alarm_count; // Set the alarm count to the default value
        gptimer_set_alarm_action(dimmer_wait_timer, &wait_alarm_config);

        gptimer_start(dimmer_wait_timer); // Start the timer
    } else {
        ESP_DRAM_LOGW("ZCD ISR", "Interrupt happened mid timer, check ZCD signal"); // Log a warning if the timer is already running
    }
    
}

// Function to configure the GPIOs (pins, directions, interrupts, etc.)
static void configure_gpios(){
    
    // Configuration for the zero crossing detector pin
    gpio_config_t zcd_pin_conf = {
        .pin_bit_mask = (1ULL << PIN_ZCD_IN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&zcd_pin_conf)); // Apply the configuration

    //ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT)); // Install ISR service
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_ZCD_IN, zcd_isr_handler, NULL)); // Add ISR handler for the ZCD pin

    // Configuration for the TRIAC pin
    gpio_config_t out_pin_conf = {
        .pin_bit_mask = (1ULL << PIN_TRIAC_OUT) | (1ULL << PIN_RELAY_OUT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&out_pin_conf)); // Apply the configuration

    // THIS IS JUST FOR NOW, REMOVE LATER
    ESP_ERROR_CHECK(gpio_set_level(PIN_RELAY_OUT, 1)); // Enable the relay (dimmer in digital mode)
}

static void delete_gpios(){
    ESP_ERROR_CHECK(gpio_set_level(PIN_RELAY_OUT, 0)); // Disable the relay (dimmer in analog mode)
    ESP_ERROR_CHECK(gpio_isr_handler_remove(PIN_ZCD_IN)); // Remove the ISR handler for the ZCD pin (only pin with ISR)
    
    // Reset the GPIOs to their default state (high impedance input)
    gpio_reset_pin(PIN_ZCD_IN);
    gpio_reset_pin(PIN_TRIAC_OUT);
    gpio_reset_pin(PIN_RELAY_OUT);
    
    ESP_LOGI(MODULE_TAG, "GPIOs cleaned up and reset to high-impedance state");
}

// Function to configure the timers
static void configure_timers(){

    // Configuration for both timers (counting up, and 1 us resolution)
    gptimer_config_t gptimer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1 * 1000 * 1000,
    };
    // Apply the configuration to both timers
    ESP_ERROR_CHECK(gptimer_new_timer(&gptimer_config, &dimmer_wait_timer)); 
    ESP_ERROR_CHECK(gptimer_new_timer(&gptimer_config, &dimmer_pulse_timer));

    // Configure the alarm count and behaviour for the wait timer
    gptimer_alarm_config_t wait_alarm_config = {
        .alarm_count = DIMMER_TIMER_COUNT_DEFAULT, // Triggers the alarm event when the timer reaches this value
        .flags.auto_reload_on_alarm = false,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(dimmer_wait_timer, &wait_alarm_config));

    // Configure the alarm count and behaviour for the pulse timer
    gptimer_alarm_config_t pulse_alarm_config = {
        .alarm_count = PULSE_WIDTH_US, // Triggers the alarm event when the timer reaches this value
        .flags.auto_reload_on_alarm = false,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(dimmer_pulse_timer, &pulse_alarm_config));

    // Configure callback functions for both timers on alarm event
    gptimer_event_callbacks_t wait_callbacks = {
        .on_alarm = dimmer_wait_callback, 
    };
    gptimer_event_callbacks_t pulse_callbacks = {
        .on_alarm = dimmer_pulse_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(dimmer_wait_timer, &wait_callbacks, NULL));
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(dimmer_pulse_timer, &pulse_callbacks, NULL));

    // Enable both timers. The timers are not started yet, but are now ready to be started
    ESP_ERROR_CHECK(gptimer_enable(dimmer_wait_timer));
    ESP_ERROR_CHECK(gptimer_enable(dimmer_pulse_timer));

    gptimer_set_raw_count(dimmer_wait_timer, 0);
    gptimer_set_raw_count(dimmer_pulse_timer, 0);
}

static void delete_timers(){
    // Stop the timers
    if(dimmer_wait_timer != NULL){
        ESP_LOGI(MODULE_TAG, "Deleting dimmer wait timer...");
        gptimer_stop(dimmer_wait_timer); // Stop the wait timer
        gptimer_disable(dimmer_wait_timer); // Disable the wait timer
        gptimer_del_timer(dimmer_wait_timer); // Delete the wait timer
        dimmer_wait_timer = NULL; // Reset the timer handle
    }
    if(dimmer_pulse_timer != NULL){
        ESP_LOGI(MODULE_TAG, "Deleting dimmer pulse timer...");
        gptimer_stop(dimmer_pulse_timer); // Stop the pulse timer
        gptimer_disable(dimmer_pulse_timer); // Disable the pulse timer
        gptimer_del_timer(dimmer_pulse_timer); // Delete the pulse timer
        dimmer_pulse_timer = NULL; // Reset the timer handle
    }
}

static void vTaskStartDimmerModule(void *pvParameters){
    if(!isModuleRunning) { // Check if the module is not already running
        isModuleRunning = true; // Set the module running flag
        ESP_LOGI(MODULE_TAG, "Starting dimmer module..."); // Log the start of the module

        configure_gpios();
        configure_timers();
        
    } else {
        ESP_LOGW(MODULE_TAG, "Dimmer module is already running, ignoring start request.");
    }

    vTaskDelete(NULL); // Delete the task
}

static void vTaskStopDimmerModule(void *pvParameters){
    if(isModuleRunning){
        isModuleRunning = false; // Set the module running flag to false
        ESP_LOGI(MODULE_TAG, "Stopping dimmer module..."); // Log the stop of the module

        delete_timers(); // Stop the timers
        delete_gpios(); // Delete the GPIOs  

    } else {
        ESP_LOGW(MODULE_TAG, "Dimmer module is not running, ignoring stop request.");
    }

    vTaskDelete(NULL); // Delete the task
}

const module_t dimmer_module = {
    .name = "Dimmer Module", // Name of the module
    .start_function = start_dimmer_module, // Function pointer to start the module
    .stop_function = stop_dimmer_module, // Function pointer to stop the module
};
//