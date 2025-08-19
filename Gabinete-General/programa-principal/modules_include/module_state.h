#ifndef MODULE_STATE_H
#define MODULE_STATE_H

typedef enum {
    INIT,
    DISCONNECTED,
    CONNECTED,
    RUNNING,
    STOPPING,
    FAULT,
} module_state_t;

#endif