# Architecture Diagrams

## Component Structure

```
┌─────────────────────────────────────────────────────────────────┐
│                        Main Application                          │
│                          (main.c)                                │
└───────────────────────┬─────────────────────────────────────────┘
                        │
                        │ uses
                        ▼
┌─────────────────────────────────────────────────────────────────┐
│                     Module Manager                               │
│                  (src/module_manager.c)                          │
│                                                                   │
│  • Detects connected module via ID voltage                       │
│  • Initializes appropriate module                                │
│  • Manages module lifecycle                                      │
└──────────────┬────────────────────────┬─────────────────────────┘
               │                        │
               │ uses                   │ uses
               ▼                        ▼
┌──────────────────────────┐  ┌──────────────────────────┐
│    Dimmer Module         │  │     Buck Module          │
│  (components/            │  │  (components/            │
│   dimmer_module/)        │  │   buck_module/)          │
├──────────────────────────┤  ├──────────────────────────┤
│                          │  │                          │
│ PUBLIC API (include/)    │  │ PUBLIC API (include/)    │
│ ┌──────────────────────┐ │  │ ┌──────────────────────┐ │
│ │ dimmer_module.h      │ │  │ │ buck_module.h        │ │
│ │  • init()            │ │  │ │  • init()            │ │
│ │  • start()           │ │  │ │  • start()           │ │
│ │  • stop()            │ │  │ │  • stop()            │ │
│ │  • deinit()          │ │  │ │  • deinit()          │ │
│ └──────────────────────┘ │  │ └──────────────────────┘ │
│                          │  │                          │
│ IMPLEMENTATION (src/)    │  │ IMPLEMENTATION (src/)    │
│ ┌──────────────────────┐ │  │ ┌──────────────────────┐ │
│ │ dimmer_interface.c   │ │  │ │ buck_interface.c     │ │
│ │ dimmer_control.c     │ │  │ │ buck_control.c       │ │
│ └──────────────────────┘ │  │ └──────────────────────┘ │
│                          │  │                          │
│ UI (PRIVATE) (ui/)       │  │ UI (PRIVATE) (ui/)       │
│ ┌──────────────────────┐ │  │ ┌──────────────────────┐ │
│ │ dimmer_ui.c          │ │  │ │ buck_ui.c            │ │
│ │ dimmer_ui_Screen*.c  │ │  │ │ buck_ui_Screen*.c    │ │
│ │ (SquareLine files)   │ │  │ │ (SquareLine files)   │ │
│ └──────────────────────┘ │  │ └──────────────────────┘ │
│           │              │  │           │              │
│           └──uses LVGL───┤  │           └──uses LVGL───┤
└──────────────────────────┘  └──────────────────────────┘
```

## Module Isolation

```
┌─────────────────────────────────────────────────────────────┐
│                  Component Isolation                         │
└─────────────────────────────────────────────────────────────┘

Each module is completely isolated:

┌──────────────────────┐      ┌──────────────────────┐
│   Dimmer Module      │      │    Buck Module       │
├──────────────────────┤      ├──────────────────────┤
│                      │      │                      │
│ Symbols:             │  ✓   │ Symbols:             │
│  dimmer_ui_Screen1   │      │  ui_Screen1          │
│  dimmer_ui_Button1   │  NO  │  ui_Button1          │
│  dimmer_ui_Label1    │      │  ui_Label1           │
│                      │ CONFLICT                    │
│ Functions:           │      │ Functions:           │
│  dimmer_module_init  │      │  buck_module_init    │
│  dimmer_module_start │      │  buck_module_start   │
│                      │      │                      │
└──────────────────────┘      └──────────────────────┘

No shared symbols = No linker conflicts!
```

## Data Flow

