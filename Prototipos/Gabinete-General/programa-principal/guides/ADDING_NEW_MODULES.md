# Adding New Modules - Developer Guide

## Overview

This project supports multiple hot-swappable modules that can coexist in the same binary and be dynamically switched at runtime. Each module has its own UI, control logic, and can be developed independently.

## Quick Start - Adding a New Module

### 1. Create Module Structure

```
components/
└── your_module/
    ├── CMakeLists.txt
    ├── include/
    │   ├── your_module.h           # Module interface (start/stop functions)
    │   └── your_control.h          # Control logic header
    ├── src/
    │   ├── your_interface.c        # Module lifecycle & UI management
    │   └── your_control.c          # Hardware control logic
    └── ui/                         # SquareLine Studio generated UI
        ├── your_ui.c
        ├── your_ui.h
        └── your_ui_Screen*.c/h
```

### 2. Create CMakeLists.txt

```cmake
# Your Module Component
# Self-contained module with control logic and UI

# Collect source files
set(MODULE_SRCS "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/your_interface.c")
    list(APPEND MODULE_SRCS "src/your_interface.c")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/your_control.c")
    list(APPEND MODULE_SRCS "src/your_control.c")
endif()

# Collect all UI source files
file(GLOB UI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.c")

# Combine all sources
set(ALL_SRCS ${MODULE_SRCS} ${UI_SOURCES})

idf_component_register(
    SRCS 
        ${ALL_SRCS}
    INCLUDE_DIRS 
        "include"
        "ui"
    REQUIRES
        lvgl__lvgl
    PRIV_REQUIRES 
        freertos
        esp_driver_gpio
        # Add your specific drivers here
        esp_lcd
        esp_timer
        hardware
        module_manager
        ui_framework
)
```

### 3. Define Module Interface

In `include/your_control.h`:

```c
#ifndef YOUR_CONTROL_H
#define YOUR_CONTROL_H

#include "module_manager.h"

// Module identification struct
extern const module_t your_module;

// Control functions
void your_control_init(void);
void your_control_set_output(float value);
// ... other control functions

#endif
```

In `src/your_interface.c`:

```c
#include "your_module.h"
#include "module_manager.h"
#include "your_ui.h"
#include "ui_config.h"

// Module definition
const module_t your_module = {
    .name = "Your Module Name",
    .ident_mv_min = 2000,  // Set your ADC voltage range
    .ident_mv_max = 2100,
    .start_function = start_your_interface,
    .stop_function = stop_your_interface,
};

void start_your_interface() {
    // Initialize your module
    _lock_acquire(&lvgl_api_lock);
    ui_init();  // Initialize UI
    // Setup your UI groups and event handlers
    _lock_release(&lvgl_api_lock);
}

void stop_your_interface() {
    // Cleanup your module
    _lock_acquire(&lvgl_api_lock);
    
    // Create blank screen before destroying UI
    lv_obj_t *blank_screen = lv_obj_create(NULL);
    lv_screen_load(blank_screen);
    
    // Destroy your UI
    ui_destroy();
    
    _lock_release(&lvgl_api_lock);
}
```

### 4. Register Module

Edit `components/common/module_manager/include/module_registry.h`:

```c
// STEP 1: Add include
#include "your_control.h"

// STEP 2: Add to registry list
#define MODULE_REGISTRY_LIST \
    &dimmer_module,          \
    &buck_module,            \
    &your_module,            \  // <-- Add here
    NULL
```

### 5. Update Component Dependencies

Edit the main `CMakeLists.txt` to include your module:

```cmake
set(EXTRA_COMPONENT_DIRS 
    "components/common/module_manager"
    "components/common/ui_framework"
    "components/common/hardware"
    "components/dimmer_module"
    "components/buck_module"
    "components/your_module"  # <-- Add here
)
```

Edit `components/common/module_manager/CMakeLists.txt`:

```cmake
PRIV_REQUIRES 
    # ... existing ...
    your_module  # <-- Add here
```

## Important Notes

### UI Symbol Conflicts

**Important:** The project uses `--allow-multiple-definition` linker flag to handle UI symbol conflicts between modules. This means:

- Multiple modules can have UI symbols with the same names (like `ui_Screen1`, `ui_Button1`, etc.)
- The linker will accept the first definition it encounters
- Each module's UI is self-contained and only accessed within that module
- **This is safe** because only one module's UI is active at any time

### SquareLine Studio Integration

When regenerating UI from SquareLine Studio:

1. Generate UI files as normal
2. Place them in your module's `ui/` directory
3. No symbol prefixing needed - conflicts are handled by the linker
4. Keep your UI code isolated within the module

### Module ADC Identification

Each module must have a unique ADC voltage range for identification:

- Buck: 600-700 mV
- Dimmer: 1400-1500 mV  
- Inverter: 1600-1700 mV (example)
- **Your module**: Choose a unique range (e.g., 2000-2100 mV)

Update `gpio_definition.h` if you need to change the ADC pin.

## Testing Your Module

1. Build the project: `idf.py build`
2. Flash: `idf.py flash`
3. Connect your module hardware (with correct ADC identification resistor)
4. Monitor: `idf.py monitor`

The module_manager will automatically detect and load your module!

## Troubleshooting

### Build Errors

- **"undefined reference to your_module"**: Did you add it to `module_registry.h`?
- **"No such file or directory"**: Check CMakeLists.txt paths and EXTRA_COMPONENT_DIRS
- **UI symbol conflicts** (should not happen): The `--allow-multiple-definition` flag should handle this

### Runtime Issues

- **Module not detected**: Check ADC voltage range and hardware connections
- **UI not loading**: Verify `ui_init()` and `ui_destroy()` are properly called with LVGL lock
- **Crashes on module switch**: Ensure proper cleanup in `stop_function`, especially blank screen creation

## Best Practices

1. **Always use LVGL lock** (`_lock_acquire`/`_lock_release`) when accessing UI
2. **Create blank screen before destroying UI** in stop_function
3. **Clean up all resources** (tasks, timers, hardware) in stop_function
4. **Use unique symbol names** for your public APIs (prefix with module name)
5. **Keep UI symbols as-is** from SquareLine Studio - no need to rename
6. **Test module switching** thoroughly (start -> stop -> start again)

## Example Modules

Reference implementations:
- `components/buck_module/` - PWM-based buck converter with complex UI
- `components/dimmer_module/` - Simple dimmer with multi-screen UI

## Architecture

```
┌─────────────────────────────────────────┐
│         Module Manager                   │
│  - Detects modules via ADC               │
│  - Calls start/stop functions            │
│  - Manages module lifecycle              │
└─────────────────────────────────────────┘
                  │
        ┌─────────┴─────────┬──────────┐
        │                   │          │
   ┌────▼────┐         ┌────▼────┐   ┌▼──────┐
   │ Buck    │         │ Dimmer  │   │ Your  │
   │ Module  │         │ Module  │   │Module │
   └─────────┘         └─────────┘   └───────┘
   - UI (isolated)     - UI (isolated)
   - Control Logic     - Control Logic
   - Hardware Drivers  - Hardware Drivers
```

Each module is completely self-contained and hot-swappable!
