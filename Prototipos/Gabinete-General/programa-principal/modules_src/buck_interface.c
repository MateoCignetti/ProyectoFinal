#include "buck_interface.h"

#include <sys/lock.h>
#include "lvgl.h"
#include "module_manager.h"
#include "ui.h"

// Module definition
/*const module_t buck_module = {
    .name = "Buck Converter",
    .ident_mv_min = 1900,
    .ident_mv_max = 2000,
    .start_function = start_buck_interface,
    .stop_function = stop_buck_interface,
};*/

// Mutex for LVGL API calls 
static _lock_t lvgl_api_lock;

// Task handle for cleanup
static TaskHandle_t xTaskUpdateGroups_handle = NULL;

// Input device for LVGL (encoder)
static lv_indev_t *indev_encoder = NULL; // Input device for LVGL (encoder)

extern lv_indev_t *indev_encoder; // Input device for LVGL (encoder)

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

void start_buck_interface(){
    xTaskCreate(vTaskUpdateGroups,
            "UpdateGroups",
            configMINIMAL_STACK_SIZE * 4,
            NULL,
            tskIDLE_PRIORITY + 1,
            &xTaskUpdateGroups_handle
            );

    _lock_acquire(&lvgl_api_lock);
    ui_init();
    create_groups_for_ui();
    _lock_release(&lvgl_api_lock);
}

void stop_buck_interface(){
    // Delete the task first
    if (xTaskUpdateGroups_handle != NULL) {
        vTaskDelete(xTaskUpdateGroups_handle);
        xTaskUpdateGroups_handle = NULL;
    }
    
    // Clean up LVGL objects
    _lock_acquire(&lvgl_api_lock);
    destroy_groups_for_ui();
    ui_destroy();
    _lock_release(&lvgl_api_lock);
}

/**
 * @brief Create a groups for ui object
 * 
 */
static void create_groups_for_ui(void){
    for(int i=0; i < SCREEN_COUNT; i++){
    lv_obj_add_event_cb(ui_freqScreen, ui_event_freqScreen, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_controlScreen, ui_event_controlScreen, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_loadScreen, ui_event_loadScreen, LV_EVENT_CLICKED, NULL);
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
    lv_group_focus_obj(ui_freqScreen);
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