/**
 * @file module_connection.h
 * @brief Module Connection - Monitors physical connection state
 * 
 * This component monitors the physical connection of modules using GPIO.
 * It provides connection state and callbacks for state changes.
 */

#ifndef MODULE_CONNECTION_H
#define MODULE_CONNECTION_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Connection states
 */
typedef enum {
    CONNECTION_INIT,            ///< Initializing
    CONNECTION_DISCONNECTED,    ///< No module connected
    CONNECTION_DETECTING,       ///< Module detection in progress
    CONNECTION_CONNECTED,       ///< Module connected and stable
    CONNECTION_FAULT,           ///< Error state
} module_connection_state_t;

/**
 * @brief Event flags for task notifications
 */
#define CONNECTION_EVENT_CHANGE  (1 << 0)  ///< Connection state changed
#define CONNECTION_EVENT_STABLE  (1 << 1)  ///< Connection is stable

/**
 * @brief Callback function type for connection state changes
 * 
 * @param connected true if module is connected, false if disconnected
 */
typedef void (*connection_callback_t)(bool connected);

/**
 * @brief Initialize the module connection monitoring system
 * 
 * Sets up GPIO for connection detection and creates monitoring task
 */
void initialize_module_connection(void);

/**
 * @brief Register a callback for connection state changes
 * 
 * @param callback Function to call when connection state changes
 */
void register_connection_callback(connection_callback_t callback);

/**
 * @brief Get the current connection state
 * 
 * @return Current connection state
 */
module_connection_state_t get_connection_state(void);

/**
 * @brief Check if a module is currently connected
 * 
 * @return true if connected, false otherwise
 */
bool is_module_connected(void);

#ifdef __cplusplus
}
#endif

#endif // MODULE_CONNECTION_H
