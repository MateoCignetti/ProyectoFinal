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
#include "ui_config.h"
#include "dimmer_ui.h"

#include "driver/gpio.h" // GPIO driver
#include "driver/gptimer.h" // Driver for the gptimers
#include "esp_attr.h" // Needed for IRAM_ATTR (needed for interrupt handler)
#include "esp_log.h" // ESP-IDF logging library
#include "freertos/FreeRTOS.h" // FreeRTOS general library
#include "freertos/task.h" // FreeRTOS task library
#include "freertos/semphr.h" // FreeRTOS semaphore library

// Module identification voltage range (in millivolts)
#define DIMMER_IDENT_MV_MIN 500
#define DIMMER_IDENT_MV_MAX 600

#define PIN_RELAY_OUT PIN_S1
#define PIN_TRIAC_OUT PIN_S15 // TRIAC output pin
#define PIN_ZCD_IN PIN_S16 // Zero crossing detector input pin

#define PULSE_WIDTH_US 25 // Pulse width for driving the triac, in microseconds
#define DIMMER_TIMER_COUNT_DEFAULT 9000 // Dimmer wait timer alarm count. Turns on TRIAC at 9 ms after zero crossing
                                        // (basically no load voltage.)
#define DIMMER_UI_UPDATE_PERIOD_MS 100

#define ALARM_COUNT_MIN_US 1000 // Minimum alarm count for the dimmer
#define ALARM_COUNT_MAX_US 9200 // Maximum alarm count for the dimmer


dimmer_control_state_t dimmer_control_state = DIMMER_CONTROL_IDLE; // Current state of the dimmer control
static SemaphoreHandle_t xDimmerControlStateMutex; // Mutex for protecting access to the dimmer control state
static TaskHandle_t xTaskUpdateDimmerControlState_handle = NULL; // Task handle for the dimmer control state update task
static TaskHandle_t xTaskDimmerUIUpdate_handle = NULL; // Task handle for the dimmer UI update task

static gptimer_handle_t dimmer_wait_timer = NULL; // Handle for the wait timer
static gptimer_handle_t dimmer_pulse_timer = NULL; // Handle for the pulse timer

static bool isModuleRunning = false;
static const char* MODULE_TAG = "Dimmer Module"; // Module name for logging

static int32_t slider_CC_value = DIMMER_TIMER_COUNT_DEFAULT;
static SemaphoreHandle_t xSliderValueMutex;


// Private function prototypes
static bool dimmer_wait_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);
static bool dimmer_pulse_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);
static void zcd_isr_handler();
static void configure_gpios();
static void delete_gpios();
static void configure_timers();
static void delete_timers();
static void create_module_tasks();
static void delete_module_tasks();
static void start_dimmer_module();
static void stop_dimmer_module();
static void vTaskUpdateDimmerControlState(void *pvParameters);
static void vTaskDimmerUIUpdate(void *pvParameters);

// Public declarations
const module_t dimmer_module = {
    .name = "Dimmer Module",
    .ident_mv_min = DIMMER_IDENT_MV_MIN,
    .ident_mv_max = DIMMER_IDENT_MV_MAX,
    .start_function = start_dimmer_module,
    .stop_function = stop_dimmer_module,
};

