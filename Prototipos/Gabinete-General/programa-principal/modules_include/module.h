#ifndef MODULE_STATE_H
#define MODULE_STATE_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

typedef enum {
    INIT,
    DISCONNECTED,
    CONNECTED,
    //RUNNING,
    //STOPPING,
    FAULT,
} module_state_t;

// Public functions
module_state_t get_module_state();
void set_module_state(module_state_t new_state);
void initialize_module_state();

// External mutex declaration
extern SemaphoreHandle_t xModuleStateMutex;

#endif