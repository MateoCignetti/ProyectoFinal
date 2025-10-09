# Component-Based Module Architecture

## Overview

This document describes the new component-based isolation architecture for the programa-principal project.

## Architecture Principles

### 1. **Complete Isolation**
Each module is a self-contained ESP-IDF component with:
- Its own source files
- Its own UI implementation
- Its own dependencies
- Its own public interface

### 2. **No Symbol Conflicts**
- Each module has its own namespace
- UI symbols are internal to the module
- Only the public interface is exposed

### 3. **Clear Dependencies**
- Modules depend on common utilities
- Common utilities never depend on modules
- UI code is private to each module

## Directory Structure

```
programa-principal/
│
├── components/                        # All custom components
│   │
│   ├── common/                        # Shared utilities (future)
│   │   ├── module_manager/           # Module detection and lifecycle
│   │   ├── ui_framework/             # Common UI utilities
│   │   └── hardware/                 # Hardware abstractions
│   │
│   ├── dimmer_module/                # Dimmer module component
│   │   ├── CMakeLists.txt           # Component build config
│   │   │
│   │   ├── include/                  # Public headers
│   │   │   ├── dimmer_module.h      # Main public API
│   │   │   ├── dimmer_interface.h   # Interface functions
│   │   │   └── dimmer_control.h     # Control functions
│   │   │
│   │   ├── src/                      # Implementation
│   │   │   ├── dimmer_interface.c   # Interface implementation
│   │   │   └── dimmer_control.c     # Control logic
│   │   │
│   │   └── ui/                       # UI files (private)
│   │       ├── dimmer_ui.h          # UI header
│   │       ├── dimmer_ui.c          # Main UI file
│   │       ├── dimmer_ui_Screen*.c  # Screen implementations
│   │       └── ... (other UI files)
│   │
│   └── buck_module/                  # Buck converter module component
│       ├── CMakeLists.txt
│       ├── include/
│       │   ├── buck_module.h
│       │   ├── buck_interface.h
│       │   └── buck_control.h
│       ├── src/
│       │   ├── buck_interface.c
│       │   └── buck_control.c
│       └── ui/
│           ├── buck_ui.h
│           ├── buck_ui.c
│           └── ... (other UI files)
│
├── main/                              # Main application
│   ├── CMakeLists.txt
│   └── main.c
│
├── src/                               # Legacy common code (to be migrated)
│   ├── module_manager.c
│   ├── module_connection.c
│   ├── ui_config.c
│   └── encoder.c
│
└── CMakeLists.txt                     # Project configuration
```

## Component Structure

### Each Module Component Contains:

#### 1. **CMakeLists.txt**
```cmake
file(GLOB UI_SOURCES "ui/*.c")

idf_component_register(
    SRCS 
        "src/module_interface.c"
        "src/module_control.c"
        ${UI_SOURCES}
    
    INCLUDE_DIRS 
        "include"
        "ui"
    
    PRIV_REQUIRES 
        freertos
        lvgl__lvgl
        # ... other dependencies
)
```

#### 2. **include/** - Public API
- `module_name.h` - Main public interface
- Other public headers as needed

#### 3. **src/** - Implementation
- Business logic
- Hardware control
- Module lifecycle management

#### 4. **ui/** - Private UI
- All SquareLine Studio generated files
- UI is internal implementation detail
- Not exposed to other components

## Public Interface Pattern

Each module exposes a consistent public interface:

```c
// In dimmer_module.h or buck_module.h

bool module_init(void);           // Initialize the module
void module_start(void);          // Start module operation
void module_stop(void);           // Stop module operation
void module_deinit(void);         // Cleanup resources
const char* module_get_name(void); // Get module name
void module_get_ident_range(int *min, int *max); // ID voltage range
```

## Usage Example

### In main.c or module_manager:

```c
#include "dimmer_module.h"
#include "buck_module.h"

void module_manager_init(void) {
    // Detect which module is connected
    int voltage_mv = read_identification_voltage();
    
    int min_mv, max_mv;
    
    // Check dimmer
    dimmer_module_get_ident_range(&min_mv, &max_mv);
    if (voltage_mv >= min_mv && voltage_mv <= max_mv) {
        dimmer_module_init();
        dimmer_module_start();
        return;
    }
    
    // Check buck
    buck_module_get_ident_range(&min_mv, &max_mv);
    if (voltage_mv >= min_mv && voltage_mv <= max_mv) {
        buck_module_init();
        buck_module_start();
        return;
    }
}
```

## Benefits

### ✅ **Maintainability**
- Each module is independent
- Changes to one module don't affect others
- Easy to understand module boundaries

### ✅ **Scalability**
- Adding a new module is straightforward
- Copy a module template
- Implement the public interface
- Done!

### ✅ **No Conflicts**
- UI symbols are private
- No linker errors from duplicate symbols
- Clean namespace separation

### ✅ **Testability**
- Each module can be tested independently
- Mock interfaces easily
- Unit test individual components

### ✅ **Reusability**
- Modules can be used in other projects
- Just copy the component directory
- Self-contained with all dependencies

## Migration Path

See `MIGRATION_GUIDE.md` for step-by-step migration instructions.

Quick start:
```bash
# Run the migration script
./migrate_to_components.sh

# Update CMakeLists.txt
# Build and test
idf.py build
```

## Future Enhancements

1. **Common Library**
   - Move module_manager to components/common/module_manager
   - Create ui_framework for shared UI utilities
   - Hardware abstraction layer

2. **Module Registry**
   - Automatic module discovery
   - Plugin architecture
   - Dynamic loading

3. **Configuration**
   - Kconfig for each module
   - Runtime configuration
   - Module-specific settings

## Questions?

See the migration guide or architecture documentation for more details.
