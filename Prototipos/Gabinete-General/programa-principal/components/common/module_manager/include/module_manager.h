/**
 * @file module_manager.h
 * @brief Module Manager - Handles module detection and lifecycle
 * 
 * This component is responsible for:
 * - Detecting which module is connected via identification voltage
 * - Managing module lifecycle (initialization, start, stop)
 * - Coordinating between module_connection and actual modules
 * - Maintaining module registry
 */

#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/**
 * @brief Module manager states
 */
typedef enum {
    MANAGER_INIT,       ///< Initializing
    MANAGER_READY,      ///< Ready, no module detected
    MANAGER_STARTING,   ///< Starting module identification
    MANAGER_RUNNING,    ///< Module is running
    MANAGER_STOPPING,   ///< Stopping current module
    MANAGER_FAULT,      ///< Error state
} module_manager_state_t;

void set_module_manager_state(module_manager_state_t new_state);

/**
 * @brief Module definition structure
 * 
 * Each module must provide this structure to be registered
 */
typedef struct {
    const char* name;                   ///< Module name
    uint16_t ident_mv_min;             ///< Minimum identification voltage (mV)
    uint16_t ident_mv_max;             ///< Maximum identification voltage (mV)
    void (*start_function)(void);      ///< Function to start the module
    void (*stop_function)(void);       ///< Function to stop the module
} module_t;

/**
 * @brief Initialize the module manager
 * 
 * Sets up ADC for module identification, creates tasks,
 * and initializes the connection monitoring system.
 */
void module_manager_init(void);

/**
 * @brief Get the current state of the module manager
 * 
 * @return Current manager state
 */
module_manager_state_t get_module_manager_state(void);

/**
 * @brief Get the currently active module
 * 
 * @return Pointer to current module_t, or NULL if no module active
 */
const module_t* get_current_module(void);

#endif // MODULE_MANAGER_H
