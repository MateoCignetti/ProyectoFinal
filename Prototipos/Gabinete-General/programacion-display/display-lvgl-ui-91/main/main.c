/**
 * @file main.c
 * @author your name (you@domain.com)
 * @brief This project is a test for SPI display programming using LVGL library.
 * @version 0.1
 * @date 2025-09-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/lock.h>
#include <sys/param.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "math.h"

// FREERTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// TIMER
#include "esp_timer.h"
#include "driver/gptimer.h"

// DISPLAY
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

// GPIO
#include "driver/gpio.h"

// SPI
#include "driver/spi_master.h"

// ADC
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

// LEDC
#include "driver/ledc.h"

// Extern libraries
#include "lvgl.h"
#include "esp_lcd_ili9341.h"
#include "encoder.h"
#include "ui.h"

// Include state machine variables
#include "control_state_machine.h"

/*----------- ENCODER DEFINES ------------*/
// Rotary encoder pin configuration
#define PIN_NUM_CLOCKWISE GPIO_NUM_47
#define PIN_NUM_COUNTERCLOCKWISE GPIO_NUM_48
#define PIN_NUM_BUTTON GPIO_NUM_21
/*--------------------------------*/

/*----------- RELAY -----------*/
/*-----------------------------*/

/*----------- CONTROL DEFINES ------------*/
#define PWM_FREQUENCY 19000 // Frequency of PWM signal.
#define TIMER_PERIOD_US 52 // Timer period in microseconds, (Ts).
#define PRINT_LOGS 0 // Set to 1 to print logs, 0 to disable.
#define MAX_PWM_DUTY_CYCLE 4095.0 // Maximum duty cycle for the PWM signal (12-bit resolution)4095.
//#define MAX_PWM_DUTY_CYCLE_11_BIT 2047.0 // Maximum duty cycle for the PWM signal (11-bit resolution).
#define MAX_OUTPUT_VOLTAGE 12.0 // Maximum output voltage of the buck converter in volts.
/*----------------------------------------*/

/*----------- DISPLAY DEFINES ------------*/
// Display pin configuration 
#define LCD_HOST    SPI2_HOST
#define PIN_NUM_SCLK GPIO_NUM_10
#define PIN_NUM_MOSI GPIO_NUM_11
#define PIN_NUM_DC   GPIO_NUM_12
#define PIN_NUM_CS GPIO_NUM_14
#define PIN_NUM_RST GPIO_NUM_13

// Display parameters configuration
#define LCD_H_RES  240
#define LCD_V_RES  320
/*--------------------------------*/

/*------------ LVGL DEFINES --------------*/
#define LVGL_TICK_INCREMENT_MS 2  // LVGL tick increment in milliseconds
/*--------------------------------*/

/*------------ HANDLES -----------*/
static QueueHandle_t encoder_queue = NULL;  // Queue to handle rotary encoder events
static esp_lcd_panel_io_handle_t io_handle = NULL; // LCD panel IO handle
static esp_lcd_panel_handle_t panel_handle = NULL; // LCD panel handle
static esp_timer_handle_t lvgl_tick_timer = NULL;   // Timer handle for LVGL tick

static adc_oneshot_unit_handle_t adc1_handle = NULL;   // ADC handle. Used to save the ADC configurations.
static adc_cali_handle_t adc1_cali_handle = NULL;  // ADC calibration handle.
static gptimer_handle_t gptimer_handle = NULL; // Timer handle used for PID control and to make the sampling time consistent.
static TaskHandle_t xTaskPID = NULL; // PID task handle. Used to notify the PID task when the timer is triggered.

/*--------------------------------*/

/*--------- STATE MACHINE VARIABLES ---------*/
//typedef enum {
//    CONTROL_MODE_IDLE,
//    CONTROL_MODE_FIXED_PWM,
//    CONTROL_MODE_PID
//} control_mode_t;

volatile control_mode_t control_mode = CONTROL_MODE_IDLE;
/*-------------------------------------------*/

/*------------ CONTROL VARIABLES -----------*/
uint32_t setpoint_v = 0; // Setpoint voltage in volts
int feedback_mv = 0;    // Feedback voltage in millivolts
float feedback_v = 0.0;   // Feedback voltage in volts. It is used to compare with the setpoint voltage
float error = 0.0;  // Error between setpoint and feedback voltage.

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

