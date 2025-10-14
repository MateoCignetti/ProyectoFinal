# Common Components

This directory contains shared utilities used by all modules in the system.

## Component Overview

### 1. Module Manager (`module_manager/`)

**Purpose:** Module detection, lifecycle management, and coordination

**Responsibilities:**
- Detect which module is connected via identification voltage
- Initialize and manage module lifecycle
- Coordinate module switching
- Maintain module registry

**Files:**
- `include/module_manager.h` - Main manager interface
- `include/module_connection.h` - Physical connection monitoring
- `include/module_registry.h` - Central module registry
- `src/module_manager.c` - Implementation (to be migrated)
- `src/module_connection.c` - Implementation (to be migrated)

**Usage:**
```c
#include "module_manager.h"

void app_main(void) {
    module_manager_init();  // Starts detection and management
}
```

### 2. UI Framework (`ui_framework/`)

**Purpose:** LVGL configuration and common UI utilities

**Responsibilities:**
- Initialize and configure LVGL
- Manage LCD display
- Handle input devices (rotary encoder)
- Provide thread-safe LVGL API access

**Files:**
- `include/ui_config.h` - UI initialization and LVGL setup
- `include/encoder.h` - Rotary encoder driver
- `src/ui_config.c` - Implementation (to be migrated)
- `src/encoder.c` - Implementation (to be migrated)

**Usage:**
```c
#include "ui_config.h"

void app_main(void) {
    setup_user_interface();  // Initialize LVGL and display
    
    // Thread-safe LVGL access
    ui_lock();
    lv_obj_set_x(my_obj, 10);
    ui_unlock();
}
```

### 3. Hardware (`hardware/`)

**Purpose:** Hardware abstraction and GPIO definitions

**Responsibilities:**
- Centralize GPIO pin definitions
- Provide hardware abstraction layer
- Common hardware utilities (ADC, GPIO, etc.)

**Files:**
- `include/gpio_definition.h` - All GPIO pin assignments
- `include/hardware_config.h` - Hardware utilities
- `src/` - Implementation (future)

**Usage:**
```c
#include "gpio_definition.h"
#include "hardware_config.h"

void setup_my_module(void) {
    // Use centralized pin definitions
    gpio_set_level(PIN_LCD_BL, 1);
    
    // Read module identification
    int voltage_mv;
    read_module_ident_voltage(&voltage_mv);
}
```

## Component Dependencies

```
┌─────────────────┐
│  Application    │
└────────┬────────┘
         │
         ├──────────────┬──────────────┐
         │              │              │
         ▼              ▼              ▼
┌────────────┐  ┌─────────────┐  ┌──────────┐
│  Module    │  │ UI Framework│  │ Hardware │
│  Manager   │  │             │  │          │
└─────┬──────┘  └──────┬──────┘  └────┬─────┘
      │                │              │
      │                └──────────────┤
      └───────────────────────────────┘
                     │
                     ▼
              ┌──────────────┐
              │  ESP-IDF     │
              │  Drivers     │
              └──────────────┘
```

## Migration Status

### Current State (Before Migration)
```
src/
├── module_manager.c     → To migrate to module_manager/src/
├── module_connection.c  → To migrate to module_manager/src/
├── ui_config.c          → To migrate to ui_framework/src/
└── encoder.c            → To migrate to ui_framework/src/

include/
├── module_manager.h     → To migrate to module_manager/include/
├── module_connection.h  → To migrate to module_manager/include/
├── module_registry.h    → To migrate to module_manager/include/
├── ui_config.h          → To migrate to ui_framework/include/
├── encoder.h            → To migrate to ui_framework/include/
└── gpio_definition.h    → To migrate to hardware/include/
```

### Target State (After Migration)
```
components/common/
├── module_manager/
│   ├── CMakeLists.txt ✓
│   ├── include/       ✓
│   └── src/           (ready for migration)
├── ui_framework/
│   ├── CMakeLists.txt ✓
│   ├── include/       ✓
│   └── src/           (ready for migration)
└── hardware/
    ├── CMakeLists.txt ✓
    ├── include/       ✓
    └── src/           (future)
```

## How to Use in Your Project

### 1. Add to CMakeLists.txt

Update your main `CMakeLists.txt`:
```cmake
set(EXTRA_COMPONENT_DIRS 
    "components/common/module_manager"
    "components/common/ui_framework"
    "components/common/hardware"
    "components/dimmer_module"
    "components/buck_module"
)
```

### 2. Include in Your Code

```c
// In your module or main file
#include "module_manager.h"     // For module management
#include "ui_config.h"          // For UI setup
#include "gpio_definition.h"    // For pin definitions
#include "encoder.h"            // For encoder events
```

### 3. Migration Steps

1. **Copy existing source files:**
   ```bash
   # Module Manager
   cp src/module_manager.c components/common/module_manager/src/
   cp src/module_connection.c components/common/module_manager/src/
   
   # UI Framework
   cp src/ui_config.c components/common/ui_framework/src/
   cp src/encoder.c components/common/ui_framework/src/
   ```

2. **Update include paths in source files:**
   - Change `#include "../include/xyz.h"` to `#include "xyz.h"`
   - Headers are now found via INCLUDE_DIRS in CMakeLists.txt

3. **Update main CMakeLists.txt** (see above)

4. **Build and test:**
   ```bash
   idf.py build
   ```

## Benefits of This Structure

✅ **Centralized Common Code** - Shared utilities in one place  
✅ **Clear Dependencies** - Easy to understand what depends on what  
✅ **Reusable** - Common components can be used by any module  
✅ **Maintainable** - Changes to common code are centralized  
✅ **Scalable** - Easy to add new common utilities  

## Adding a New Common Component

1. Create directory: `components/common/new_component/`
2. Create subdirectories: `include/`, `src/`
3. Create `CMakeLists.txt`
4. Implement your component
5. Add to main `CMakeLists.txt`
6. Update this README

## Component Guidelines

- **Module Manager**: Only module management logic
- **UI Framework**: Only UI/display/input logic
- **Hardware**: Only hardware abstraction
- **Keep focused**: Each component has one clear purpose
- **No cross-dependencies**: Common components shouldn't depend on modules

## Next Steps

1. Migrate source files from `src/` to appropriate component `src/` directories
2. Test build with new structure
3. Update module components to use common components
4. Remove old `src/` and `include/` directories once migration is complete
