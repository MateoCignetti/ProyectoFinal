#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

typedef enum {
    MANAGER_INIT,
    MANAGER_READY,
    MANAGER_RUNNING,
    MANAGER_STOPPING,
    MANAGER_FAULT,
} module_manager_state_t;

typedef struct {
    const char* name; // Name of the module
    uint16_t ident_mv_min; // Minimum voltage for module identification
    uint16_t ident_mv_max; // Maximum voltage for module identification
    void (*start_function)(void); // Function pointer to start the module
    void (*stop_function)(void); // Function pointer to stop the module
} module_t;

void module_manager_init(void); // Initialize the module manager

module_manager_state_t get_module_manager_state(); // Get the current state of the module manager

#endif