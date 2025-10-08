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
#include "buck_interface.h"

#include <unistd.h>
#include <sys/param.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_attr.h"

// TIMER
#include "esp_timer.h"
#include "driver/gptimer.h"

// DISPLAY
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

// GPIO
#include "gpio_definition.h"

// SPI
#include "driver/spi_master.h"

// Extern libraries
#include "esp_lcd_ili9341.h"
#include "lvgl.h"
#include "encoder.h"

/*----------- DISPLAY DEFINES ------------*/
// Display spi configuration
#define LCD_HOST    SPI2_HOST

// Display parameters configuration
#define LCD_H_RES  240
#define LCD_V_RES  320
/*--------------------------------*/

/*------------ LVGL DEFINES --------------*/
#define LVGL_TICK_INCREMENT_MS 2  // LVGL tick increment in milliseconds
/*--------------------------------*/

const char *TAG = "UI_CONFIG";

/*------------ HANDLES -----------*/
static QueueHandle_t encoder_queue = NULL;  // Queue to handle rotary encoder events
static esp_lcd_panel_io_handle_t io_handle = NULL; // LCD panel IO handle
static esp_lcd_panel_handle_t panel_handle = NULL; // LCD panel handle
static esp_timer_handle_t lvgl_tick_timer = NULL;   // Timer handle for LVGL tick
static TaskHandle_t lvgl_port_task_handle = NULL;   // Task handle for LVGL port task
/*--------------------------------*/

/*----------- ENCODER VARIABLES -----------*/
static rotary_encoder_event_t encoder_event;  // Buffer to allocate events from the encoder
lv_indev_t *indev_encoder = NULL; // Input device for LVGL (encoder)
/*-----------------------------------------*/

/* --- VARIABLES TO NAVIGATE IN LVGL --- */
static int32_t accumulated_diff = 0;    // Accumulated difference from encoder turns
static lv_indev_state_t button_state = LV_INDEV_STATE_RELEASED; // Current button state
static bool button_event_pending = false; // Flag to track if button event needs to be reported
/*--------------------------------------*/

// Mutex for LVGL API calls (exported for use by modules)
_lock_t lvgl_api_lock;

/*-------- FUNCTION PROTOTYPES --------*/
static void lvgl_flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);    // LVGL flush callback
static void increase_lvgl_tick(void *arg);  // Function to increase LVGL tick
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);  // Notify LVGL when flush is done
static void read_encoder_callback(lv_indev_t *indev_drv, lv_indev_data_t *data);    // Read encoder state for LVGL
/*-------------------------------------*/

/*------- TASKS FUNCTION PROTOTYPES -------*/
static void lvgl_port_task(void *arg);  // Task to handle LVGL
/*----------------------------------------*/

void setup_user_interface(){

    encoder_queue = xQueueCreate(10, sizeof(rotary_encoder_event_t));
    if (encoder_queue == NULL) {
        ESP_LOGE("ENCODER", "Failed to create encoder queue");
        return;
    }

    ESP_ERROR_CHECK(rotary_encoder_init(encoder_queue));
    
    static rotary_encoder_t display_encoder ={
        .pin_a = PIN_ROTARY_DATA,
        .pin_b = PIN_ROTARY_CLK,
        .pin_btn = PIN_ROTARY_SW,
        .btn_pressed_time_us = 0,
        .btn_state = RE_BTN_RELEASED,
        .acceleration = {0, 0}
    };

    ESP_ERROR_CHECK(rotary_encoder_add(&display_encoder));

    spi_bus_config_t bus_config = {
        .sclk_io_num = PIN_TFT_SCK,      // SPI clock pin
        .mosi_io_num = PIN_TFT_MOSI,      // SPI MOSI pin
        .miso_io_num = -1,                // Not used
        .quadwp_io_num = -1,              // Not used
        .quadhd_io_num = -1,              // Not used
        .max_transfer_sz = 4096,          // Max transfer size in bytes
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO)); // Initialize SPI bus

    // Configure the SPI panel IO for the LCD
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_TFT_DC,        // Data/Command pin
        .cs_gpio_num = PIN_TFT_CS,        // Chip select pin
        .pclk_hz = 10 * 1000 * 1000,      // SPI clock frequency (10 MHz)
        .lcd_cmd_bits = 8,                // Command length in bits
        .lcd_param_bits = 8,              // Parameter length in bits
        .spi_mode = 0,                    // SPI mode 0
        .trans_queue_depth = 10,          // Transaction queue depth
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle)); // Create new SPI panel IO

    // Configure the LCD panel driver (ILI9341)
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_TFT_RESET,            // Reset pin
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

    xTaskCreatePinnedToCore(lvgl_port_task,
                "LVGL",
                4096,
                NULL,
                2,
                &lvgl_port_task_handle,
                0
                );
    

    ESP_LOGI(TAG, "User interface setup complete");
}

/**    lv_group_focus_obj(ui_freqScreen);
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
 * This function processes encoder events and updates LVGL input data.
 * Button state changes are only reported once per actual event to prevent
 * double-triggering during screen refreshes.
 * Limits event processing to prevent holding LVGL lock too long.
 * 
 * @param indev_drv 
 * @param data 
 */
static void read_encoder_callback(lv_indev_t *indev_drv, lv_indev_data_t *data){
    // Limit events processed per callback to reduce lock hold time
    const int MAX_EVENTS_PER_CALL = 5;
    int events_processed = 0;
    
    // Process pending events in the queue (up to limit)
    while (events_processed < MAX_EVENTS_PER_CALL && 
           xQueueReceive(encoder_queue, &encoder_event, 0) == pdTRUE){
        events_processed++;
        //printf("Event received: ");
        switch (encoder_event.type) {
            case RE_ET_CHANGED:
                //printf("Encoder turned, diff: %ld\n", encoder_event.diff);
                accumulated_diff += encoder_event.diff;
                break;
            case RE_ET_BTN_PRESSED:
                //printf("Button pressed\n");
                button_state = LV_INDEV_STATE_PRESSED;
                button_event_pending = true; // Mark that we have a new button event
                break;
            case RE_ET_BTN_RELEASED:
                //printf("Button released\n");
                button_state = LV_INDEV_STATE_RELEASED;
                button_event_pending = true; // Mark that we have a new button event
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
    
    // Report encoder rotation
    data->enc_diff = accumulated_diff;
    accumulated_diff = 0; // Reset after reading
    
    // Report button state only if there's a pending event
    if (button_event_pending) {
        data->state = button_state;
        button_event_pending = false; // Clear the flag after reporting once
    } else {
        // No new button event, maintain current state but don't trigger new actions
        data->state = LV_INDEV_STATE_RELEASED;
    }
    
    //printf("Slider value freq (Screen2): %ld\n", lv_slider_get_value(ui_SliderFreq1));
}