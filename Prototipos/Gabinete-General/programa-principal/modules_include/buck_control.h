#ifndef BUCK_CONTROL_H
#define BUCK_CONTROL_H

#include "module_manager.h"

// Control modes for the buck module
typedef enum {
    CONTROL_MODE_IDLE,
    CONTROL_MODE_FIXED_PWM,
    CONTROL_MODE_PID,
    CONTROL_MODE_FAULT
} control_mode_t;

// Public API functions
control_mode_t get_control_mode(void);
void set_control_mode(control_mode_t new_mode);

extern const module_t buck_module; // Declaration of the buck module

#endif // BUCK_CONTROL_H