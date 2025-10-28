/**
 * @file module_connection.h
 * @brief Module Connection - Monitors physical connection state
 * 
 * This component monitors the physical connection of modules using GPIO.
 * It provides connection state and callbacks for state changes.
 */

#ifndef MODULE_CONNECTION_H
#define MODULE_CONNECTION_H


/**
 * @brief Connection states
 */
typedef enum {
    CONNECTION_INIT,            ///< Initializing
    CONNECTION_DISCONNECTED,    ///< No module connected
    CONNECTION_CONNECTED,       ///< Module connected and stable
    CONNECTION_FAULT,           ///< Error state
} module_connection_state_t;


/**
 * @brief Get the current connection state
 * 
 * @return Current connection state
 */
module_connection_state_t get_connection_state(void);

/**
 * @brief Initialize the module connection monitoring system
 * 
 * Sets up GPIO for connection detection and creates monitoring task
 */
void initialize_module_connection(void);


#endif // MODULE_CONNECTION_H
