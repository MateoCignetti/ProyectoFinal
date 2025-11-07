/**
 * @file main.c
 * @author your name (you@domain.com)
 * @brief This project is a test for SPI display programming using LVGL library.
 * @version 0.8
 * @date 2025-09-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "buck_control.h"
#include "buck_interface.h"

#include "gpio_definition.h"
#include <math.h>
#include <sys/lock.h>
#include "esp_log.h"
#include "buck_ui.h"

// FREERTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// TIMER
#include "driver/gptimer.h"

// GPIO
#include "driver/gpio.h"

// ADC
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

// LEDC
#include "driver/ledc.h"

// External mutex for LVGL API calls - defined in ui_config.c
extern _lock_t lvgl_api_lock;

// Pin
#define PIN_RELAY PIN_S8 // Pin to control the relay for load switching
#define ADC_MODULE_BUCK_UNIT ADC_S9_UNIT
#define ADC_MODULE_BUCK_CHANNEL ADC_S9_CHANNEL
#define LEDC_MODULE_BUCK_CHANNEL LEDC_CHANNEL_0
#define LEDC_MODULE_BUCK_TIMER LEDC_TIMER_0
#define LEDC_MODULE_BUCK_PIN PIN_S7
#define BUCK_DEFAULT_LOAD BUCK_LOAD_RESISTIVE

// Module identification voltage range (in millivolts)
#define BUCK_IDENT_MV_MIN 1900
#define BUCK_IDENT_MV_MAX 2000

/*----------- CONTROL DEFINES ------------*/
#define DEFAULT_PWM_FREQUENCY 19000 // Frequency of PWM signal.
#define TIMER_PERIOD_US 1000 // Timer period in microseconds, (Ts). 1ms = 1kHz control loop
#define PRINT_LOGS 0 // Set to 1 to print logs, 0 to disable.
#define MAX_PWM_DUTY_CYCLE 4095.0 // Maximum duty cycle for the PWM signal (12-bit resolution)4095.
//#define MAX_PWM_DUTY_CYCLE_11_BIT 2047.0 // Maximum duty cycle for the PWM signal (11-bit resolution).
#define MAX_OUTPUT_VOLTAGE 12.0 // Maximum output voltage of the buck converter in volts.
/*----------------------------------------*/

/*------------ HANDLES -----------*/

static adc_oneshot_unit_handle_t adc1_unit_handle = NULL;   // ADC handle. Used to save the ADC configurations.
static adc_cali_handle_t adc1_cali_handle = NULL;  // ADC calibration handle.
static gptimer_handle_t gptimer_handle = NULL; // Timer handle used for PID control and to make the sampling time consistent.
static TaskHandle_t xTaskControlUpdate_handle = NULL; // PID task handle. Used to notify the PID task when the timer is triggered.
static TaskHandle_t xTaskUIUpdate_handle = NULL; // UI update task handle for cached values
static TaskHandle_t xTaskLoadUpdate_handle = NULL; // Load type update task handle
static SemaphoreHandle_t xControlModeMutex = NULL; // Mutex to protect access to the control mode variable.
static SemaphoreHandle_t xLoadTypeMutex = NULL; // Mutex to protect access to the load type variable.

// Shutdown flags for graceful task termination
static volatile bool shutdown_pid_task = false;
static volatile bool shutdown_ui_task = false;

// Cached UI values to avoid blocking PID loop
static volatile uint32_t cached_fixed_pwm_value = 0;
static volatile uint32_t cached_setpoint_v = 0;
static volatile uint32_t cached_pwm_frequency = DEFAULT_PWM_FREQUENCY;

/*--------------------------------*/

const char* MODULE_TAG = "Buck Control"; // Module name for logging


volatile control_mode_t control_mode = CONTROL_MODE_IDLE;
static buck_load_type_t buck_load_type = BUCK_DEFAULT_LOAD;
/*-------------------------------------------*/

//PID variables
// PID constants and variables
const float Kp = 0.5;
const float Ki = 65.0;
const float Kd = 0;
const float Ts = TIMER_PERIOD_US / 1000000.0;
const float Nc = 3;

