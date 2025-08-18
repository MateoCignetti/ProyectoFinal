#include "module_manager.h"
#include "dimmer_control.h"

#include <stdio.h>

static const char* TAG = "Module Manager"; // Module name for logging

static const module_ident_t module_ident_list[] = {
    // Add module identification entries here
    // Example:
    // { .module_ident_mv_min = 1000, .module_ident_mv_max = 2000, .module = &my_module },
    {2000, 2200, &dimmer_module},

    {0, 0, NULL} // Sentinel value to mark the end of the list
};

void module_manager_init(void){
    // Initialize the module manager
}

static const module_t* identify_module(uint16_t voltage_mv) {
    uint16_t module_ident_mv; // CHANGE TO READ ADC

    for (int i = 0; module_ident_list[i].module != NULL; i++) {
        if (module_ident_mv >= module_ident_list[i].module_ident_mv_min && module_ident_mv <= module_ident_list[i].module_ident_mv_max) {
            return module_ident_list[i].module; // Return the identified module
        }
    }

    ESP_LOGW(TAG, "No module identified for voltage: %d mV", voltage_mv); // Log warning if no module is identified
    return NULL; // No module identified
}

