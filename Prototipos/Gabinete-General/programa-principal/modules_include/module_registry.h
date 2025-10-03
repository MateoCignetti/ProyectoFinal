/**
 * @file module_registry.h
 * @brief Central registry for all module implementations
 * 
 * This file serves as the single point where all modules are registered.
 * When adding a new module, simply:
 * 1. Create your module's .c and .h files (following MODULE_TEMPLATE.md)
 * 2. Add #include "your_module.h" below
 * 3. Add &your_module_ident to MODULE_REGISTRY_LIST
 * 
 * That's it! No need to modify any other files.
 * 
 * @author Mateo Antonio Cignetti
 * @date 2025-10-02
 * @copyright Copyright (c) 2025
 */

#ifndef MODULE_REGISTRY_H
#define MODULE_REGISTRY_H

#include "module_manager.h"

// ============================================================================
// STEP 1: Include all module headers here
// ============================================================================
// Each module header should declare:
// - extern const module_t <module_name>_module;

#include "dimmer_control.h"
#include "buck_control.h"
// Add more module includes here as they are developed
// Example:
// #include "inverter_control.h"


// ============================================================================
// STEP 2: Add your module to the list below
// ============================================================================
/**
 * Module Registry
 * 
 * Add your module struct reference here.
 * The module manager will automatically iterate through this list
 * to identify connected modules based on their ident_mv_min/max values.
 */

// For testing without actual modules, remove later
const module_t inverter_module = {
    .name = "Inverter Module",
    .ident_mv_min = 1600,
    .ident_mv_max = 1700,
    .start_function = NULL,
    .stop_function = NULL,
};

#define MODULE_REGISTRY_LIST \
    &dimmer_module,          \
    &inverter_module,       \
    &buck_module,          \
    /* Add more modules here: */ \
    /* &boost_module, */ \
    /* &pwm_inverter_module, */     \
    NULL  /* Sentinel - do not remove */

#endif // MODULE_REGISTRY_H
