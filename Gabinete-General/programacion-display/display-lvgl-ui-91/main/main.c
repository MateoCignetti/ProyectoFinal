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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lcd_ili9341.h"
#include "ui.h"

#define LCD_HOST    SPI2_HOST

/* VER LOS PINES*/
#define PIN_NUM_SCLK GPIO_NUM_10
#define PIN_NUM_MOSI GPIO_NUM_11
#define PIN_NUM_DC   GPIO_NUM_12
#define PIN_NUM_CS GPIO_NUM_14
#define PIN_NUM_RST GPIO_NUM_13

#define LCD_H_RES  240
#define LCD_V_RES  320
#define PARALLEL_LINES     16
#define ROTATE_FRAME       30

static _lock_t lvgl_api_lock;

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx){
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void lvgl_flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map){
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // because SPI LCD is big-endian, we need to swap the RGB bytes order
    lv_draw_sw_rgb565_swap(px_map, (offsetx2 + 1 - offsetx1) * (offsety2 + 1 - offsety1));
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
}

static void increase_lvgl_tick(void *arg){
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(2);
}

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

void app_main(void){
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
    esp_lcd_panel_io_handle_t io_handle = NULL;
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
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,            // Reset pin
        .color_space = LCD_RGB_ELEMENT_ORDER_BGR, // Color space (BGR)
        .bits_per_pixel = 16,                     // 16 bits per pixel
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle)); // Create new ILI9341 panel

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));      // Reset the panel
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));       // Initialize the panel

    // OPCIÓN 3: Rotación 180° (portrait invertido)
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true)); // Turn on display
    //ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false)); // Set color inversion off
    //ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true)); // Swap X and Y axes
    // ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false)); // Mirror X axis
    //ESP_ERROR_CHECK(gpio_set_level(PIN_NUM_BACKLIGHT, 1)); // Turn on backlight (uncomment if needed)

    // Initialize LVGL
    lv_init();

    // Create display for LVGL
    lv_display_t * display = lv_display_create(LCD_H_RES, LCD_V_RES);

    // LVGL will render to this 1/10 screen sized buffer for 2 bytes/pixel
    size_t draw_buffer_size = LCD_H_RES * LCD_V_RES / 10 * 2;
    // Use DMA-capable memory for the buffer
    void *buffer = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_size, 0);
    assert(buffer);

    lv_display_set_buffers(display, buffer, NULL, draw_buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_display_set_user_data(display, panel_handle);

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

    lv_display_set_flush_cb(display, lvgl_flush_callback);

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;

    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 2 * 1000)); // Call every 2ms

    const esp_lcd_panel_io_callbacks_t cbs ={
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display));
    
    xTaskCreate(
        lvgl_port_task,
        "LVGL",
        4096,
        NULL,
        2,
        NULL
    );

    _lock_acquire(&lvgl_api_lock);
    ui_init();
    _lock_release(&lvgl_api_lock);
}