// PID coefficients. These coefficients are obtained from the PID with derivative filter
const float a_coefficients[3] = {
    1,
    -2 + Nc * Ts,
    1 - Nc * Ts
};
const float b_coefficients[3] = {
    Kp + Kd * Nc,
    -2 * Kp - 2 * Kd * Nc + Ki * Ts + Kp * Nc * Ts,
    Kp + Kd * Nc - Ki * Ts - Kp * Nc * Ts + Ki * Nc * Ts * Ts
};
/*------------------------------------------*/

/*-------- FUNCTION PROTOTYPES --------*/
void start_buck_module(void); // Function to start the buck converter module
void stop_buck_module(void); // Function to stop the buck converter module
void set_control_mode(control_mode_t new_mode); // Function to set the control mode
control_mode_t get_control_mode(void); // Function to get the current control mode
static void delete_gpio(void); // Function to delete the GPIO
static void delete_adc(void); // Function to delete the ADC
static void delete_timer(void); // Function to delete the timer
static void delete_ledc(void); // Function to delete the LEDC
static void create_module_tasks(void); // Function to create the PID task
static void delete_module_tasks(void); // Function to delete the PID task

static void adc_init_and_config(void); // ADC initialization and configuration
static void adc_cali_config(void); // ADC calibration configuration
static void ledc_config(void); // LEDC configuration
static void gptimer_config(void);  // Timer configuration
static void configure_gpio(void); // GPIO configuration
static bool gptimer_on_alarm_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *arg);  // Timer callback function
long map(long x, long in_min, long in_max, long out_min, long out_max); 
/*-------------------------------------*/

/*------- TASKS FUNCTION PROTOTYPES -------*/
static void vTaskUIUpdate(void *arg); // UI update task function to cache values
static void vTaskControlUpdate(void *arg); // PID task function
static void vTaskLoadUpdate(void *arg); // Load type update task function
/*----------------------------------------*/

/* ----------- MODULE DECLARATION ----------- */
const module_t buck_module = {
    .name = "Buck Converter Module",
    .ident_mv_min = BUCK_IDENT_MV_MIN,
    .ident_mv_max = BUCK_IDENT_MV_MAX,
    .start_function = start_buck_module,
    .stop_function = stop_buck_module,
};
//

void start_buck_module(void){
    // Initialize shutdown flags
    shutdown_pid_task = false;
    shutdown_ui_task = false;
    
    start_buck_interface();

    adc_init_and_config();
    adc_cali_config();
    ledc_config();
    configure_gpio();
    create_module_tasks();
    gptimer_config();    
}

void stop_buck_module(void){
    // Stop timer BEFORE deleting tasks (important order)
    delete_timer();

    // Now safe to delete tasks
    delete_module_tasks();
    
    // Clean up hardware
    delete_adc();
    delete_ledc();
    delete_gpio();

    ESP_LOGI(MODULE_TAG, "Before stop buck interface.");
    stop_buck_interface();
    ESP_LOGI(MODULE_TAG, "After stop buck interface.");
}   

