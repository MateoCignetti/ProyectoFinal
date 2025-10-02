/**
 * @file MODULE_TEMPLATE.md
 * @brief Template and guide for creating new module control programs
 * @author Mateo Antonio Cignetti
 * @date 2025-10-02
 * @copyright Copyright (c) 2025
 */

# How to Add a New Module

This guide explains how to add a new module control program to the system.

## Step-by-Step Guide

### 1. Create Module Header File (`modules_include/your_module.h`)

```c
#ifndef YOUR_MODULE_H
#define YOUR_MODULE_H

#include "module_manager.h"

// Module identification voltage range (in millivolts)
// These values are measured from the module's identification resistor divider
#define YOUR_MODULE_IDENT_MV_MIN 1000  // Replace with your actual min voltage
#define YOUR_MODULE_IDENT_MV_MAX 1100  // Replace with your actual max voltage

// Public declarations
extern const module_t your_module;

// Public functions (if any)
// void your_module_set_parameter(int value);

#endif // YOUR_MODULE_H
```

### 2. Create Module Source File (`modules_src/your_module.c`)

```c
/**
 * @file your_module.c
 * @brief Control program for your module
 * @version 0.1
 * @date 2025-XX-XX
 * @copyright Copyright (c) 2025
 */

#include "your_module.h"
#include "gpio_definition.h"
#include "esp_log.h"

static const char* MODULE_TAG = "Your Module";
static bool isModuleRunning = false;

// Private function prototypes
static void start_your_module(void);
static void stop_your_module(void);

// Public declarations - REQUIRED
const module_t your_module = {
    .name = "Your Module Name",
    .ident_mv_min = YOUR_MODULE_IDENT_MV_MIN,
    .ident_mv_max = YOUR_MODULE_IDENT_MV_MAX,
    .start_function = start_your_module,
    .stop_function = stop_your_module,
};

// Private functions
static void start_your_module(void) {
    if (!isModuleRunning) {
        isModuleRunning = true;
        ESP_LOGI(MODULE_TAG, "Starting your module...");
        
        // TODO: Initialize your module here
        // - Configure GPIOs
        // - Set up timers/PWM
        // - Initialize communication peripherals
        // - etc.
        
    } else {
        ESP_LOGW(MODULE_TAG, "Module already running, ignoring start request.");
    }
}

static void stop_your_module(void) {
    if (isModuleRunning) {
        isModuleRunning = false;
        ESP_LOGI(MODULE_TAG, "Stopping your module...");
        
        // TODO: Clean up your module here
        // - Disable interrupts
        // - Stop timers
        // - Reset GPIOs to safe state
        // - Free allocated resources
        // - etc.
        
    } else {
        ESP_LOGW(MODULE_TAG, "Module not running, ignoring stop request.");
    }
}

// Public functions (if any)
// void your_module_set_parameter(int value) {
//     // Implementation
// }
```

### 3. Register Module in Registry (`modules_include/module_registry.h`)

This is the ONLY core file you need to edit! Add your module in two places:

**STEP 1:** Add the include:
```c
#include "dimmer_control.h"
#include "your_module.h"  // <-- Add this line
```

**STEP 2:** Add to the MODULE_REGISTRY_LIST:
```c
#define MODULE_REGISTRY_LIST \
    &dimmer_module,          \
    &your_module,            \
    NULL
```

That's it! The module manager will automatically pick it up.

### 4. Update CMakeLists.txt (if needed)

If you created the files in the standard locations, CMake should pick them up automatically.
If not, add them to `modules_src/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "your_module.c"
         "dimmer_control.c"
         ...
    INCLUDE_DIRS "."
)
```

## That's It!

Your module will now be:
- Automatically detected when connected (based on identification voltage)
- Started when the connection is confirmed
- Stopped when disconnected or when another module is plugged in

## Example: See Dimmer Module

For a complete working example, see:
- `modules_include/dimmer_control.h`
- `modules_src/dimmer_control.c`

## Important Notes

1. **Module Identification Voltage**: Each module must have a unique identification voltage range defined by a resistor divider on the module PCB.

2. **Thread Safety**: The module manager runs in a FreeRTOS task. If your module creates additional tasks or uses interrupts, ensure proper synchronization.

3. **Error Handling**: Use `ESP_ERROR_CHECK()` for critical operations and log warnings/errors appropriately.

4. **GPIO Management**: Always reset GPIOs to high-impedance or safe state in your stop function.

5. **Resource Cleanup**: Ensure all resources (timers, interrupts, memory) are properly freed in the stop function.

## Module Identification Voltage Ranges

Document your module's identification voltage here to avoid conflicts:

| Module | Min (mV) | Max (mV) | Status |
|--------|----------|----------|--------|
| Dimmer | 500 | 600 | ✓ Implemented |
| Inverter | 1600 | 1700 | Planned |
| Buck Converter | 1900 | 2000 | Planned |
| Your Module | 1000 | 1100 | Your entry here |

## Questions?

Contact: mateo@cignetti.ar
