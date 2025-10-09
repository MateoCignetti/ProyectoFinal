# UI Framework Component
# Provides common UI utilities, LVGL configuration, and input device handling

# Check if source files exist
set(SRCS "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui_config.c")
    list(APPEND SRCS "src/ui_config.c")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/encoder.c")
    list(APPEND SRCS "src/encoder.c")
endif()

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