// Function to safely set control mode
void set_control_mode(control_mode_t new_mode) {
    if (xControlModeMutex != NULL && xSemaphoreTake(xControlModeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        control_mode = new_mode;
        xSemaphoreGive(xControlModeMutex);
    } else {
        ESP_LOGE(MODULE_TAG, "Failed to get control mode mutex");
        control_mode = CONTROL_MODE_FAULT;
    }
}

// Function to safely read control state
control_mode_t get_control_mode() {
    control_mode_t state; // Initialize to safe default
    if (xControlModeMutex != NULL && xSemaphoreTake(xControlModeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = control_mode;
        xSemaphoreGive(xControlModeMutex);
    } else {
        ESP_LOGE(MODULE_TAG, "Failed to get control mode mutex");
        state = CONTROL_MODE_FAULT;
    }
    return state;
}

void set_buck_load(buck_load_type_t new_load_type){
    if (xLoadTypeMutex != NULL && xSemaphoreTake(xLoadTypeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        buck_load_type = new_load_type;
        xSemaphoreGive(xLoadTypeMutex);
        
    } else {
        ESP_LOGE(MODULE_TAG, "Failed to get load type mutex");
    }

    // Notify the load task about the state change
    if(xTaskLoadUpdate_handle != NULL) {
        xTaskNotifyGive(xTaskLoadUpdate_handle);
    }
}

buck_load_type_t get_buck_load(void){
    buck_load_type_t current_load_type = BUCK_DEFAULT_LOAD;
    if (xLoadTypeMutex != NULL && xSemaphoreTake(xLoadTypeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        current_load_type = buck_load_type;
        xSemaphoreGive(xLoadTypeMutex);
    } else {
        ESP_LOGE(MODULE_TAG, "Failed to get load type mutex");
    }

    return current_load_type;
}


/**
 * @brief This function initializes and configures the ADC for reading the feedback voltage.
 * It sets the ADC unit, clock source, and attenuation. 
 * 
 */
static void adc_init_and_config(void){
    // Initialize ADC
    adc_oneshot_unit_init_cfg_t adc1_init_config = {
        .unit_id = ADC_MODULE_BUCK_UNIT,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc1_init_config, &adc1_unit_handle));

    adc_oneshot_chan_cfg_t adc1_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_unit_handle, ADC_MODULE_BUCK_CHANNEL, &adc1_config));
}

/**
 * @brief Calibration configuration for the ADC. It determines the calibration scheme
 * and sets the ADC calibration parameters like attenuation and bitwidth.
 * 
 */
static void adc_cali_config(void){
    // Initialize ADC calibration
    adc_cali_curve_fitting_config_t adc1_cali_config = {
        .unit_id = ADC_MODULE_BUCK_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&adc1_cali_config, &adc1_cali_handle));
}

static void delete_adc(void){
    if (adc1_unit_handle != NULL) {
        ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_unit_handle));
        adc1_unit_handle = NULL;
    }
    if (adc1_cali_handle != NULL) {
        ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(adc1_cali_handle));
        adc1_cali_handle = NULL;
    }
    gpio_reset_pin(PIN_S9); // Reset the ADC pin to its default state
}

/**
 * @brief Configures the LEDC for PWM output.
 * It sets the frequency of the PWM signal and the resolution of the duty cycle.
 * Also, it initializes the LEDC channel with the configurations and installs the fade function.
 * Perhaps it could be better to use MCPWM instead of LEDC.
 * 
 */
static void ledc_config(void){
    // Initialize LEDC
    ledc_timer_config_t ledc_timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_MODULE_BUCK_TIMER,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .freq_hz = DEFAULT_PWM_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_cfg));

    ledc_channel_config_t ledc_channel_cfg ={
        .gpio_num = LEDC_MODULE_BUCK_PIN,
        .channel = LEDC_MODULE_BUCK_CHANNEL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_MODULE_BUCK_TIMER,
        .duty = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_cfg));
    ledc_fade_func_install(0);
}

static void delete_ledc(void){
    ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_MODULE_BUCK_CHANNEL, 0, 0);
    ESP_ERROR_CHECK(ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_MODULE_BUCK_CHANNEL, 0));
    ledc_fade_func_uninstall();
    gpio_reset_pin(LEDC_MODULE_BUCK_PIN); // Reset the LEDC pin to its default state
}

static void configure_gpio(void){
    // Configuration for the relay to change the load
    gpio_config_t relay_pin_conf = {
        .pin_bit_mask = (1ULL << PIN_RELAY),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&relay_pin_conf)); // Apply the configuration
    gpio_set_level(PIN_RELAY, 0); // Set initial state to LOW (resistive load)
}

static void delete_gpio(void){
    ESP_ERROR_CHECK(gpio_set_level(PIN_RELAY, 0)); // Disable the relay (dimmer in analog mode)
    gpio_reset_pin(PIN_RELAY); // Reset the relay pin to its default state
    
    ESP_LOGI(MODULE_TAG, "GPIOs cleaned up and reset to high-impedance state");
}

/**
 * @brief Timer callback function that is called when the timer alarm is triggered.
 * It notifies the PID task.
 * @param timer 
 * @param edata 
 * @param arg 
 * @return true 
 * @return false 
 */
static bool gptimer_on_alarm_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *arg){

    BaseType_t xHigherPriorityTaskWoken = pdFALSE; // Variable to check if a higher priority task was woken up

    // Notify the PID task that the timer alarm has been triggered.
    // NOTE: FreeRTOS task notification is used because it's more efficient and lightweight
    // compared to semaphores or queues. The limitation is that only one task can be notified.
    vTaskNotifyGiveFromISR(xTaskControlUpdate_handle, &xHigherPriorityTaskWoken);
    
    return xHigherPriorityTaskWoken == pdTRUE; // Return true if a higher priority task was woken up
}

