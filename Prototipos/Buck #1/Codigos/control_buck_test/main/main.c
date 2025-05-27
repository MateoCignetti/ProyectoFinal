/**
 * @file main.c
 * @author Alejo Alesandria (aalesandria@facultad.sanfrancisco.utn.edu.ar)
 * @brief Main file for the DC-DC Buck Converter PID Control using ESP32-S3. Using GPTimer and RTOS Notifications.
 * @version 0.1
 * @date 2025-05-15
 * 
 * @copyright Copyright (c) 2025
 * 
 */

/*-------------- Includes ----------------*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "math.h"

/*--------------- Defines -----------------*/
#define PWM_FREQUENCY 19000 // Frequency of PWM signal.
#define TIMER_PERIOD_US 200 // Timer period in microseconds, (Ts).
#define PRINT_LOGS 0 // Set to 1 to print logs, 0 to disable.

/*--------------- Handles -----------------*/
adc_oneshot_unit_handle_t adc1_handle = NULL;   // ADC handle. Used to save the ADC configurations.
adc_cali_handle_t adc1_cali_handle = NULL;  // ADC calibration handle.
gptimer_handle_t gptimer_handle = NULL; // Timer handle used for PID control and to make the sampling time consistent.
TaskHandle_t xTaskPID = NULL; // PID task handle. Used to notify the PID task when the timer is triggered.

/*--------------- Variables ---------------*/
const int setpoint_v = 4; // Setpoint voltage in volts
int feedback_mv = 0;    // Feedback voltage in millivolts
float feedback_v = 0.0;   // Feedback voltage in volts. It is used to compare with the setpoint voltage
float error = 0.0;  // Error between setpoint and feedback voltage.

/*--------------- PID Variables -----------*/
// PID constants and variables
const float Kp = 0.5;
const float Ki = 62.4;
const float Kd = 0.002741;
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

// PID input and output arrays
float input_array[3] = {0, 0, 0};
float output_array[3] = {0, 0, 0}; 
int pwm_output_bits = 0;

/*--------------- Function prototypes ------------*/
void adc_init_and_config(void); // ADC initialization and configuration
void adc_cali_config(void); // ADC calibration configuration
void ledc_config(void); // LEDC configuration
void gptimer_config(void);  // Timer configuration
static bool gptimer_on_alarm_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *arg);  // Timer callback function
void vTaskPid(void *arg); // PID task function

void app_main(void){
    adc_init_and_config();
    adc_cali_config();
    ledc_config();
    
    BaseType_t task_create = xTaskCreatePinnedToCore(vTaskPid,
                            "vTaskPid", 
                            configMINIMAL_STACK_SIZE * 4, 
                            NULL, 
                            tskIDLE_PRIORITY + 1, 
                            &xTaskPID, 
                            1); // Create a task to run the PID control
    
    if(task_create != pdPASS){
        ESP_LOGE("Task", "Error creating PID task");
        return;
    } else {
        ESP_LOGI("Task", "PID task created successfully");
    }

    gptimer_config();

}

/**
 * @brief PID task function that runs when the notification from the timer is received.
 * It calculates and applies the PID control.
 * 
 * @param arg 
 */