```
User Interaction
      │
      ▼
┌─────────────┐
│  Hardware   │
│  (Encoder)  │
└──────┬──────┘
       │ events
       ▼
┌──────────────────────────┐
│   UI Config              │
│   (LVGL Input Device)    │
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│   Active Module UI       │
│   (dimmer_ui or buck_ui) │
└──────┬───────────────────┘
       │ screen changes
       │ button presses
       ▼
┌──────────────────────────┐
│   Module Interface       │
│   (dimmer/buck_interface)│
└──────┬───────────────────┘
       │ control commands
       ▼
┌──────────────────────────┐
│   Module Control         │
│   (dimmer/buck_control)  │
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│   Hardware (GPIO/PWM)    │
└──────────────────────────┘
```

## Build System Flow

```
Project CMakeLists.txt
        │
        │ includes
        ▼
┌────────────────────────────────┐
│  EXTRA_COMPONENT_DIRS          │
│  ├─ components/dimmer_module   │
│  ├─ components/buck_module     │
│  └─ src                        │
└────────────────────────────────┘
        │
        ├─────────────┬─────────────┐
        ▼             ▼             ▼
┌─────────────┐ ┌──────────┐ ┌──────────┐
│   Dimmer    │ │   Buck   │ │   Src    │
│ CMakeLists  │ │CMakeLists│ │CMakeLists│
└─────────────┘ └──────────┘ └──────────┘
        │             │             │
        │ registers   │ registers   │ registers
        ▼             ▼             ▼
┌─────────────────────────────────────┐
│         ESP-IDF Build System        │
│                                     │
│  ├─ libdimmer_module.a              │
│  ├─ libbuck_module.a                │
│  └─ libsrc.a                        │
└─────────────────────────────────────┘
        │
        │ links
        ▼
┌─────────────────────────────────────┐
│      programa-principal.elf         │
└─────────────────────────────────────┘
```

## Dependency Graph

```
                 ┌──────────────┐
                 │     main     │
                 └──────┬───────┘
                        │
                        ▼
                 ┌──────────────┐
                 │module_manager│
                 └──┬────────┬──┘
                    │        │
        ┌───────────┘        └───────────┐
        ▼                                ▼
┌───────────────┐                ┌───────────────┐
│dimmer_module  │                │  buck_module  │
├───────────────┤                ├───────────────┤
│ • interface   │                │ • interface   │
│ • control     │                │ • control     │
│ • ui          │                │ • ui          │
└───┬───────────┘                └───┬───────────┘
    │                                │
    │ depends on                     │ depends on
    ▼                                ▼
┌────────────────────────────────────────────────┐
│           Common Dependencies                   │
│  • LVGL                                        │
│  • FreeRTOS                                    │
│  • ESP-IDF drivers                             │
└────────────────────────────────────────────────┘

Note: Dimmer and Buck modules are independent!
They share NO code with each other.
```

## Adding a New Module

```
Step 1: Create Structure
┌────────────────────────────┐
│ components/new_module/     │
│  ├─ CMakeLists.txt         │
│  ├─ include/               │
│  │   └─ new_module.h       │
│  ├─ src/                   │
│  │   ├─ new_interface.c    │
│  │   └─ new_control.c      │
│  └─ ui/                    │
│      └─ (UI files)         │
└────────────────────────────┘

Step 2: Implement Interface
┌────────────────────────────┐
│ new_module.h               │
├────────────────────────────┤
│ bool new_module_init()     │
│ void new_module_start()    │
│ void new_module_stop()     │
│ void new_module_deinit()   │
└────────────────────────────┘

Step 3: Register in CMake
┌────────────────────────────┐
│ set(EXTRA_COMPONENT_DIRS   │
│   "components/new_module"  │
│   ...                      │
│ )                          │
└────────────────────────────┘

Step 4: Add to Module Manager
┌────────────────────────────┐
│ #include "new_module.h"    │
│                            │
│ if (voltage matches) {     │
│   new_module_init();       │
│   new_module_start();      │
│ }                          │
└────────────────────────────┘

Done! ✓
```