// PID input and output arrays
float input_array[3] = {0, 0, 0};
float output_array[3] = {0, 0, 0}; 
int pwm_output_bits = 0;
int fixed_pwm_value = 0;
/*------------------------------------------*/

/*------------- PWM FREQUENCY --------------*/
static int set_pwm_frequency = 0;
/*------------------------------------------*/

/*----------- ENCODER VARIABLES -----------*/
static rotary_encoder_event_t encoder_event;  // Buffer to allocate events from the encoder
static lv_indev_t *indev_encoder = NULL; // Input device for LVGL (encoder)
/*-----------------------------------------*/

/* --- VARIABLES TO NAVIGATE IN LVGL --- */
static int32_t accumulated_diff = 0;    // Accumulated difference from encoder turns
static lv_indev_state_t button_state = LV_INDEV_STATE_RELEASED; // Current button state

// State for screen management
typedef enum {
    SCREEN_1,
    SCREEN_2,
    SCREEN_3,
    SCREEN_4,
    SCREEN_5,
    SCREEN_6,
    SCREEN_COUNT
} screen_state_t;

static lv_group_t *groups[SCREEN_COUNT];    // Array of LVGL groups for each screen
static lv_group_t *current_group; // Current LVGL group
static screen_state_t current_screen = SCREEN_1;    // Current screen state
/*--------------------------------------*/

// Mutex for LVGL API calls 
static _lock_t lvgl_api_lock;

/*-------- FUNCTION PROTOTYPES --------*/
static void adc_init_and_config(void); // ADC initialization and configuration
static void adc_cali_config(void); // ADC calibration configuration
static void ledc_config(void); // LEDC configuration
static void gptimer_config(void);  // Timer configuration
static bool gptimer_on_alarm_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *arg);  // Timer callback function
static void vTaskPid(void *arg); // PID task function
static void create_pid_task(void); // Function to create the PID task
long map(long x, long in_min, long in_max, long out_min, long out_max); 

static void lvgl_flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);    // LVGL flush callback
static void increase_lvgl_tick(void *arg);  // Function to increase LVGL tick
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);  // Notify LVGL when flush is done
static void read_encoder_callback(lv_indev_t *indev_drv, lv_indev_data_t *data);    // Read encoder state for LVGL
static void create_groups_for_ui(void); // Create LVGL groups for UI navigation
/*-------------------------------------*/

/*------- TASKS FUNCTION PROTOTYPES -------*/
static void lvgl_port_task(void *arg);  // Task to handle LVGL
static void vTaskUpdateGroups(void *pvParameters);  // Task to update LVGL groups based on active screen
static void vTaskUpdatePwmFrequency(void *pvParameters);
/*----------------------------------------*/