void set_dimmer_control_state(dimmer_control_state_t new_dimmer_control_state){
    if(xDimmerControlStateMutex != NULL && xSemaphoreTake(xDimmerControlStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        dimmer_control_state = new_dimmer_control_state;
        xSemaphoreGive(xDimmerControlStateMutex);
    } else {
        ESP_LOGE(MODULE_TAG, "Failed to get dimmer control state mutex");
    }

    // Notify the dimmer control state update task about the state change
    if(xTaskUpdateDimmerControlState_handle != NULL) {
        xTaskNotifyGive(xTaskUpdateDimmerControlState_handle);
    }
}

dimmer_control_state_t get_dimmer_control_state(void){
    dimmer_control_state_t current_dimmer_control_state;
    if(xDimmerControlStateMutex != NULL && xSemaphoreTake(xDimmerControlStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        current_dimmer_control_state = dimmer_control_state;
        xSemaphoreGive(xDimmerControlStateMutex);
    } else {
        ESP_LOGE(MODULE_TAG, "Failed to get dimmer control state mutex");
        current_dimmer_control_state = DIMMER_CONTROL_IDLE; // Return idle state on error
    }
    return current_dimmer_control_state;
}

//

// Private functions
static void start_dimmer_module(){
    if(!isModuleRunning) { // Check if the module is not already running
        isModuleRunning = true; // Set the module running flag
        ESP_LOGI(MODULE_TAG, "Starting dimmer module..."); // Log the start of the module
        create_module_tasks();
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
        delete_module_tasks(); // Delete the module tasks
    } else {
        ESP_LOGW(MODULE_TAG, "Dimmer module got a request to stop, but is not running... ignoring stop request.");
    }
}

static void create_module_tasks(){
    xDimmerControlStateMutex = xSemaphoreCreateMutex();
    if (xDimmerControlStateMutex == NULL) {
        ESP_LOGE(MODULE_TAG, "Failed to create dimmer control state mutex");
        return;
    }

    BaseType_t xReturned = xTaskCreate(vTaskUpdateDimmerControlState,
                                       "Update Dimmer Control State Task",
                                       4096,
                                       NULL,
                                       5,
                                       &xTaskUpdateDimmerControlState_handle);

    if (xReturned != pdPASS) {
        ESP_LOGE(MODULE_TAG, "Failed to create Module Connection Task");
    }

    xSliderValueMutex = xSemaphoreCreateMutex();
    if (xSliderValueMutex == NULL) {
        ESP_LOGE(MODULE_TAG, "Failed to create slider value mutex");
        return;
    }

    xReturned = xTaskCreate(vTaskDimmerUIUpdate,
                                    "Update Dimmer UI Task",
                                    4096,
                                    NULL,
                                    5,
                                    &xTaskDimmerUIUpdate_handle);

    if (xReturned != pdPASS) {
        ESP_LOGE(MODULE_TAG, "Failed to create Module Connection Task");
    }
}

static void delete_module_tasks(){
    if (xTaskUpdateDimmerControlState_handle != NULL) {

        // Signal idle task to delete the task
        vTaskDelete(xTaskUpdateDimmerControlState_handle);

        // Check if deleted, if not, wait until it is deleted
        eTaskState task_state = eTaskGetState(xTaskUpdateDimmerControlState_handle);
        while (task_state != eDeleted) {
            vTaskDelay(pdMS_TO_TICKS(10));
            // Update task state
            task_state = eTaskGetState(xTaskUpdateDimmerControlState_handle);

        }
        ESP_LOGI(MODULE_TAG, "Update Dimmer Control task exited gracefully");

        xTaskUpdateDimmerControlState_handle = NULL;
    }
    
    
    if (xDimmerControlStateMutex != NULL) {
        vSemaphoreDelete(xDimmerControlStateMutex);
        xDimmerControlStateMutex = NULL;
    }

    if (xTaskDimmerUIUpdate_handle != NULL) {

        // Signal idle task to delete the task
        vTaskDelete(xTaskDimmerUIUpdate_handle);

        // Check if deleted, if not, wait until it is deleted
        eTaskState task_state = eTaskGetState(xTaskDimmerUIUpdate_handle);
        while (task_state != eDeleted) {
            vTaskDelay(pdMS_TO_TICKS(10));
            // Update task state
            task_state = eTaskGetState(xTaskDimmerUIUpdate_handle);

        }
        ESP_LOGI(MODULE_TAG, "Update Dimmer UI task exited gracefully");

        xTaskDimmerUIUpdate_handle = NULL;
    }

    if (xSliderValueMutex != NULL) {
        vSemaphoreDelete(xSliderValueMutex);
        xSliderValueMutex = NULL;
    }
}

static void vTaskUpdateDimmerControlState(void *pvParameters){
    // Task to update the dimmer control state

    while(1){
        // Wait for notification about state change
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    
        dimmer_control_state_t current_state = get_dimmer_control_state();
        
        // When changing states, stop and reset the timer to prevent spurious triggers
        gptimer_stop(dimmer_wait_timer);
        gptimer_set_raw_count(dimmer_wait_timer, 0);
        gptimer_stop(dimmer_pulse_timer);
        gptimer_set_raw_count(dimmer_pulse_timer, 0);

        switch(current_state){
            case DIMMER_CONTROL_IDLE:
                ESP_LOGI(MODULE_TAG, "Dimmer control state set to IDLE");
                // In idle state, enable the relay (digital mode)
                gpio_set_level(PIN_RELAY_OUT, 1);
                // Disable zero crossing interrupts in idle mode
                gpio_set_intr_type(PIN_ZCD_IN, GPIO_INTR_DISABLE);
                break;

            case DIMMER_CONTROL_ANALOG:
                ESP_LOGI(MODULE_TAG, "Dimmer control state set to ANALOG");
                // In analog mode, disable the relay
                gpio_set_level(PIN_RELAY_OUT, 0);
                // Disable zero crossing interrupts in analog mode
                gpio_set_intr_type(PIN_ZCD_IN, GPIO_INTR_DISABLE);
                break;

            case DIMMER_CONTROL_DIGITAL:
                ESP_LOGI(MODULE_TAG, "Dimmer control state set to DIGITAL");

                // In digital mode, enable the relay (digital mode)
                gpio_set_level(PIN_RELAY_OUT, 1);
                gpio_set_intr_type(PIN_ZCD_IN, GPIO_INTR_ANYEDGE); // Set ZCD interrupt to trigger on both edges
                break;
        }
    }
}

static void vTaskDimmerUIUpdate(void *arg){
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xUpdatePeriod = pdMS_TO_TICKS(DIMMER_UI_UPDATE_PERIOD_MS); // Update every 100ms

    int32_t slider_CC_value_local = DIMMER_TIMER_COUNT_DEFAULT;
    int32_t slider_CC_value_new = 0;

    dimmer_control_state_t current_state;

    gptimer_alarm_config_t wait_alarm_config = {
        .flags.auto_reload_on_alarm = false, // Set the auto reload flag to false
    };
    
    ESP_LOGI(MODULE_TAG, "Dimmer UI Update task started");
    
    while(true){
        
        current_state = get_dimmer_control_state();

        if(current_state == DIMMER_CONTROL_DIGITAL){

            if(xSliderValueMutex != NULL && xSemaphoreTake(xSliderValueMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                slider_CC_value_local = slider_CC_value;
                xSemaphoreGive(xSliderValueMutex);
            }

            // Only process if we can acquire the LVGL lock
            if (_lock_try_acquire(&lvgl_api_lock) == 0) {
                slider_CC_value_new = lv_slider_get_value(dimmer_ui_SliderCC);
                _lock_release(&lvgl_api_lock);
                
                // Convert slider value (0-50) to microseconds (100-9900 µs)
                // Multiply by 200 to get 0-10000 range, then clamp to valid range
                slider_CC_value_new *= 200; // Convert to microseconds

                if(slider_CC_value_new < ALARM_COUNT_MIN_US){
                    slider_CC_value_new = ALARM_COUNT_MIN_US;
                } else if(slider_CC_value_new > ALARM_COUNT_MAX_US){
                    slider_CC_value_new = ALARM_COUNT_MAX_US;
                }


                ESP_LOGI(MODULE_TAG, "Slider local values: CC=%d µs", slider_CC_value_local);
                ESP_LOGI(MODULE_TAG, "Slider new values: CC=%d µs", slider_CC_value_new);

                // Prepare label text buffers OUTSIDE the lock
                char buffer_cc[10];

                float cc_ms = slider_CC_value_new / 1000.0f; // Convert µs to ms

                snprintf(buffer_cc, sizeof(buffer_cc), "%.1f ms", cc_ms);
                
                // Update on-screen labels - only hold lock during UI updates
                if (_lock_try_acquire(&lvgl_api_lock) == 0) {
                    lv_label_set_text(dimmer_ui_LabelCC, buffer_cc);

                    _lock_release(&lvgl_api_lock);
                }

                // Check if any slider value has changed
                if(slider_CC_value_local != slider_CC_value_new){
                    
                    // Update the global slider values
                    if(xSliderValueMutex != NULL && xSemaphoreTake(xSliderValueMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        slider_CC_value = slider_CC_value_new;

                        xSemaphoreGive(xSliderValueMutex);
                    }
                    
                    // Update the timer alarm based on current state
                    //gptimer_stop(dimmer_wait_timer); // Stop the wait timer to update the alarm count safely
                    //gptimer_set_raw_count(dimmer_wait_timer, 0); // Reset the timer count to 0 before changing the alarm
        
                    wait_alarm_config.alarm_count = slider_CC_value_new;
                    ESP_ERROR_CHECK(gptimer_set_alarm_action(dimmer_wait_timer, &wait_alarm_config));
                    
                    //gptimer_start(dimmer_wait_timer); // Restart the wait timer
                }
            } // If lock is busy, skip this entire update cycle

        }

        vTaskDelayUntil(&xLastWakeTime, xUpdatePeriod);
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
    // The ZCD interrupt can be configured to trigger on:
    // - ANYEDGE (full wave mode - both positive and negative)
    // - NEGEDGE (positive semicycle mode - only when going from high to low, entering positive half)
    // - POSEDGE (negative semicycle mode - only when going from low to high, entering negative half)
    // 
    // Since gpio_set_intr_type() properly configures which edge triggers this ISR,
    // we can safely start the timer here - it will only be called on the correct edge.
    gptimer_start(dimmer_wait_timer); // Start the timer
}

// Function to configure the GPIOs (pins, directions, interrupts, etc.)
static void configure_gpios(){
    
    // Configuration for the zero crossing detector pin
    gpio_config_t zcd_pin_conf = {
        .pin_bit_mask = (1ULL << PIN_ZCD_IN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,  // Start with interrupts disabled, will be enabled when mode is set
    };
    ESP_ERROR_CHECK(gpio_config(&zcd_pin_conf)); // Apply the configuration

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM)); // Install ISR service
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
    ESP_ERROR_CHECK(gpio_uninstall_isr_service(ESP_INTR_FLAG_IRAM)); // Install ISR service

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