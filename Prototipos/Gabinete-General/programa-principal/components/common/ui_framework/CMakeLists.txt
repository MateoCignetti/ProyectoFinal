# UI Framework Component
# Provides common UI utilities, LVGL configuration, and input device handling

# Collect source files
set(MODULE_SRCS "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui_config.c")
    list(APPEND MODULE_SRCS "src/ui_config.c")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/idle_screen.c")
    list(APPEND MODULE_SRCS "src/idle_screen.c")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.c")
    list(APPEND MODULE_SRCS "src/encoder.c")
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
        hardware
    PRIV_REQUIRES 
        freertos
        esp_driver_gpio
        esp_driver_spi
        esp_lcd
        esp_timer
)
