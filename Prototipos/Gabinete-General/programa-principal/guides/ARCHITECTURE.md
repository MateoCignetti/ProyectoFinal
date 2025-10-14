/**
 * @file ARCHITECTURE.md
 * @brief System architecture overview for developers
 * @author Mateo Antonio Cignetti
 * @date 2025-10-02
 */

# System Architecture - General Cabinet

## Overview

The General Cabinet is a modular power electronics control system with hot-swap capability. Modules are automatically detected via voltage divider identification and managed by a central state machine.

## Key Components

### 1. Module Manager (`module_manager.c`)
- Central controller for module lifecycle
- Handles module identification via ADC reading
- Manages state transitions (INIT → READY → RUNNING → STOPPING)
- Coordinates with module connection system

### 2. Module Connection (`module_connection.c`)
- Monitors hot-plug signals (HP_POWER and HP_SIGNAL pins)
- Implements debouncing via timer (3 second delay)
- Notifies module manager of connection changes
- ISR-based detection with FreeRTOS task processing

### 3. Module Registry System
- **`module_registry.h`**: Single include point for all modules
- **`module_t`**: Single struct containing module info, voltage range, and function pointers
- Each module self-declares its identification parameters
- Eliminates need to modify core manager files

### 4. Individual Modules
Each module (e.g., `dimmer_control.c`) implements:
- `start_function()`: Initialize and run the module
- `stop_function()`: Safely shut down the module
- Identification voltage range constants
- Self-contained hardware management

## Module Structure

Each module is defined by a single `module_t` struct:

```c
typedef struct {
    const char* name;              // Human-readable module name
    uint16_t ident_mv_min;         // Minimum identification voltage (mV)
    uint16_t ident_mv_max;         // Maximum identification voltage (mV)
    void (*start_function)(void);  // Called when module is detected
    void (*stop_function)(void);   // Called when module is disconnected
} module_t;
```

**Example declaration:**
```c
const module_t dimmer_module = {
    .name = "Dimmer Module",
    .ident_mv_min = 500,
    .ident_mv_max = 600,
    .start_function = start_dimmer_module,
    .stop_function = stop_dimmer_module,
};
```

## Module Registration Pattern

Developers only:
1. Create module `.c` and `.h` files with standard structure
2. Edit `module_registry.h` (2 lines: include + list entry)

**That's it!** Never touch `module_manager.c`.

**Benefits:**
- ✅ Single-file registration (module_registry.h)
- ✅ Single-struct design (no redundant pointers)
- ✅ Core manager code is untouchable
- ✅ Clear separation of concerns
- ✅ Easy to maintain and extend
- ✅ Template-driven development
- ✅ Reduced chance of breaking existing modules
- ✅ Macro-based list for compile-time registration

## Data Flow

```
Module Connected
    ↓
[module_connection.c] ISR → Debounce Timer → Task Notification
    ↓
[module_manager.c] Callback triggered
    ↓
Setup ADC → Read identification voltage
    ↓
Search module_list[] for matching voltage range
    ↓
Found? → Compare voltage to module->ident_mv_min/max
    ↓
Match found → Call module->start_function()
    ↓
Module running (e.g., dimmer starts timers & ISRs)
    ↓
Module Disconnected → module->stop_function()
    ↓
Cleanup complete → Ready for next module
```

## Thread Safety

- **Module Manager State**: Protected by `xModuleManagerMutex`
- **Connection State**: Protected by `xConnectionStateMutex`
- **ISR Handlers**: Use `IRAM_ATTR` and proper FreeRTOS ISR functions
- **Timer Callbacks**: Return `BaseType_t` for context switching

## Module Identification

Each module has a unique resistor divider on its PCB that produces a specific voltage on the `PIN_MODULE_IDENT` pin:

| Module | Voltage Range | Status |
|--------|---------------|--------|
| Dimmer | 500-600 mV | Implemented |
| Inverter | 1600-1700 mV | Planned |
| Buck Converter | 1900-2000 mV | Planned |

100 mV windows provide sufficient noise margin while allowing many modules.

## GPIO ISR Service

- **Centralized installation**: `module_connection.c` installs the service once
- **Multiple handlers**: Different modules can add handlers without reinstalling
- **Proper cleanup**: Service uninstalled when connection system shuts down

## Best Practices

1. **Always NULL-check handles** before using timers/peripherals
2. **Reset GPIOs to safe state** in stop functions (high-impedance)
3. **Use ESP_ERROR_CHECK** for critical operations
4. **Log state transitions** for debugging
5. **Handle stop-during-operation** gracefully (ISR may be active)
6. **Free all resources** in stop function (no memory leaks)


## File Organization

```
programa-principal/
├── main/
│   ├── main.c                      # Entry point
│   └── CMakeLists.txt              # Main component build config
├── include/
│   └── gpio_definition.h           # Shared pin definitions
├── src/
│   ├── module_manager.c            # Manager implementation (core framework)
│   ├── module_connection.c         # Connection detection implementation
│   └── CMakeLists.txt              # Core framework build config
├── modules_include/
│   ├── module_manager.h            # Manager interface
│   ├── module_connection.h         # Connection interface  
│   ├── module_registry.h           # ✨ Module registry (add modules here!)
│   └── dimmer_control.h            # Dimmer module interface
├── modules_src/
│   ├── dimmer_control.c            # Dimmer module implementation
│   └── CMakeLists.txt              # Modules component build config
├── ARCHITECTURE.md                 # This file - system documentation
├── MODULE_TEMPLATE.md              # ✨ Developer guide for adding modules
├── DEVELOPER_GUIDE.txt             # Visual workflow guide
├── README.md                       # Project readme
└── CMakeLists.txt                  # Top-level project configuration
```

### Component Structure

The project is organized into **ESP-IDF components**:

- **`main`**: Application entry point, initializes the system
- **`src`**: Core framework (module_manager, module_connection)
  - Handles module lifecycle and hot-plug detection
  - Dependencies: `modules_src`, FreeRTOS, GPIO, GPTimer, ADC
- **`modules_src`**: Individual module implementations
  - Each module is self-contained (dimmer, future inverter, etc.)
  - Add new modules here without touching core framework
  - Dependencies: FreeRTOS, GPIO, GPTimer, etc. (per module)

### Adding a New Module

1. **Create files in `modules_src/` and `modules_include/`**
2. **Edit `modules_include/module_registry.h`** (add 2 lines)
3. **Done!** Build and flash

See [MODULE_TEMPLATE.md](MODULE_TEMPLATE.md) for details.

## Questions or Issues?

Contact: mateo@cignetti.ar
Repository: github.com/MateoCignetti/ProyectoFinal
