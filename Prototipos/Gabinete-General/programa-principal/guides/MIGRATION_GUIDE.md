# Module Restructuring Migration Guide

## Overview
This guide helps you migrate from the old flat structure to the new component-based isolation architecture.

## New Architecture

### Directory Structure
```
components/
├── dimmer_module/          # Self-contained dimmer module
│   ├── CMakeLists.txt     # Build configuration
│   ├── include/           # Public headers
│   │   ├── dimmer_module.h      # Main public interface
│   │   ├── dimmer_interface.h   # (moved from modules_include)
│   │   └── dimmer_control.h     # (moved from modules_include)
│   ├── src/               # Implementation files
│   │   ├── dimmer_interface.c   # (moved from modules_src)
│   │   └── dimmer_control.c     # (moved from modules_src)
│   └── ui/                # UI files (internal to module)
│       ├── dimmer_ui.h          # (moved from modules_include/ui_dimmer)
│       ├── dimmer_ui.c          # (moved from modules_src/ui_dimmer)
│       └── ... (all other UI files)
│
└── buck_module/            # Self-contained buck module
    ├── CMakeLists.txt
    ├── include/
    │   ├── buck_module.h
    │   ├── buck_interface.h
    │   └── buck_control.h
    ├── src/
    │   ├── buck_interface.c
    │   └── buck_control.c
    └── ui/
        ├── buck_ui.h
        ├── buck_ui.c
        └── ... (all other UI files)
```

## Migration Steps

### Step 1: Copy Files to New Structure

#### For Dimmer Module:
```bash
# Copy interface and control headers
cp modules_include/dimmer_interface.h components/dimmer_module/include/
cp modules_include/dimmer_control.h components/dimmer_module/include/

# Copy interface and control sources
cp modules_src/dimmer_interface.c components/dimmer_module/src/
cp modules_src/dimmer_control.c components/dimmer_module/src/

# Copy all UI files
cp modules_include/ui_dimmer/* components/dimmer_module/ui/
cp modules_src/ui_dimmer/* components/dimmer_module/ui/
```

#### For Buck Module:
```bash
# Copy interface and control headers
cp modules_include/buck_interface.h components/buck_module/include/
cp modules_include/buck_control.h components/buck_module/include/

# Copy interface and control sources
cp modules_src/buck_interface.c components/buck_module/src/
cp modules_src/buck_control.c components/buck_module/src/

# Copy all UI files
cp modules_include/ui_buck/* components/buck_module/ui/
cp modules_src/ui_buck/* components/buck_module/ui/
```

### Step 2: Update Include Paths

#### In dimmer_interface.c and dimmer_control.c:
Change:
```c
#include "dimmer_ui.h"
```
To:
```c
#include "dimmer_ui.h"  // Now found in ui/ subdirectory
```

#### In dimmer UI files:
No changes needed - they reference each other locally

### Step 3: Update Main CMakeLists.txt

Change:
```cmake
set(EXTRA_COMPONENT_DIRS "modules_src" "modules_src/ui_buck" "modules_src/ui_dimmer" "src")
```

To:
```cmake
set(EXTRA_COMPONENT_DIRS "components/dimmer_module" "components/buck_module" "src")
```

### Step 4: Update Code That Uses Modules

#### Old way:
```c
#include "buck_interface.h"
#include "dimmer_interface.h"

start_buck_interface();
```

#### New way:
```c
#include "buck_module.h"
#include "dimmer_module.h"

buck_module_start();
```

### Step 5: Fix Symbol Conflicts (if needed)

If you still have symbol conflicts between buck and dimmer UIs, run:
```bash
python3 rename_dimmer_symbols.py
```

This will rename all `ui_*` symbols in dimmer to `dimmer_ui_*`

## Benefits of New Architecture

✅ **Complete Isolation**: Each module is self-contained
✅ **No Symbol Conflicts**: Modules don't share symbol names
✅ **Easy to Add Modules**: Just create a new component folder
✅ **Better Encapsulation**: Internal UI details are hidden
✅ **Cleaner Dependencies**: Clear component boundaries
✅ **Easier Testing**: Each module can be tested independently

## Rollback Plan

If you need to rollback, the old files are still in:
- `modules_src/`
- `modules_include/`

Just update CMakeLists.txt to use the old EXTRA_COMPONENT_DIRS.

## Next Steps

1. Copy files as shown above
2. Update main CMakeLists.txt
3. Update module_manager to use new interfaces
4. Test build
5. Delete old directories once confirmed working
