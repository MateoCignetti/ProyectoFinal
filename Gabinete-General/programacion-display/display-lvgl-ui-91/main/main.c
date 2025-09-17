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
#include "encoder.h"
#include "ui.h"

// Rotary encoder pin configuration
#define PIN_NUM_CLOCKWISE GPIO_NUM_48
#define PIN_NUM_COUNTERCLOCKWISE GPIO_NUM_47
#define PIN_NUM_BUTTON GPIO_NUM_21

/* Display pin configuration */
#define LCD_HOST    SPI2_HOST
#define PIN_NUM_SCLK GPIO_NUM_10
#define PIN_NUM_MOSI GPIO_NUM_11
#define PIN_NUM_DC   GPIO_NUM_12
#define PIN_NUM_CS GPIO_NUM_14
#define PIN_NUM_RST GPIO_NUM_13

// Display parameters configuration
#define LCD_H_RES  240
#define LCD_V_RES  320
#define PARALLEL_LINES     16
#define ROTATE_FRAME       30

// LVGL related defines
#define LVGL_TICK_INCREMENT_MS 2  // LVGL tick increment in milliseconds

// Handles
QueueHandle_t encoder_queue = NULL;  // Queue to handle rotary encoder events
rotary_encoder_event_t encoder_event;  // Buffer to allocate events from the encoder
esp_lcd_panel_io_handle_t io_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;

// Encoder variables
lv_indev_t *indev_encoder = NULL; // Input device for LVGL

// Variable para acumular diff desde última lectura
static int32_t accumulated_diff = 0;
static lv_indev_state_t button_state = LV_INDEV_STATE_PRESSED;

// Display screen variables
//lv_group_t *screen1 = NULL;
//lv_group_t *screen2 = NULL;
//lv_group_t *screen3 = NULL;
//lv_group_t *screen4 = NULL;
//lv_group_t *screen5 = NULL;
static lv_group_t *current_group;  // Current active group

// State for screen management
typedef enum {
    SCREEN_1,
    SCREEN_2,
    SCREEN_3,
    SCREEN_4,
    SCREEN_5,
    SCREEN_COUNT
} screen_state_t;

static lv_group_t *groups[SCREEN_COUNT];
static screen_state_t current_screen = SCREEN_1;

// Mutex for LVGL API calls 
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
    lv_tick_inc(LVGL_TICK_INCREMENT_MS);
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

/*                if(accumulated_diff == 1){
                    // Move to next object in the current group
                    lv_group_focus_next(current_group);
                } else if(accumulated_diff == -1){
                    // Move to previous object in the current group
                    lv_group_focus_prev(current_group);
                }*/

void read_encoder_callback(lv_indev_t *indev_drv, lv_indev_data_t *data){
    if (xQueueReceive(encoder_queue, &encoder_event, 0) == pdTRUE){
        printf("Event received: ");
        switch (encoder_event.type) {
            case RE_ET_CHANGED:
                printf("Encoder turned, diff: %ld\n", encoder_event.diff);
                accumulated_diff += encoder_event.diff;
                //if(accumulated_diff == 1){
                //    // Move to next object in the current group
                //    lv_group_focus_next(groups[current_screen]);
                //} else{
                //    // Move to previous object in the current group
                //    lv_group_focus_prev(groups[current_screen]);
                //}
                break;
            case RE_ET_BTN_PRESSED:
                printf("Button pressed\n");
                button_state = LV_INDEV_STATE_PRESSED;
                break;
            case RE_ET_BTN_RELEASED:
                printf("Button released\n");
                button_state = LV_INDEV_STATE_RELEASED;
                break;
            case RE_ET_BTN_LONG_PRESSED:
                printf("Button long pressed\n");
                break;
            case RE_ET_BTN_CLICKED:
                printf("Button clicked\n");
                break;
            default:
                printf("Unknown event\n");
                break;
        }
    }
    data->enc_diff = accumulated_diff;
    data->state = button_state;
    accumulated_diff = 0; // Reset after reading
}

static void vTaskUpdateGroups(void *pvParameters){
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 100;
    // Initialise the xLastWakeTime variable with the current time.
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
        }
        // Actualizar grupo solo si cambió la pantalla
        if (new_screen != current_screen) {
            current_screen = new_screen;
            //current_group = groups[current_screen];
            lv_group_set_default(groups[current_screen]);
            lv_indev_set_group(indev_encoder, groups[current_screen]);
            printf("Cambié a pantalla %d\n", current_screen);
        }
        uint32_t count = lv_group_get_obj_count(groups[current_screen]);
        printf("Cantidad de objetos: %ld\n", count);
        _lock_release(&lvgl_api_lock);

        vTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS(xFrequency));
    }
}