/**
 * @brief GPTimer configuration function. It initializes the timer with a specified resolution
 * and sets the timer direction. In addition, it configures the timer alarm action with the
 * specified period and registers the callback function for the timer alarm event.
 * 
 */
static void gptimer_config(void){
    #if PRINT_LOGS
        ESP_LOGI("GPTIMER", "GPTIMER initializing...");
    #endif
    gptimer_config_t gptimer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&gptimer_config, &gptimer_handle));

    gptimer_event_callbacks_t callbacks = {
        .on_alarm = gptimer_on_alarm_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer_handle, &callbacks, NULL));
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = TIMER_PERIOD_US,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };

    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer_handle, &alarm_config));

    ESP_ERROR_CHECK(gptimer_enable(gptimer_handle));

    ESP_ERROR_CHECK(gptimer_start(gptimer_handle));
}

static void delete_timer(void){
    if (gptimer_handle != NULL) {
        ESP_ERROR_CHECK(gptimer_stop(gptimer_handle));
        ESP_ERROR_CHECK(gptimer_set_raw_count(gptimer_handle, 0)); // Reset the wait timer count
        ESP_ERROR_CHECK(gptimer_disable(gptimer_handle));
        ESP_ERROR_CHECK(gptimer_del_timer(gptimer_handle));
        gptimer_handle = NULL;
    }
}

/**
 * @brief Create a pid task object
 * 
 */
static void create_module_tasks(void){
    xControlModeMutex = xSemaphoreCreateMutex();
    if (xControlModeMutex == NULL) {
        ESP_LOGE(MODULE_TAG, "Failed to create control mode mutex");
        return;
    }
    
    // Create UI update task (lower priority, updates cached values)
    BaseType_t xReturnedUI = xTaskCreatePinnedToCore(vTaskUIUpdate,
                                        "vTaskUIUpdate", 
                                        configMINIMAL_STACK_SIZE * 2, 
                                        NULL, 
                                        tskIDLE_PRIORITY + 1,  // Priority 1 (lower than PID)
                                        &xTaskUIUpdate_handle,
                                        1
                                        );
    
    if (xReturnedUI != pdPASS) {
        ESP_LOGE(MODULE_TAG, "Failed to create UI Update Task");
        return;
    }
    
    BaseType_t xReturned = xTaskCreatePinnedToCore(vTaskControlUpdate,
                                        "vTaskControlUpdate", 
                                        configMINIMAL_STACK_SIZE * 4, 
                                        NULL, 
                                        tskIDLE_PRIORITY + 2,  // Priority 2 (same as LVGL)
                                        &xTaskControlUpdate_handle,
                                        1
                                        ); // Create a task to run the PID control
    
    if (xReturned != pdPASS) {
        ESP_LOGE(MODULE_TAG, "Failed to create PID Task");
        return;
    }

    xLoadTypeMutex = xSemaphoreCreateMutex();

    if (xLoadTypeMutex == NULL) {
        ESP_LOGE(MODULE_TAG, "Failed to create load type mutex");
        return;
    }

    BaseType_t xReturnedLoad = xTaskCreatePinnedToCore(vTaskLoadUpdate,
                                        "vTaskLoadUpdate", 
                                        configMINIMAL_STACK_SIZE * 4, 
                                        NULL, 
                                        tskIDLE_PRIORITY + 2,  // Priority 2 (same as LVGL)
                                        &xTaskLoadUpdate_handle,
                                        1
                                        ); // Create a task to run the PID control

    if (xReturnedLoad != pdPASS) {
        ESP_LOGE(MODULE_TAG, "Failed to create Load Type Task");
        return;
    }
}

