/**
 * @file main.c
 * @author Mateo Antonio Cignetti (mateo@cignetti.ar)
 * @brief Isolated dimmer control for the dimmer prototype, using two gptimers to avoid blocking tasks
 * @version 0.9
 * @date 2025-05-25
 * 
 * @copyright Copyright (c) 2025
 * 
*/

#include "driver/gpio.h" // GPIO driver
#include "driver/gptimer.h" // Driver for the gptimers
#include "esp_attr.h" // Needed for IRAM_ATTR (needed for interrupt handler)
#include "esp_log.h" // ESP-IDF logging library

// Generates a test 50 Hz square wave signal on the defined pin below,
// to test the dimmer without the need of a real zero crossing detector.
// Only for testing without the dimmer prototype connected.
#define USE_TEST_ZCD_SIGNAL 1

#if USE_TEST_ZCD_SIGNAL
#include "driver/ledc.h"
#define PIN_TEST_ZCD_SIGNAL GPIO_NUM_10
#define TEST_SIGNAL_FREQUENCY 50
#endif
//

#define ESP_INTR_FLAG_DEFAULT 0 // Default interrupt flag

#define PIN_ZCD_IN GPIO_NUM_11 // Zero crossing detector input pin
#define PIN_TRIAC_OUT GPIO_NUM_12 // TRIAC output pin
#define PULSE_WIDTH_US 25 // Pulse width for driving the triac, in microseconds
#define DIMMER_TIMER_COUNT_DEFAULT 9900 // Turns on TRIAC at 9,9 ms after zero crossing
                                        // (basically starts the dimmer off)

static gptimer_handle_t dimmer_wait_timer = NULL;
static gptimer_handle_t dimmer_pulse_timer = NULL;

static bool timer_is_running = false; // Aux flag to ensure proper timer operation, shouldn't be needed
                                      // under normal operation, but it's a safety measure (I guess?)
static uint16_t dimmer_wait_alarm_count = DIMMER_TIMER_COUNT_DEFAULT; // Static variable to store the alarm count



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
    }
    
}

// Function to configure the GPIOs (pins, directions, interrupts, etc.)
void configure_gpios(void){
    
    // Configuration for the zero crossing detector pin
    gpio_config_t zcd_pin_conf = {
        .pin_bit_mask = 1ULL << PIN_ZCD_IN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&zcd_pin_conf)); // Apply the configuration

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT)); // Install ISR service
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_ZCD_IN, zcd_isr_handler, NULL)); // Add ISR handler for the ZCD pin

    // Configuration for the TRIAC pin
    gpio_config_t triac_pin_conf = {
        .pin_bit_mask = 1ULL << PIN_TRIAC_OUT,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&triac_pin_conf)); // Apply the configuration
}

// Function to configure the timers
void configure_timers(void){

    // Configuration for both timers (couting up, and 1 us resolution)
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

// Function to configure the test signal
#if USE_TEST_ZCD_SIGNAL
void configure_test_signal(void){

    // Configure the ledc timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = TEST_SIGNAL_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Configure the ledc channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .gpio_num = PIN_TEST_ZCD_SIGNAL,
        .duty = 512, // 50% duty cycle
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}
#endif

void app_main(void){
    
    // Configure the test signal if enabled
    #if USE_TEST_ZCD_SIGNAL
    configure_test_signal();
    #endif
    //

    configure_timers(); // Configure the timers
    configure_gpios(); // Configure the GPIOs
}