void app_main(void){
    adc_init_and_config();
    adc_cali_config();
    ledc_config();

    // Configuration for the relay to change the load
    gpio_config_t relay_pin_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_18),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&relay_pin_conf)); // Apply the configuration
    gpio_set_level(GPIO_NUM_18, 0); // Set initial state to LOW (resistive load)
    encoder_queue = xQueueCreate(10, sizeof(rotary_encoder_event_t));
    if (encoder_queue == NULL) {
        ESP_LOGE("ENCODER", "Failed to create encoder queue");
        return;
    }

    ESP_ERROR_CHECK(rotary_encoder_init(encoder_queue));
    
    rotary_encoder_t display_encoder ={
        .pin_a = PIN_NUM_CLOCKWISE,
        .pin_b = PIN_NUM_COUNTERCLOCKWISE,
        .pin_btn = PIN_NUM_BUTTON,
        .btn_pressed_time_us = 0,
        .btn_state = RE_BTN_RELEASED,
        .acceleration = {0, 0}
    };

    ESP_ERROR_CHECK(rotary_encoder_add(&display_encoder));

    spi_bus_config_t bus_config = {
        .sclk_io_num = PIN_NUM_SCLK,      // SPI clock pin
        .mosi_io_num = PIN_NUM_MOSI,      // SPI MOSI pin
        .miso_io_num = -1,                // Not used
        .quadwp_io_num = -1,              // Not used
        .quadhd_io_num = -1,              // Not used
        .max_transfer_sz = 4096,          // Max transfer size in bytes
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO)); // Initialize SPI bus

    // Configure the SPI panel IO for the LCD
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_DC,        // Data/Command pin
        .cs_gpio_num = PIN_NUM_CS,        // Chip select pin
        .pclk_hz = 10 * 1000 * 1000,      // SPI clock frequency (10 MHz)
        .lcd_cmd_bits = 8,                // Command length in bits
        .lcd_param_bits = 8,              // Parameter length in bits
        .spi_mode = 0,                    // SPI mode 0
        .trans_queue_depth = 10,          // Transaction queue depth
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle)); // Create new SPI panel IO

    // Configure the LCD panel driver (ILI9341)
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,            // Reset pin
        .color_space = LCD_RGB_ELEMENT_ORDER_BGR, // Color space (BGR)
        .bits_per_pixel = 16,                     // 16 bits per pixel
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle)); // Create new ILI9341 panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));      // Reset the panel
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));       // Initialize the panel
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));   // Mirror the display vertically
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true)); // Turn on display

    lv_init();  // Initialize LVGL

    lv_display_t * display = lv_display_create(LCD_H_RES, LCD_V_RES);   // Create LVGL display
    size_t draw_buffer_size = LCD_H_RES * LCD_V_RES / 10 * 2;   // Size of the draw buffer (1/10 of screen size, 2 bytes per pixel)

    void *buffer = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_size, 0); // Allocate DMA-capable memory for the draw buffer
    if (buffer == NULL) {
        ESP_LOGE("MAIN", "Failed to allocate DMA buffer");
        return;
    }

    lv_display_set_buffers(display, buffer, NULL, draw_buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);    // Set display buffers
    lv_display_set_user_data(display, panel_handle);    // Associate the panel handle with the LVGL display
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);   // Set color format to RGB565
    lv_display_set_flush_cb(display, lvgl_flush_callback);  // Set flush callback
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_0);    // Set display rotation

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick"
    };

    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer)); // Create timer for LVGL tick
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 2 * 1000)); // Call every 2ms

    const esp_lcd_panel_io_callbacks_t cbs ={
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display));   // Register callback for flush done notification
    
    // Create input device for LVGL
    indev_encoder = lv_indev_create();  // Create a new input device
    lv_indev_set_type(indev_encoder, LV_INDEV_TYPE_ENCODER);    // Set input device type to encoder
    lv_indev_set_read_cb(indev_encoder, read_encoder_callback); // Set read callback for the encoder

    xTaskCreate(lvgl_port_task,
                "LVGL",
                4096,
                NULL,
                2,
                NULL
                );

    xTaskCreate(vTaskUpdateGroups,
                "UpdateGroups",
                configMINIMAL_STACK_SIZE * 4,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL
                );

    _lock_acquire(&lvgl_api_lock);
    ui_init();
    create_groups_for_ui();
    _lock_release(&lvgl_api_lock);

    create_pid_task();
    gptimer_config();
    xTaskCreate(vTaskUpdatePwmFrequency,
                "Update PWM Frequency",
                configMINIMAL_STACK_SIZE * 4,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL
                );

    while (true){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}

static void vTaskUpdatePwmFrequency(void *pvParameters){
    int last_pwm_frequency = 0;
    TickType_t xLastWakeTime;
    const TickType_t xPeriod = pdMS_TO_TICKS(1000); // 200 ms period
    xLastWakeTime = xTaskGetTickCount();
    while (true){
        set_pwm_frequency = lv_slider_get_value(ui_SliderFreq1)*1000;
        last_pwm_frequency = ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0);
        if (set_pwm_frequency != last_pwm_frequency){
            ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, set_pwm_frequency);
        }
        //printf("Set PWM Frequency: %d, Real PWM Frequency: %d\n", set_pwm_frequency, last_pwm_frequency);
        vTaskDelayUntil( &xLastWakeTime, xPeriod );
    }
}

/**
 * @brief Create a pid task object
 * 
 */
static void create_pid_task(void){
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
}

long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * @brief PID task function that runs when the notification from the timer is received.
 * It calculates and applies the PID control.
 * 
 * @param arg 
 */
