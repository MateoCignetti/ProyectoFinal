/**
 * @file dimmer.c
 * @author Mateo Antonio Cignetti (mateo@cignetti.ar)
 * @brief Control program for the dimmer module.
 * @version 0.9
 * @date 2025-10-02
 * 
 * @copyright Copyright (c) 2025
 * 
*/

#include "dimmer_control.h"
#include "gpio_definition.h"
#include "dimmer_interface.h"

#include "driver/gpio.h" // GPIO driver
#include "driver/gptimer.h" // Driver for the gptimers
#include "esp_attr.h" // Needed for IRAM_ATTR (needed for interrupt handler)
#include "esp_log.h" // ESP-IDF logging library
#include "freertos/FreeRTOS.h" // FreeRTOS general library
#include "freertos/task.h" // FreeRTOS task library

// Module identification voltage range (in millivolts)
#define DIMMER_IDENT_MV_MIN 500
#define DIMMER_IDENT_MV_MAX 600

#define PIN_RELAY_OUT PIN_S1
#define PIN_TRIAC_OUT PIN_S15 // TRIAC output pin
#define PIN_ZCD_IN PIN_S16 // Zero crossing detector input pin

#define PULSE_WIDTH_US 25 // Pulse width for driving the triac, in microseconds
#define DIMMER_TIMER_COUNT_DEFAULT 9900 // Dimmer wait timer alarm count. Turns on TRIAC at 9,9 ms after zero crossing
                                        // (basically no load voltage.)
/* UNCOMMENT WHEN IMPLEMENTING USER INTERFACE
#define ALARM_COUNT_MIN 100 // Minimum alarm count for the dimmer
#define ALARM_COUNT_MAX 9900 // Maximum alarm count for the dimmer
*/

static gptimer_handle_t dimmer_wait_timer = NULL; // Handle for the wait timer
static gptimer_handle_t dimmer_pulse_timer = NULL; // Handle for the pulse timer

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
static void start_dimmer_module();
static void stop_dimmer_module();

// Public declarations
const module_t dimmer_module = {
    .name = "Dimmer Module",
    .ident_mv_min = DIMMER_IDENT_MV_MIN,
    .ident_mv_max = DIMMER_IDENT_MV_MAX,
    .start_function = start_dimmer_module,
    .stop_function = stop_dimmer_module,
};
//

// Private functions
static void start_dimmer_module(){
    if(!isModuleRunning) { // Check if the module is not already running
        isModuleRunning = true; // Set the module running flag
        ESP_LOGI(MODULE_TAG, "Starting dimmer module..."); // Log the start of the module
        start_dimmer_interface(); // Start the dimmer interface (UI)
        configure_timers();
        configure_gpios();
        
    } else {
        ESP_LOGW(MODULE_TAG, "Dimmer module got a request to run, but is already running... ignoring start request.");
    }
}

static void stop_dimmer_module(){
    if(isModuleRunning){ // Check if the module is running
        isModuleRunning = false; // Set the module running flag to false
        ESP_LOGI(MODULE_TAG, "Stopping dimmer module..."); // Log the stop of the module

        delete_gpios(); // Delete the GPIOs  
        delete_timers(); // Stop the timers
        stop_dimmer_interface();
    } else {
        ESP_LOGW(MODULE_TAG, "Dimmer module got a request to stop, but is not running... ignoring stop request.");
    }
}

// Callback of the timer that starts waiting after the zero crossing to turn on the TRIAC
static bool dimmer_wait_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    
    gptimer_stop(timer); // Stop the timer as soon as possible
    gptimer_set_raw_count(timer, 0); // Set the timer count to 0. Very important, if not reset
                                     // the timer will execute the alarm event immediately

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
    

    return true;
}

// Handler for the zero crossing detector interrupt.
static void IRAM_ATTR zcd_isr_handler(){
    static gptimer_alarm_config_t wait_alarm_config = {
        .flags.auto_reload_on_alarm = false, // Set the auto reload flag to false
    };
        
    wait_alarm_config.alarm_count = dimmer_wait_alarm_count; // Set the alarm count to the default value
    gptimer_set_alarm_action(dimmer_wait_timer, &wait_alarm_config);

    gptimer_start(dimmer_wait_timer); // Start the timer
    
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
}

static void delete_timers(){
    // Stop the timers
    if(dimmer_wait_timer != NULL){
        ESP_LOGI(MODULE_TAG, "Deleting dimmer wait timer...");
        gptimer_stop(dimmer_wait_timer); // Stop the wait timer
        gptimer_set_raw_count(dimmer_wait_timer, 0); // Reset the wait timer count
        gptimer_disable(dimmer_wait_timer); // Disable the wait timer
        gptimer_del_timer(dimmer_wait_timer); // Delete the wait timer

        dimmer_wait_timer = NULL; // Reset the timer handle
    }
    if(dimmer_pulse_timer != NULL){
        ESP_LOGI(MODULE_TAG, "Deleting dimmer pulse timer...");
        gptimer_stop(dimmer_pulse_timer); // Stop the pulse timer
        gptimer_set_raw_count(dimmer_pulse_timer, 0); // Reset the pulse timer count
        gptimer_disable(dimmer_pulse_timer); // Disable the pulse timer
        gptimer_del_timer(dimmer_pulse_timer); // Delete the pulse timer

        dimmer_pulse_timer = NULL; // Reset the timer handle
    }
}


//