static void create_groups_for_ui(void){
    for(int i=0; i < SCREEN_COUNT; i++){
        groups[i] = lv_group_create();
    }
    
    // Add interactive objects to groups[SCREEN_1] group
    lv_group_add_obj(groups[SCREEN_1], ui_Function1);
    lv_group_add_obj(groups[SCREEN_1], ui_Function2);
    lv_group_add_obj(groups[SCREEN_1], ui_Function3);
    lv_group_add_obj(groups[SCREEN_1], ui_Function4);

    lv_obj_add_flag(ui_Function1, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(ui_Function2, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(ui_Function3, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(ui_Function4, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(ui_Function1, ui_event_Function1, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_Function2, ui_event_Function2, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_Function3, ui_event_Function3, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_Function4, ui_event_Function4, LV_EVENT_PRESSED, NULL);

    // Add interactive objects to groups[SCREEN_2] group
    lv_group_add_obj(groups[SCREEN_2], ui_Button2);
    lv_group_add_obj(groups[SCREEN_2], ui_Slider2);

    lv_obj_add_flag(ui_Button2, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(ui_Slider2, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(ui_Button2, ui_event_Button2, LV_EVENT_PRESSED, NULL);
    //lv_obj_add_event_cb(ui_Slider2, ui_event_Slider2, LV_EVENT_VALUE_CHANGED, NULL);

    // Add interactive objects to groups[SCREEN_3] group
    lv_group_add_obj(groups[SCREEN_3], ui_Button3);
    lv_group_add_obj(groups[SCREEN_3], ui_Slider3);

    lv_obj_add_flag(ui_Button3, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(ui_Slider3, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(ui_Button3, ui_event_Button3, LV_EVENT_PRESSED, NULL);
    //lv_obj_add_event_cb(ui_Slider3, ui_event_Slider3, LV_EVENT_VALUE_CHANGED, NULL);

    // Add interactive objects to groups[SCREEN_4] group
    lv_group_add_obj(groups[SCREEN_4], ui_Button1);
    lv_group_add_obj(groups[SCREEN_4], ui_Slider4);

    lv_obj_add_flag(ui_Button1, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(ui_Slider4, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(ui_Button1, ui_event_Button1, LV_EVENT_PRESSED, NULL);
    //lv_obj_add_event_cb(ui_Slider4, ui_event_Slider4, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Add interactive objects to groups[SCREEN_5] group
    lv_group_add_obj(groups[SCREEN_5], ui_Button4);
    lv_group_add_obj(groups[SCREEN_5], ui_Button5);
    lv_group_add_obj(groups[SCREEN_5], ui_Button6);

    lv_obj_add_flag(ui_Button4, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(ui_Button5, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(ui_Button6, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(ui_Button4, ui_event_Button4, LV_EVENT_PRESSED, NULL);
    //lv_obj_add_event_cb(ui_Button5, ui_event_Button5, LV_EVENT_PRESSED, NULL);
    //lv_obj_add_event_cb(ui_Button6, ui_event_Button6, LV_EVENT_PRESSED, NULL);

    //current_group = groups[SCREEN_1];  // Grupo inicial
    current_screen = SCREEN_1;
    lv_group_set_default(groups[current_screen]);
    lv_indev_set_group(indev_encoder, groups[current_screen]);
    lv_group_focus_obj(ui_Function1);

}

void app_main(void){
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
        .btn_state = RE_BTN_PRESSED,
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

    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true)); // Turn on display

    // Initialize LVGL
    lv_init();

    // Create display for LVGL
    lv_display_t * display = lv_display_create(LCD_H_RES, LCD_V_RES);
    // LVGL will render to this 1/10 screen sized buffer for 2 bytes/pixel
    size_t draw_buffer_size = LCD_H_RES * LCD_V_RES / 10 * 2;
    // Use DMA-capable memory for the buffer
    void *buffer = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_size, 0);
    //assert(buffer);
    if (buffer == NULL) {
        ESP_LOGE("MAIN", "Failed to allocate DMA buffer");
        return;
    }

    lv_display_set_buffers(display, buffer, NULL, draw_buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_display_set_user_data(display, panel_handle);

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

    lv_display_set_flush_cb(display, lvgl_flush_callback);

    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_0);

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
    
    // Create input device for LVGL
    indev_encoder = lv_indev_create();
    lv_indev_set_type(indev_encoder, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev_encoder, read_encoder_callback);

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
    //lv_scr_load(ui_Screen1);
    _lock_release(&lvgl_api_lock);

    while (true){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}