static void vTaskPid(void *arg){
    while(true){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Wait for the timer alarm notification
        switch(control_mode){
            case CONTROL_MODE_FIXED_PWM:
                fixed_pwm_value = lv_slider_get_value(ui_SliderDuty);
                pwm_output_bits = map(fixed_pwm_value, 0, 100, 0, MAX_PWM_DUTY_CYCLE);
                break;
            case CONTROL_MODE_PID:
            /*--------------------------------------------------------------------------------*/
            /*--------------------------------------------------------------------------------*/
                // WARNING: This section is based on 12 Bit duty cycle resolution.
                // but the LEDC is configured for 11 bits resolution.
            /*--------------------------------------------------------------------------------*/
            /*--------------------------------------------------------------------------------*/
                // Read ADC value
                setpoint_v = lv_slider_get_value(ui_SliderSP);
                adc_oneshot_get_calibrated_result(adc1_handle, adc1_cali_handle, ADC_CHANNEL_7, &feedback_mv);  // Return mV value.
                feedback_v = feedback_mv / 1000.0; // Convert to volts  
                
                // TODO: Evaluate the alternative linearization function
                //feedback_v = 3.6052 * feedback_v + 0.0704;
                // with a noise-free source to determine its accuracy and performance compared to the current funcion.
                feedback_v = -0.0058 * pow(feedback_v, 3) - 0.0146 * pow(feedback_v, 2) + 3.6873 * feedback_v + 0.0328;

                if (feedback_v < 0) {
                    feedback_v = 0; // Limit feedback voltage to 0V
                } else if (feedback_v > MAX_OUTPUT_VOLTAGE) {
                    feedback_v = MAX_OUTPUT_VOLTAGE; // Limit feedback voltage to 12V
                }    

                // Calculate error
                error = setpoint_v - feedback_v;
                
                // PID control logic here
                input_array[0] = setpoint_v - feedback_v;

                output_array[0] = b_coefficients[0] * input_array[0] + b_coefficients[1] * input_array[1] + b_coefficients[2] * input_array[2] - a_coefficients[1] * output_array[1] - a_coefficients[2] * output_array[2];

                pwm_output_bits = (int) (output_array[0] * MAX_PWM_DUTY_CYCLE / MAX_OUTPUT_VOLTAGE);
                break;
            case CONTROL_MODE_IDLE:
            default:
                pwm_output_bits = 0;
                break;
        }

        if(pwm_output_bits > MAX_PWM_DUTY_CYCLE){
            pwm_output_bits = MAX_PWM_DUTY_CYCLE;
        } else if(pwm_output_bits < 0) {
            pwm_output_bits = 0;
        }
        
        ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, pwm_output_bits, 0);  // Set new duty cycle based on PID output
        input_array[2] = input_array[1];
        input_array[1] = input_array[0];
        output_array[2] = output_array[1];
        output_array[1] = output_array[0];

        static TickType_t last_print = 0;
        if (xTaskGetTickCount() - last_print >= pdMS_TO_TICKS(1000)) {
            last_print = xTaskGetTickCount();
            printf("Set-point = %ld, Feedback = %.2f V, Error = %.2f, PWM = %d\n", setpoint_v, feedback_v, error, pwm_output_bits);
        }
        #if PRINT_LOGS
            static TickType_t last_print = 0;
            if (xTaskGetTickCount() - last_print >= pdMS_TO_TICKS(1000)) {
                last_print = xTaskGetTickCount();
                ESP_LOGI("STATUS", "Set-point = %d", "Feedback = %.2f V, Error = %.2f, PWM = %d", setpoint_v, feedback_v, error, pwm_output_bits);
            }
        #endif
    }
}

/**
 * @brief This function initializes and configures the ADC for reading the feedback voltage.
 * It sets the ADC unit, clock source, and attenuation. 
 * 
 */
static void adc_init_and_config(void){
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
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_7, &adc1_config));
    
    #if PRINT_LOGS
        ESP_LOGI("ADC", "ADC1 initialized and configured");
    #endif
}

/**
 * @brief Calibration configuration for the ADC. It determines the calibration scheme
 * and sets the ADC calibration parameters like attenuation and bitwidth.
 * 
 */