static void delete_module_tasks(void){

    if (xTaskLoadUpdate_handle != NULL) {

        vTaskDelete(xTaskLoadUpdate_handle);
        eTaskState task_state = eTaskGetState(xTaskLoadUpdate_handle);
        while (task_state != eDeleted) { 
            vTaskDelay(pdMS_TO_TICKS(10));
            // Update task state
            task_state = eTaskGetState(xTaskLoadUpdate_handle);
        }
        ESP_LOGI(MODULE_TAG, "LoadUpdate task exited gracefully");

        xTaskLoadUpdate_handle = NULL;
    }

    if (xLoadTypeMutex != NULL) {
        vSemaphoreDelete(xLoadTypeMutex);
        xLoadTypeMutex = NULL;
    }

    if (xTaskControlUpdate_handle != NULL) {
        // Signal task to shutdown
        shutdown_pid_task = true;

        eTaskState task_state = eTaskGetState(xTaskControlUpdate_handle);
        while (task_state != eDeleted) { 
            vTaskDelay(pdMS_TO_TICKS(10));
            // Update task state
            task_state = eTaskGetState(xTaskControlUpdate_handle);

        }
        ESP_LOGI(MODULE_TAG, "ControlUpdate task exited gracefully");

        xTaskControlUpdate_handle = NULL;
    }
    
    if (xTaskUIUpdate_handle != NULL) {
        // Signal UI task to shutdown
        shutdown_ui_task = true;

        eTaskState task_state = eTaskGetState(xTaskUIUpdate_handle);
        while (task_state != eDeleted) { 
            vTaskDelay(pdMS_TO_TICKS(10));
            task_state = eTaskGetState(xTaskUIUpdate_handle);
        }
        ESP_LOGI(MODULE_TAG, "UIUpdate task exited gracefully");

        xTaskUIUpdate_handle = NULL;
    }
    
    if (xControlModeMutex != NULL) {
        vSemaphoreDelete(xControlModeMutex);
        xControlModeMutex = NULL;
    }
}

/**
 * @brief UI update task that runs at lower frequency to cache UI values.
 * This prevents the high-frequency PID task from being blocked by LVGL lock contention.
 * Uses try-lock to avoid blocking when LVGL is busy processing encoder input.
 * 
 * @param arg 
 */
static void vTaskUIUpdate(void *arg){
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xUpdatePeriod = pdMS_TO_TICKS(100); // Update every 100ms
    
    while(true){
        // Check for shutdown
        if (shutdown_ui_task) {
            ESP_LOGI(MODULE_TAG, "UI update task shutting down gracefully");
            vTaskDelete(NULL);
            return;
        }
        
        // Try to acquire lock without blocking - skip this update if LVGL is busy
        if (_lock_try_acquire(&lvgl_api_lock) == 0) {
            cached_fixed_pwm_value = lv_slider_get_value(buck_ui_SliderDuty);
            cached_setpoint_v = lv_slider_get_value(buck_ui_SliderSP);
            cached_pwm_frequency = lv_slider_get_value(buck_ui_SliderFreq1) * 1000;
            _lock_release(&lvgl_api_lock);
        }
        // If lock is busy, skip this update cycle - cached values remain valid
        
        vTaskDelayUntil(&xLastWakeTime, xUpdatePeriod);
    }
}

/**
 * @brief PID task function that runs when the notification from the timer is received.
 * It calculates and applies the PID control.
 * 
 * @param arg 
 */
