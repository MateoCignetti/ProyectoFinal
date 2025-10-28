#include "dimmer_interface.h"

#include <sys/lock.h>
#include "esp_log.h"
#include "lvgl.h"
#include "module_manager.h"
#include "dimmer_ui.h"
#include "ui_config.h"
#include "idle_screen.h"
// REVISAR TEMA DE ARCHIVOS A INCLUIR

static const char* TAG = "Dimmer Interface";

// Mutex for LVGL API calls - defined in dimmer_ui_config.c
extern _lock_t lvgl_api_lock;

// Task handle for cleanup
static TaskHandle_t xTaskUpdateGroups_handle = NULL;

// Shutdown flag for graceful task termination
static volatile bool shutdown_requested = false;

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

static void create_groups_for_ui(void);
static void destroy_groups_for_ui(void);
static void vTaskUpdateGroups(void *pvParameters);

void start_dimmer_interface(){
    // Initialize shutdown flag
    shutdown_requested = false;

    xTaskCreate(vTaskUpdateGroups,
            "UpdateGroups",
            configMINIMAL_STACK_SIZE * 4,
            NULL,
            tskIDLE_PRIORITY + 1,
            &xTaskUpdateGroups_handle
            );

    _lock_acquire(&lvgl_api_lock);
    // CRITICAL: Initialize new UI BEFORE destroying old screen
    // This prevents deleting screens while their animations are still active
    dimmer_ui_init();
    create_groups_for_ui();
    stop_idle_screen();  // Now safe to destroy idle screen
    _lock_release(&lvgl_api_lock);
}

void stop_dimmer_interface(){
    // Signal the task to shutdown gracefully
    if (xTaskUpdateGroups_handle != NULL) {
        shutdown_requested = true;

        // Wait for task to self-delete with timeout protection
        eTaskState task_state = eTaskGetState(xTaskUpdateGroups_handle);

        while (task_state != eDeleted) {
            vTaskDelay(pdMS_TO_TICKS(10));
            task_state = eTaskGetState(xTaskUpdateGroups_handle);
        }
        ESP_LOGI(TAG, "UpdateGroups task exited gracefully");

        xTaskUpdateGroups_handle = NULL;
    }

    _lock_acquire(&lvgl_api_lock);

    // CRITICAL: Create new screen BEFORE destroying old UI
    // This prevents deleting screens while their animations are still active
    start_idle_screen();

    // Destroy groups (removes references to UI objects)
    destroy_groups_for_ui();

    // NOW safe to destroy the old UI
    dimmer_ui_destroy();

    // Clean up the initial actions object to prevent memory leak
    if(dimmer_ui____initial_actions0) {
        lv_obj_del(dimmer_ui____initial_actions0);
        dimmer_ui____initial_actions0 = NULL;
    }

    _lock_release(&lvgl_api_lock);

    ESP_LOGI(TAG, "Dimmer interface stopped, blank screen loaded");
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
    lv_group_add_obj(groups[SCREEN_1], dimmer_ui_ButtonAnalogical);
    lv_group_add_obj(groups[SCREEN_1], dimmer_ui_ButtonDigital);

    lv_obj_add_event_cb(dimmer_ui_ButtonAnalogical, dimmer_ui_event_ButtonAnalogical, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(dimmer_ui_ButtonDigital, dimmer_ui_event_ButtonDigital, LV_EVENT_CLICKED, NULL);

    // Add interactive objects to groups[SCREEN_2] group
    lv_group_add_obj(groups[SCREEN_2], dimmer_ui_ButtonReturn1);
    lv_obj_add_event_cb(dimmer_ui_ButtonReturn1, dimmer_ui_event_ButtonReturn1, LV_EVENT_CLICKED, NULL);

    // Add interactive objects to groups[SCREEN_3] group
    lv_group_add_obj(groups[SCREEN_3], dimmer_ui_ButtonReturn3);
    lv_group_add_obj(groups[SCREEN_3], dimmer_ui_SliderCC);
    lv_obj_add_event_cb(dimmer_ui_ButtonReturn3, dimmer_ui_event_ButtonReturn3, LV_EVENT_CLICKED, NULL);
    //lv_obj_add_event_cb(dimmer_ui_SliderCC, dimmer_ui_event_SliderCC, LV_EVENT_VALUE_CHANGED, NULL);


    current_group = groups[SCREEN_1];  // Grupo inicial
    current_screen = SCREEN_1;
    lv_group_set_default(groups[current_screen]);

    // Set the encoder to control this group
    if (indev_encoder != NULL) {
        lv_indev_set_group(indev_encoder, groups[current_screen]);
        ESP_LOGI(TAG, "Encoder assigned to group for SCREEN_1");
    } else {
        ESP_LOGE(TAG, "indev_encoder is NULL! Cannot assign to group");
    }
}

/**
 * @brief Destroy all LVGL groups created for the UI
 *
 */
static void destroy_groups_for_ui(void){
    // Delete all groups
    for(int i = 0; i < SCREEN_COUNT; i++){
        if(groups[i] != NULL){
            lv_group_delete(groups[i]);
            groups[i] = NULL;
        }
    }
    current_group = NULL;
    current_screen = SCREEN_1;
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
        // Check for shutdown request before acquiring lock
        if (shutdown_requested) {
            ESP_LOGI(TAG, "UpdateGroups task shutting down gracefully");
            vTaskDelete(NULL); // Delete self
            return;
        }

        _lock_acquire(&lvgl_api_lock);
        // Determine the currently active screen
        lv_obj_t *active_screen = lv_screen_active();
        screen_state_t new_screen = current_screen;

        // Mapear pantalla activa a screen_state_t
        if (active_screen == dimmer_ui_Screen1) {
            new_screen = SCREEN_1;
        } else if (active_screen == dimmer_ui_Screen2) {
            new_screen = SCREEN_2;
        } else if (active_screen == dimmer_ui_Screen3) {
            new_screen = SCREEN_3;
        }
        // Actualizar grupo solo si cambió la pantalla
        if (new_screen != current_screen) {
            current_screen = new_screen;
            current_group = groups[current_screen];
            lv_group_set_default(current_group);
            lv_indev_set_group(indev_encoder, groups[current_screen]);
            lv_group_focus_obj(lv_obj_get_child(active_screen, 0)); // Focus on first object of new screen
            //printf("Cambié a pantalla %d\n", current_screen);
        }
        _lock_release(&lvgl_api_lock);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(xFrequency));
    }
}