static void adc_cali_config(void){
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
 * Perhaps it could be better to use MCPWM instead of LEDC.
 * 
 */
static void ledc_config(void){
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
        .gpio_num = GPIO_NUM_17,
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
    // NOTE: FreeRTOS task notification is used because it's more efficient and lightweight
    // compared to semaphores or queues. The limitation is that only one task can be notified.
    vTaskNotifyGiveFromISR(xTaskPID, &xHigherPriorityTaskWoken);
    
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

    #if PRINT_LOGS
        ESP_LOGI("Timer", "GPTimer initialized and configured");
    #endif
}

/**
 * @brief Callback to notify LVGL that the flush operation is complete
 * 
 * @param panel_io 
 * @param edata 
 * @param user_ctx 
 * @return true 
 * @return false 
 */
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx){
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

/**
 * @brief Flush callback to transfer a buffer to the display
 * 
 * @param disp 
 * @param area 
 * @param px_map 
 */
static void lvgl_flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map){
    panel_handle = lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // because SPI LCD is big-endian, we need to swap the RGB bytes order
    lv_draw_sw_rgb565_swap(px_map, (offsetx2 + 1 - offsetx1) * (offsety2 + 1 - offsety1));
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
}

/**
 * @brief Increase LVGL tick count
 * 
 * @param arg 
 */
static void increase_lvgl_tick(void *arg){
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(LVGL_TICK_INCREMENT_MS);
}

/**
 * @brief LVGL task to handle periodic tasks
 * 
 * @param arg 
 */
static void lvgl_port_task(void *arg){
    uint32_t time_till_next_ms = 0;
    while (1) {
        _lock_acquire(&lvgl_api_lock);
        time_till_next_ms = lv_timer_handler();
        _lock_release(&lvgl_api_lock);
        // in case of triggering a task watch dog time out
        time_till_next_ms = MAX(time_till_next_ms, 1000/CONFIG_FREERTOS_HZ);
        // in case of lvgl display not ready yet
        time_till_next_ms = MIN(time_till_next_ms, 500);
        usleep(1000 * time_till_next_ms);
    }
}

/**
 * @brief Callback to read encoder data
 * 
 * @param indev_drv 
 * @param data 
 */
void read_encoder_callback(lv_indev_t *indev_drv, lv_indev_data_t *data){
    if (xQueueReceive(encoder_queue, &encoder_event, 0) == pdTRUE){
        //printf("Event received: ");
        switch (encoder_event.type) {
            case RE_ET_CHANGED:
                //printf("Encoder turned, diff: %ld\n", encoder_event.diff);
                accumulated_diff += encoder_event.diff;
                break;
            case RE_ET_BTN_PRESSED:
                //printf("Button pressed\n");
                button_state = LV_INDEV_STATE_PRESSED;
                break;
            case RE_ET_BTN_RELEASED:
                //printf("Button released\n");
                button_state = LV_INDEV_STATE_RELEASED;
                break;
            case RE_ET_BTN_LONG_PRESSED:
                //printf("Button long pressed\n");
                break;
            case RE_ET_BTN_CLICKED:
                //printf("Button clicked\n");
                break;
            default:
                //printf("Unknown event\n");
                break;
        }
    }
    data->enc_diff = accumulated_diff;
    data->state = button_state;
    accumulated_diff = 0; // Reset after reading
    //printf("Slider value freq (Screen2): %ld\n", lv_slider_get_value(ui_SliderFreq1));
}

/**
 * @brief Task to update LVGL groups based on the active screen
 * 
 * @param pvParameters 
 */
static void vTaskUpdateGroups(void *pvParameters){
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 100;
    xLastWakeTime = xTaskGetTickCount();

    while (1) {
        _lock_acquire(&lvgl_api_lock);
        // Detectar la pantalla activa
        lv_obj_t *active_screen = lv_screen_active();
        screen_state_t new_screen = current_screen;

        // Mapear pantalla activa a screen_state_t
        if (active_screen == ui_Screen1) {
            new_screen = SCREEN_1;
        } else if (active_screen == ui_Screen2) {
            new_screen = SCREEN_2;
        } else if (active_screen == ui_Screen3) {
            new_screen = SCREEN_3;
        } else if (active_screen == ui_Screen4) {
            new_screen = SCREEN_4;
        } else if (active_screen == ui_Screen5) {
            new_screen = SCREEN_5;
        } else if (active_screen == ui_Screen6) {
            new_screen = SCREEN_6;
        }
        // Actualizar grupo solo si cambió la pantalla
        if (new_screen != current_screen) {
            current_screen = new_screen;
            current_group = groups[current_screen];
            lv_group_set_default(current_group);
            lv_indev_set_group(indev_encoder, groups[current_screen]);
            //printf("Cambié a pantalla %d\n", current_screen);
        }
        _lock_release(&lvgl_api_lock);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(xFrequency));
    }
}