static void vTaskControlUpdate(void *arg){
    uint32_t fixed_pwm_value = 0;
    uint32_t pwm_frequency = DEFAULT_PWM_FREQUENCY;
    uint32_t setpoint_v = 0;

    float feedback_v = 0.0;   // Feedback voltage in volts. It is
    int feedback_mv = 0;    // Feedback voltage in millivolts
    int pwm_output_bits = 0; // PWM duty cycle value to be set
    
    while(true){
        // Check for shutdown with timeout to avoid blocking forever
        if (shutdown_pid_task) {
            ESP_LOGI(MODULE_TAG, "PID task shutting down gracefully");
            vTaskDelete(NULL);
            return;
        }
        
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)); // Wait for timer with timeout
        
        // Check shutdown again after waiting
        if (shutdown_pid_task) {
            ESP_LOGI(MODULE_TAG, "PID task shutting down gracefully");
            vTaskDelete(NULL);
            return;
        }
        
        switch(control_mode){
            case CONTROL_MODE_FIXED_PWM:
            
                // Use cached value - no lock needed!
                fixed_pwm_value = cached_fixed_pwm_value;
                pwm_output_bits = map(fixed_pwm_value, 0, 100, 0, MAX_PWM_DUTY_CYCLE);
                break;
            case CONTROL_MODE_PID:
                static float input_array[3] = {0, 0, 0};
                static float output_array[3] = {0, 0, 0}; 
            /*--------------------------------------------------------------------------------*/
            /*--------------------------------------------------------------------------------*/
                // WARNING: This section is based on 12 Bit duty cycle resolution.
                // but the LEDC is configured for 11 bits resolution.
            /*--------------------------------------------------------------------------------*/
            /*--------------------------------------------------------------------------------*/
                // Use cached setpoint value - no lock needed!
                setpoint_v = cached_setpoint_v;

                adc_oneshot_get_calibrated_result(adc1_unit_handle, adc1_cali_handle, ADC_CHANNEL_7, &feedback_mv);  // Return mV value.
                feedback_v = feedback_mv / 1000.0; // Convert to volts  
                
                // Polynomial linearization - optimized for speed (no pow() function)
                // Original: feedback_v = -0.0058 * pow(feedback_v, 3) - 0.0146 * pow(feedback_v, 2) + 3.6873 * feedback_v + 0.0328;
                // Optimized: Use direct multiplication instead of pow() to avoid watchdog timeout
                float fb_v2 = feedback_v * feedback_v;        // v^2
                float fb_v3 = fb_v2 * feedback_v;             // v^3
                feedback_v = -0.0058 * fb_v3 - 0.0146 * fb_v2 + 3.6873 * feedback_v + 0.0328;

                if (feedback_v < 0) {
                    feedback_v = 0; // Limit feedback voltage to 0V
                } else if (feedback_v > MAX_OUTPUT_VOLTAGE) {
                    feedback_v = MAX_OUTPUT_VOLTAGE; // Limit feedback voltage to 12V
                }    
                
                // PID control logic here
                input_array[0] = setpoint_v - feedback_v;

                output_array[0] = b_coefficients[0] * input_array[0] + b_coefficients[1] * input_array[1] + b_coefficients[2] * input_array[2] - a_coefficients[1] * output_array[1] - a_coefficients[2] * output_array[2];

                pwm_output_bits = (int) (output_array[0] * MAX_PWM_DUTY_CYCLE / MAX_OUTPUT_VOLTAGE);

                if(pwm_output_bits > MAX_PWM_DUTY_CYCLE){
                    pwm_output_bits = MAX_PWM_DUTY_CYCLE;
                } else if(pwm_output_bits < 0) {
                    pwm_output_bits = 0;
                }
                input_array[2] = input_array[1];
                input_array[1] = input_array[0];
                output_array[2] = output_array[1];
                output_array[1] = output_array[0];
                break;
            case CONTROL_MODE_IDLE:
                pwm_output_bits = 0;
                break;
            default:
                break;
        }
        
        ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_MODULE_BUCK_CHANNEL, pwm_output_bits, 0);  // Set new duty cycle based on PID output

        // Use cached frequency value - no lock needed!
        pwm_frequency = cached_pwm_frequency;
        
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, pwm_frequency);

        #if PRINT_LOGS
            static TickType_t last_print = 0;
            if (xTaskGetTickCount() - last_print >= pdMS_TO_TICKS(1000)) {
                last_print = xTaskGetTickCount();
                ESP_LOGI("STATUS", "Set-point = %d", "Feedback = %.2f V, Error = %.2f, PWM = %d", setpoint_v, feedback_v, error, pwm_output_bits);
            }
        #endif
    }
}

void vTaskLoadUpdate(void *arg){
    buck_load_type_t current_load_type = BUCK_DEFAULT_LOAD;

    while(true){

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Wait for notification indefinitely

        // Try to acquire load type mutex
        current_load_type = get_buck_load();

        // Set relay based on load type
        if (current_load_type == BUCK_LOAD_INDUCTIVE) {
            gpio_set_level(PIN_RELAY, 1); // Set relay to HIGH for inductive load
        } else {
            gpio_set_level(PIN_RELAY, 0); // Set relay to LOW for resistive load
        }
        set_control_mode(CONTROL_MODE_IDLE); // Set control mode to IDLE when switching
    }
}

long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

