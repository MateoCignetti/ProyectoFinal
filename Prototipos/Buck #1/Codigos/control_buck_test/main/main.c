/**
 * @file main.c
 * @author your name (you@domain.com)
 * @brief Programa destinado a controlar un convertidor DC-DC Buck por medio de un PID
 * @version 0.1
 * @date 2025-05-15
 * 
 * @copyright Copyright (c) 2025
 * 
 */

 /*-------------- Includes ----------------*/
#include <stdio.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "math.h"

/*--------------- Defines -----------------*/
#define PWM_FREQUENCY 15000 // 15kHz
#define TIMER_PERIOD_US 200

/*--------------- Handles -----------------*/
adc_oneshot_unit_handle_t adc1_handle = NULL;
adc_cali_handle_t adc1_cali_handle = NULL;
gptimer_handle_t gptimer_handle = NULL;

/*--------------- Variables ---------------*/
const int setpoint_v = 6; // set point 6 V
int feedback_mv = 0;
float feedback_v = 0;

/*--------------- PID Variables -----------*/
// PID constants and variables
const float Kp = 0.5381;
const float Ki = 52.39;
const float Kd = 0.0002741;
const float Ts = TIMER_PERIOD_US / 1000000.0;
const float Nc = 0.001841;

// PID coefficients
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
void adc_init_and_config(void);
void adc_cali_config(void);
void ledc_config(void);
void gptimer_config(void);
static bool gptimer_on_alarm_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *arg);

void app_main(void){
    adc_init_and_config();
    adc_cali_config();
    ledc_config();
    gptimer_config();
}

void adc_init_and_config(void){
    // Initialize ADC
    adc_oneshot_unit_init_cfg_t adc1_init_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT, // REVISAR
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc1_init_config, &adc1_handle));

    adc_oneshot_chan_cfg_t adc1_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &adc1_config));
}

void adc_cali_config(void){
    // Initialize ADC calibration
    adc_cali_curve_fitting_config_t adc1_cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = false,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&adc1_cali_config, &adc1_cali_handle));
}

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
}

static bool gptimer_on_alarm_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *arg){
    // Read ADC value
    ESP_ERROR_CHECK(adc_oneshot_get_calibrated_result(adc1_handle, adc1_cali_handle, ADC_CHANNEL_0, &feedback_mv));
    ESP_LOGI("ADC", "ADC Value: %d", feedback_mv);
    feedback_v = (float)feedback_mv / 1000.0; // Convert to volts  

    if (feedback_v > 3.3) {
        feedback_v = 3.3; // Limit feedback voltage to 3.3V
    } else if (feedback_v < 0) {
        feedback_v = 0; // Limit feedback voltage to 0V
    }    

    // Calculate error
    float error = setpoint_v - feedback_v;
    ESP_LOGI("PID", "Error: %f", error);
    
    // PID control logic here
    input_array[0] = setpoint_v - feedback_v;

    output_array[0] = b_coefficients[0] * input_array[0] + b_coefficients[1] * input_array[1] + b_coefficients[2] * input_array[2] - a_coefficients[1] * output_array[1] - a_coefficients[2] * output_array[2];

    pwm_output_bits = (int) (output_array[0] * 4095.0 / 3.3); // REVISAR CONVERSIÓN Y CURVA DE LINEALIZACION

    if(pwm_output_bits > 4095){
        pwm_output_bits = 4095;
    } else if(pwm_output_bits < 0) {
        pwm_output_bits = 0;
    }
    
    ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, pwm_output_bits, 0);
    input_array[2] = input_array[1];
    input_array[1] = input_array[0];
    output_array[2] = output_array[1];
    output_array[1] = output_array[0];

    return true;
}

void gptimer_config(void){
    gptimer_config_t gptimer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&gptimer_config, &gptimer_handle));

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = 200,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer_handle, &alarm_config));

    gptimer_event_callbacks_t callbacks = {
        .on_alarm = gptimer_on_alarm_callback,
    };

    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer_handle, &callbacks, NULL));
    ESP_ERROR_CHECK(gptimer_enable(gptimer_handle));
    ESP_ERROR_CHECK(gptimer_start(gptimer_handle));

}