#include "inverter_control.h"
#include "inverter_interface.h"

// Module identification voltage range (in millivolts)
#define INVERTER_IDENT_MV_MIN 1600
#define INVERTER_IDENT_MV_MAX 1700

static void start_inverter_module();
static void stop_inverter_module();

// Public declarations
const module_t inverter_module = {
    .name = "Inverter Module",
    .ident_mv_min = INVERTER_IDENT_MV_MIN,
    .ident_mv_max = INVERTER_IDENT_MV_MAX,
    .start_function = start_inverter_module,
    .stop_function = stop_inverter_module,
};
//

// Private functions
static void start_inverter_module(){
    // Code to initialize and start the inverter module
    start_inverter_interface(); // Start the inverter interface (UI)
}

static void stop_inverter_module(){
    // Code to stop and deinitialize the inverter module
    stop_inverter_interface(); // Stop the inverter interface (UI)
}