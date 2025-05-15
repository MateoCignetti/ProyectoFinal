/**
 * @file main.c
 * @author Mateo Antonio Cignetti (mateo@cignetti.ar)
 * @brief 
 * @version 0.1
 * @date 2025-05-15
 * 
 * @copyright Copyright (c) 2025
 * 
*/

#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_attr.h" // Include for IRAM_ATTR
#include "rom/ets_sys.h" // Include for ets_delay_us
#include "esp_log.h"

#define ESP_INTR_FLAG_DEFAULT 0
#define PIN_ZCD_IN GPIO_NUM_11
#define PIN_TRIAC_OUT GPIO_NUM_12
#define PULSE_WIDTH_US 10
#define GPTIMER_COUNT_DEFAULT 9000
#define GPTIMER_COUNT_TEST 1000

gptimer_handle_t gptimer_handle = NULL;

static bool gptimer_alarm_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx){
    gptimer_stop(timer); // Stop the timer
    gpio_set_level(PIN_TRIAC_OUT, 1);
    ets_delay_us(PULSE_WIDTH_US);  // Very short delay, check for conflicts, as a delay inside a callback is not recommended
                                   // Could be replaced with a queue like the gpio example
    gpio_set_level(PIN_TRIAC_OUT, 0);

    ESP_LOGI("GPTimer", "Alarm callback executed"); // Log the callback execution, for debugging purposes

    return true; // Return true to wake up a high priority task
}

static void IRAM_ATTR gpio_isr_handler(void* arg){
    // ISR handler code
    uint32_t gpio_num = (uint32_t)arg;
    if(gpio_num == PIN_ZCD_IN){
        // Handle the interrupt for the specific GPIO pin
        gptimer_set_raw_count(gptimer_handle, GPTIMER_COUNT_TEST); // Set the timer count
        gptimer_start(gptimer_handle); // Start the timer

    }
}

void configure_gpios(void){
    // Configure GPIOs
    gpio_config_t zcd_pin_conf = {
        .pin_bit_mask = 1ULL << PIN_ZCD_IN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&zcd_pin_conf); // Apply the configuration

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT); // Install ISR service
    gpio_isr_handler_add(PIN_ZCD_IN, gpio_isr_handler, (void*) PIN_ZCD_IN); // Add ISR handler

    gpio_config_t triac_pin_conf = {
        .pin_bit_mask = 1ULL << PIN_TRIAC_OUT,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&triac_pin_conf); // Apply the configuration
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
        .alarm_count = GPTIMER_COUNT_DEFAULT,
        .flags.auto_reload_on_alarm = false,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer_handle, &alarm_config));

    gptimer_event_callbacks_t callbacks = {
        .on_alarm = gptimer_alarm_callback,
    };

    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer_handle, &callbacks, NULL));
    ESP_ERROR_CHECK(gptimer_enable(gptimer_handle));
}

void app_main(void){
   configure_gpios();
   gptimer_config();
}
