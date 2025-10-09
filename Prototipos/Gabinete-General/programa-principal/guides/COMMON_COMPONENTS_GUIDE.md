# Common Components Implementation Guide

## Overview

The common components layer provides shared utilities that all modules can use. This creates a clean separation between:
- **Common code** - Shared by everyone
- **Module code** - Specific to each module

## Structure Created

```
components/common/
├── module_manager/          ✓ Module detection and lifecycle
│   ├── CMakeLists.txt      ✓ Build configuration
│   ├── include/            ✓ Public headers
│   │   ├── module_manager.h
│   │   ├── module_connection.h
│   │   └── module_registry.h
│   └── src/                (ready for migration)
│       ├── module_manager.c (to be copied)
│       └── module_connection.c (to be copied)
│
├── ui_framework/           ✓ LVGL and UI utilities
│   ├── CMakeLists.txt      ✓ Build configuration
│   ├── include/            ✓ Public headers
│   │   ├── ui_config.h
│   │   └── encoder.h
│   └── src/                (ready for migration)
│       ├── ui_config.c (to be copied)
│       └── encoder.c (to be copied)
│
├── hardware/               ✓ Hardware abstraction
│   ├── CMakeLists.txt      ✓ Build configuration
│   ├── include/            ✓ Public headers
│   │   ├── gpio_definition.h
│   │   └── hardware_config.h
│   └── src/                (future implementations)
│
└── README.md               ✓ Documentation
```

## What Each Component Does

### 1. Module Manager

**Manages:**
- Module detection via ADC voltage
- Module lifecycle (init, start, stop)
- Module registry (list of available modules)
- Connection state monitoring

**Key Files:**
- `module_manager.h` - Main interface, module registration
- `module_connection.h` - Physical connection monitoring
- `module_registry.h` - Central registry of all modules

**Example:**
```c
#include "module_manager.h"

void app_main(void) {
    module_manager_init();  // Auto-detects and starts module
}
```

### 2. UI Framework

**Provides:**
- LVGL initialization and configuration
- LCD display setup
- Rotary encoder input device
- Thread-safe LVGL access

**Key Files:**
- `ui_config.h` - UI setup and LVGL configuration
- `encoder.h` - Rotary encoder driver

**Example:**
```c
#include "ui_config.h"

void app_main(void) {
    setup_user_interface();
    
    // Thread-safe LVGL calls
    ui_lock();
    lv_label_set_text(my_label, "Hello");
    ui_unlock();
}
```

### 3. Hardware

**Centralizes:**
- GPIO pin definitions
- Hardware configuration constants
- Hardware abstraction utilities

**Key Files:**
- `gpio_definition.h` - All GPIO assignments
- `hardware_config.h` - Hardware utilities

**Example:**
```c
#include "gpio_definition.h"

void my_function(void) {
    // Use centralized pin definitions
    gpio_set_level(PIN_LCD_BL, 1);
}
```

## Migration Process

### Quick Migration

```bash
# Run the migration script
chmod +x migrate_common_components.sh
./migrate_common_components.sh
```

### Manual Migration

1. **Copy Module Manager files:**
   ```bash
   cp src/module_manager.c components/common/module_manager/src/
   cp src/module_connection.c components/common/module_manager/src/
   ```

2. **Copy UI Framework files:**
   ```bash
   cp src/ui_config.c components/common/ui_framework/src/
   cp src/encoder.c components/common/ui_framework/src/
   ```

3. **Update CMakeLists.txt:**
   In main `CMakeLists.txt`, change:
   ```cmake
   set(EXTRA_COMPONENT_DIRS "src")
   ```
   To:
   ```cmake
   set(EXTRA_COMPONENT_DIRS 
       "components/common/module_manager"
       "components/common/ui_framework"
       "components/common/hardware"
       "components/dimmer_module"
       "components/buck_module"
   )
   ```

