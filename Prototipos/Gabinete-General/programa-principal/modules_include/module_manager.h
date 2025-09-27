#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <stdint.h>
#include "module.h"

typedef enum {
    MANAGER_INIT,
    MANAGER_READY,
    MANAGER_RUNNING,
    MANAGER_STOPPING,
    MANAGER_FAULT,
} module_manager_state_t;

typedef struct {
    const char* name; // Name of the module
    void (*start_function)(void); // Function pointer to start the module
    void (*stop_function)(void); // Function pointer to stop the module
} module_t;

typedef struct{
    uint16_t module_ident_mv_min; // Minimum voltage for module identification
    uint16_t module_ident_mv_max; // Maximum voltage for module identification
    const module_t *module; // Pointer to the module structure
} module_ident_t;

void module_manager_init(void); // Initialize the module manager

module_manager_state_t get_module_manager_state(); // Get the current state of the module manager
void set_module_manager_state(module_manager_state_t new_state); // Set a new state for the module
extern SemaphoreHandle_t xModuleManagerMutex; // Mutex for protecting module manager state access
extern TaskHandle_t xTaskModuleManagerUpdate_handle; // Task handle for the module manager update task


#endif