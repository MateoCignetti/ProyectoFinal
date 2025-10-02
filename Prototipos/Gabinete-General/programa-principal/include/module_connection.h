#ifndef MODULE_CONNECTION_H
#define MODULE_CONNECTION_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

typedef enum {
    CONNECTION_INIT,
    CONNECTION_DISCONNECTED,
    CONNECTION_DETECTING,
    CONNECTION_CONNECTED,
    CONNECTION_FAULT,
} module_connection_state_t;

// Event flags for task notifications
#define CONNECTION_EVENT_CHANGE  (1 << 0)
#define CONNECTION_EVENT_STABLE  (1 << 1)

// Callback function type for connection state changes
typedef void (*connection_callback_t)(bool connected);

// Public functions
void initialize_module_connection(void);
void register_connection_callback(connection_callback_t callback);
module_connection_state_t get_connection_state(void);

#endif