4. **Fix include paths in migrated files:**
   
   In the copied `.c` files, change:
   ```c
   // Old:
   #include "../include/module_manager.h"
   
   // New:
   #include "module_manager.h"
   ```

5. **Build and test:**
   ```bash
   idf.py fullclean
   idf.py build
   ```

## Integration with Modules

### How Modules Use Common Components

```c
// In dimmer_module or buck_module

#include "module_manager.h"     // For module registration
#include "ui_config.h"          // For LVGL access
#include "gpio_definition.h"    // For pin definitions

// Module definition for registry
const module_t dimmer_module_def = {
    .name = "Dimmer",
    .ident_mv_min = 1500,
    .ident_mv_max = 1600,
    .start_function = dimmer_module_start,
    .stop_function = dimmer_module_stop,
};

void dimmer_module_start(void) {
    // Use UI framework
    ui_lock();
    // ... create LVGL objects
    ui_unlock();
}
```

### Dependency Graph

```
┌─────────────┐
│    main     │
└──────┬──────┘
       │
       │ uses
       ▼
┌─────────────────┐      ┌──────────────┐
│ Module Manager  │─────→│  Modules     │
│                 │      │  (dimmer,    │
└────────┬────────┘      │   buck)      │
         │               └──────────────┘
         │ uses                 │
         ▼                      │ uses
┌─────────────────┐             │
│  UI Framework   │←────────────┘
└────────┬────────┘
         │ uses
         ▼
┌─────────────────┐
│    Hardware     │
└─────────────────┘
```

## Updated Main CMakeLists.txt

Your main `CMakeLists.txt` should now look like this:

```cmake
# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS 
    "components/common/module_manager"
    "components/common/ui_framework"
    "components/common/hardware"
    "components/dimmer_module"
    "components/buck_module"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(programa-principal)
```

## Benefits

### Before (Flat Structure)
```
src/
├── module_manager.c       ❌ Mixed with everything
├── ui_config.c            ❌ Not reusable
└── encoder.c              ❌ Hard to find

include/
├── module_manager.h       ❌ All in one place
└── ui_config.h            ❌ No organization
```

### After (Component Structure)
```
components/common/
├── module_manager/        ✅ Clear purpose
├── ui_framework/          ✅ Reusable
└── hardware/              ✅ Easy to find
```

**Advantages:**
- ✅ **Clear organization** - Know where everything is
- ✅ **Reusable** - Any module can use common components
- ✅ **Maintainable** - Changes in one place
- ✅ **Testable** - Test components independently
- ✅ **Scalable** - Easy to add new common utilities

## Testing the Migration

### Build Test
```bash
idf.py fullclean
idf.py build
```

### Expected Output
```
-- Components: common/module_manager common/ui_framework common/hardware ...
-- Build files have been written to: ...
```

### Common Issues

**Issue:** `fatal error: module_manager.h: No such file or directory`
- **Fix:** Check EXTRA_COMPONENT_DIRS in CMakeLists.txt

**Issue:** `undefined reference to module_manager_init`
- **Fix:** Ensure source files are copied to src/ directories

**Issue:** Multiple definition errors
- **Fix:** Remove old `src` component from EXTRA_COMPONENT_DIRS

## Cleanup After Success

Once build succeeds and tests pass:

```bash
# Backup old structure
mkdir -p backup
mv src backup/
mv include backup/

# Or remove (if you're confident)
# rm -rf src include
```

## Next Steps

1. ✅ Common components structure created
2. ⬜ Run `migrate_common_components.sh`
3. ⬜ Update main CMakeLists.txt
4. ⬜ Build and test
5. ⬜ Update module components to use common components
6. ⬜ Clean up old directories

## Documentation

- `components/common/README.md` - Detailed component documentation
- `components/common/module_manager/include/*.h` - API documentation in headers
- `components/common/ui_framework/include/*.h` - UI API documentation

---

**You now have a professional, scalable architecture! 🎉**
