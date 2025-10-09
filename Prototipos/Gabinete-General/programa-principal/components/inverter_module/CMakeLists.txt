# Inverter Module Component
# Self-contained inverter module with control logic and UI

# Collect source files
set(MODULE_SRCS "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/inverter_interface.c")
    list(APPEND MODULE_SRCS "src/inverter_interface.c")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/inverter_control.c")
    list(APPEND MODULE_SRCS "src/inverter_control.c")
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
        esp_lcd
        esp_timer
        hardware
        module_manager
        ui_framework
)