void vTaskPid(void *arg){
    while(true){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Wait for the timer alarm notification

        // Read ADC value
        adc_oneshot_get_calibrated_result(adc1_handle, adc1_cali_handle, ADC_CHANNEL_3, &feedback_mv);  // Return mV value.
        feedback_v = (float)feedback_mv / 1000.0; // Convert to volts  
        
        // Here insert the linearization function. Both equations are similar, but it needs to test which is better. 
        // It's necessary to test them with noise-free source.
        //feedback_v = 3.6052 * feedback_v + 0.0704;
        feedback_v = -0.0058 * pow(feedback_v, 3) - 0.0146 * pow(feedback_v, 2) + 3.6873 * feedback_v + 0.0328;

        if (feedback_v < 0) {
            feedback_v = 0; // Limit feedback voltage to 0V
        } else if (feedback_v > 12) {
            feedback_v = 12; // Limit feedback voltage to 12V
        }    

        // Calculate error
        error = setpoint_v - feedback_v;
        
        // PID control logic here
        input_array[0] = setpoint_v - feedback_v;

        output_array[0] = b_coefficients[0] * input_array[0] + b_coefficients[1] * input_array[1] + b_coefficients[2] * input_array[2] - a_coefficients[1] * output_array[1] - a_coefficients[2] * output_array[2];

        pwm_output_bits = (int) (output_array[0] * 4095.0 / 12.0);

        if(pwm_output_bits > 4095){
            pwm_output_bits = 4095;
        } else if(pwm_output_bits < 0) {
            pwm_output_bits = 0;
        }
        
        ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, pwm_output_bits, 0);  // Set new duty cycle based on PID output
        input_array[2] = input_array[1];
        input_array[1] = input_array[0];
        output_array[2] = output_array[1];
        output_array[1] = output_array[0];
        
        #if PRINT_LOGS
            static TickType_t last_print = 0;
            if (xTaskGetTickCount() - last_print >= pdMS_TO_TICKS(1000)) {
                last_print = xTaskGetTickCount();
                ESP_LOGI("STATUS", "Feedback = %.2f V, Error = %.2f, PWM = %d", feedback_v, error, pwm_output_bits);
            }
        #endif
    }
}

/**
 * @brief This function initializes and configures the ADC for reading the feedback voltage.
 * It sets the ADC unit, clock source, and attenuation. 
 * 
 */
void adc_init_and_config(void){
    // Initialize ADC
    adc_oneshot_unit_init_cfg_t adc1_init_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc1_init_config, &adc1_handle));

    adc_oneshot_chan_cfg_t adc1_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_3, &adc1_config));
    
    #if PRINT_LOGS
        ESP_LOGI("ADC", "ADC1 initialized and configured");
    #endif
}

/**
 * @brief Calibration configuration for the ADC. It determines the calibration scheme
 * and sets the ADC calibration parameters like attenuation and bitwidth.
 * 
 */
void adc_cali_config(void){
    // Initialize ADC calibration
    adc_cali_curve_fitting_config_t adc1_cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&adc1_cali_config, &adc1_cali_handle));
    
    #if PRINT_LOGS
        ESP_LOGI("ADC", "ADC1 calibration initialized and configured");
    #endif
}

/**
 * @brief Configures the LEDC for PWM output.
 * It sets the frequency of the PWM signal and the resolution of the duty cycle.
 * Also, it initializes the LEDC channel with the configurations and installs the fade function.
 * Perhaps it could be better to use MCPWM insted of LEDC.
 * 
 */
void ledc_config(void){
    // Initialize LEDC
    ledc_timer_config_t ledc_timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .freq_hz = PWM_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_cfg));

    ledc_channel_config_t ledc_channel_cfg ={
        .gpio_num = GPIO_NUM_18,
        .channel = LEDC_CHANNEL_0,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_cfg));
    ledc_fade_func_install(0);

    #if PRINT_LOGS
        ESP_LOGI("LEDC", "LEDC initialized and configured");
    #endif
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
    // FreeRTOS task notification is used because it's more efficient and lightweight
    // compared to semaphores or queues. The limitation is that only one task can be notified.
    vTaskNotifyGiveFromISR(xTaskPID, &xHigherPriorityTaskWoken);
    
    return xHigherPriorityTaskWoken == pdTRUE; // Return true if a higher priority task was woken up
}

/**
 * @brief GPTimer configuration funcition. It initializes the timer with a specified resolution
 * and sets the timer direction. In addition, it configures the timer alarm action with the
 * specified period and registers the callback function for the timer alarm event.
 * 
 */
void gptimer_config(void){
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

    #if PRINT_LOGS
        ESP_LOGI("Timer", "GPTimer initialized and configured");
    #endif
}