/**
 * @brief Create a groups for ui object
 * 
 */
static void create_groups_for_ui(void){
    for(int i=0; i < SCREEN_COUNT; i++){
        groups[i] = lv_group_create();
    }
    
    // Add interactive objects to groups[SCREEN_1] group
    lv_group_add_obj(groups[SCREEN_1], ui_freqScreen);
    lv_group_add_obj(groups[SCREEN_1], ui_controlScreen);
    lv_group_add_obj(groups[SCREEN_1], ui_loadScreen);

    lv_obj_add_event_cb(ui_freqScreen, ui_event_freqScreen, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_controlScreen, ui_event_controlScreen, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_loadScreen, ui_event_loadScreen, LV_EVENT_CLICKED, NULL);

    // Add interactive objects to groups[SCREEN_2] group
    lv_group_add_obj(groups[SCREEN_2], ui_ButtonReturn1);
    lv_group_add_obj(groups[SCREEN_2], ui_SliderFreq1);
    lv_group_add_obj(groups[SCREEN_2], ui_ButtonReturnDefault1);

    lv_obj_add_event_cb(ui_ButtonReturn1, ui_event_ButtonReturn1, LV_EVENT_CLICKED, NULL);
    //lv_obj_add_event_cb(ui_SliderFreq1, ui_event_SliderFreq1, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_ButtonReturnDefault1, ui_event_ButtonReturnDefault1, LV_EVENT_CLICKED, NULL);

    // Add interactive objects to groups[SCREEN_3] group
    lv_group_add_obj(groups[SCREEN_3], ui_ButtonReturn2);
    lv_group_add_obj(groups[SCREEN_3], ui_ButtonPWM);
    lv_group_add_obj(groups[SCREEN_3], ui_ButtonPID);

    lv_obj_add_event_cb(ui_ButtonReturn2, ui_event_ButtonReturn2, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonPWM, ui_event_ButtonPWM, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonPID, ui_event_ButtonPID, LV_EVENT_CLICKED, NULL);

    // Add interactive objects to groups[SCREEN_4] group
    lv_group_add_obj(groups[SCREEN_4], ui_ButtonReturn3);
    lv_group_add_obj(groups[SCREEN_4], ui_SliderDuty);
    lv_group_add_obj(groups[SCREEN_4], ui_ButtonReturnDefault2);

    lv_obj_add_event_cb(ui_ButtonReturn3, ui_event_ButtonReturn3, LV_EVENT_CLICKED, NULL);
    //lv_obj_add_event_cb(ui_SliderDuty, ui_event_SliderDuty, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_ButtonReturnDefault2, ui_event_ButtonReturnDefault2, LV_EVENT_CLICKED, NULL);
    
    // Add interactive objects to groups[SCREEN_5] group
    lv_group_add_obj(groups[SCREEN_5], ui_ButtonReturn4);
    lv_group_add_obj(groups[SCREEN_5], ui_SliderSP);
    lv_group_add_obj(groups[SCREEN_5], ui_ButtonReturnDefault3);

    lv_obj_add_event_cb(ui_ButtonReturn4, ui_event_ButtonReturn4, LV_EVENT_CLICKED, NULL);
    //lv_obj_add_event_cb(ui_SliderSP, ui_event_SliderSP, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_ButtonReturnDefault3, ui_event_ButtonReturnDefault3, LV_EVENT_CLICKED, NULL);
    
    // Add interactive objects to groups[SCREEN_6] group
    lv_group_add_obj(groups[SCREEN_6], ui_Button5);
    lv_group_add_obj(groups[SCREEN_6], ui_Button6);
    lv_group_add_obj(groups[SCREEN_6], ui_ButtonReturn5);

    lv_obj_add_event_cb(ui_Button5, ui_event_Button5, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_Button6, ui_event_Button6, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_ButtonReturn5, ui_event_ButtonReturn5, LV_EVENT_PRESSED, NULL);


    current_group = groups[SCREEN_1];  // Grupo inicial
    current_screen = SCREEN_1;
    lv_group_set_default(groups[current_screen]);
    lv_indev_set_group(indev_encoder, groups[current_screen]);
    lv_group_focus_obj(ui_freqScreen);
}