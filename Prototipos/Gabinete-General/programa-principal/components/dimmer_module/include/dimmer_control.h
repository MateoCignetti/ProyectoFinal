#ifndef DIMMER_H
#define DIMMER_H

#include "module_manager.h"

extern const module_t dimmer_module; // Declaration of the dimmer module

typedef enum {
    DIMMER_CONTROL_IDLE,
    DIMMER_CONTROL_ANALOG,
    DIMMER_CONTROL_FULL_WAVE,
} dimmer_control_state_t;

void set_dimmer_control_state(dimmer_control_state_t state);
dimmer_control_state_t get_dimmer_control_state(void);

#endif