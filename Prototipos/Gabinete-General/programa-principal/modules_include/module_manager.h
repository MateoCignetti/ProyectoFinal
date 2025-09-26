#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <stdint.h>
#include "module_state.h"


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

#endif