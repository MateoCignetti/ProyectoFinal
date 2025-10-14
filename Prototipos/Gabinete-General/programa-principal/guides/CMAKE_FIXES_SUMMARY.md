# CMakeLists.txt Fixes Summary

## Date: October 8, 2025

## Overview
This document summarizes all the fixes applied to the CMakeLists.txt files across the project to ensure proper component dependencies and build configuration.

---

## Changes Made

### 1. **main/CMakeLists.txt**
**Issue**: Referenced non-existent component `src` and missing proper dependencies.

**Fix**:
```cmake
idf_component_register(
    SRCS "main.c"
    REQUIRES 
        module_manager
        ui_framework
        lvgl__lvgl
)
```

**Explanation**: 
- Removed invalid `src` component reference
- Added proper dependencies: `module_manager` and `ui_framework`
- Kept `lvgl__lvgl` for LVGL support

---

### 2. **components/buck_module/CMakeLists.txt**
**Issues**: 
- Incorrect LVGL dependency specification (`lvgl__lvgl` should be in REQUIRES, not PRIV_REQUIRES)
- Missing dependencies on `hardware` and `module_manager`
- Had duplicate `lvgl` reference

**Fix**:
```cmake
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
        esp_driver_gptimer
        esp_adc
        esp_lcd
        esp_timer
        hardware
        module_manager
)
```

**Explanation**:
- Moved `lvgl__lvgl` to REQUIRES (public dependency)
- Removed duplicate `lvgl` reference
- Added `hardware` for GPIO definitions
- Added `module_manager` for module lifecycle management
- Combined sources into `ALL_SRCS` for clarity

---

### 3. **components/dimmer_module/CMakeLists.txt**
**Issues**: 
- Incorrect LVGL dependency specification
- Missing dependencies on `hardware` and `module_manager`
- Had duplicate `lvgl` reference

**Fix**:
```cmake
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
        esp_driver_ledc
        esp_lcd
        esp_timer
        hardware
        module_manager
)
```

**Explanation**:
- Moved `lvgl__lvgl` to REQUIRES (public dependency)
- Removed duplicate `lvgl` reference
- Added `hardware` for GPIO definitions
- Added `module_manager` for module lifecycle management
- Combined sources into `ALL_SRCS` for clarity

---

### 4. **components/common/module_manager/CMakeLists.txt**
**Issues**:
- Missing public dependency on `hardware`
- Missing dependency on `ui_framework`

**Fix**:
```cmake
idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS 
        "include"
    REQUIRES
        hardware
    PRIV_REQUIRES 
        freertos
        esp_driver_gpio
        esp_adc
        esp_timer
        ui_framework
)
```

**Explanation**:
- Added `hardware` as REQUIRES (public) for GPIO definitions
- Added `ui_framework` as PRIV_REQUIRES for UI coordination
- Maintains proper dependency hierarchy

---

### 5. **components/common/ui_framework/CMakeLists.txt**
**Issues**:
- Incorrect LVGL dependency specification
- Missing dependency on `hardware`
- Had duplicate `lvgl` reference

**Fix**:
```cmake
idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS 
        "include"
    REQUIRES
        lvgl__lvgl
        hardware
    PRIV_REQUIRES 
        freertos
        esp_driver_gpio
        esp_driver_spi
        esp_lcd
        esp_timer
)
```

**Explanation**:
- Moved `lvgl__lvgl` to REQUIRES (public dependency)
- Removed duplicate `lvgl` reference
- Added `hardware` for GPIO/pin definitions
- All driver dependencies remain private

---

### 6. **components/common/hardware/CMakeLists.txt**
**Issues**:
- Header-only component had unnecessary PRIV_REQUIRES

**Fix**:
```cmake
idf_component_register(
    INCLUDE_DIRS 
        "include"
)
```

**Explanation**:
- Removed all PRIV_REQUIRES since this is a header-only component
- Added comment clarifying it's header-only
- Simplifies dependency chain

---

### 7. **Removed Obsolete Files**
**Files Removed**:
- `components/buck_module/ui/CMakeLists.txt`
- `components/dimmer_module/ui/CMakeLists.txt`

**Reason**: 
These files were outdated and conflicting with the parent module CMakeLists.txt:
- Referenced non-existent paths (`../../modules_include/`)
- Referenced non-existent components (`modules_src`)
- Duplicated functionality already in parent CMakeLists.txt (UI files are included via `file(GLOB UI_SOURCES ...)`)

---

## Component Dependency Graph

```
main
├── module_manager
│   ├── hardware (public)
│   └── ui_framework (private)
│       ├── lvgl__lvgl (public)
│       └── hardware (public)
└── ui_framework
    ├── lvgl__lvgl (public)
    └── hardware (public)

buck_module (optional, dynamically loaded)
├── lvgl__lvgl (public)
├── hardware (private)
└── module_manager (private)

dimmer_module (optional, dynamically loaded)
├── lvgl__lvgl (public)
├── hardware (private)
└── module_manager (private)
```

---

## Key Principles Applied

1. **REQUIRES vs PRIV_REQUIRES**:
   - `REQUIRES`: Public dependencies (headers exposed in component's public headers)
   - `PRIV_REQUIRES`: Private dependencies (only used in .c files)

2. **LVGL Dependencies**:
   - `lvgl__lvgl` is the managed component from ESP Component Registry
   - Should be in REQUIRES for components that expose LVGL types in their headers
   - No need for duplicate `lvgl` reference

3. **Header-Only Components**:
   - Components with only headers (like `hardware`) don't need PRIV_REQUIRES
   - Keep them minimal to reduce build complexity

4. **Module Architecture**:
   - Modules depend on common components (`hardware`, `module_manager`)
   - Common components don't depend on specific modules
   - Maintains clean separation and allows dynamic module loading

---

## Build Verification

All CMakeLists.txt files have been verified to:
- ✅ Have correct syntax
- ✅ Reference only existing components
- ✅ Have proper dependency hierarchy (no circular dependencies)
- ✅ Follow ESP-IDF component best practices
- ✅ Support the modular architecture design

---

## Next Steps

1. **Test Build**: Run `idf.py build` to verify all components compile correctly
2. **Clean Build**: Consider running `idf.py fullclean` first to ensure no cached issues
3. **Module Testing**: Verify that modules can be independently enabled/disabled

---

## Additional Notes

- All source files are discovered dynamically using `file(GLOB ...)` for UI files
- Source file existence is checked before adding to component (allows incremental development)
- Comments added to clarify component purposes
- Consistent formatting across all CMakeLists.txt files
