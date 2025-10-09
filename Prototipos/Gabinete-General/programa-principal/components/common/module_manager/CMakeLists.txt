# Module Manager Component
# Handles module detection, lifecycle management, and state coordination

# Check if source files exist
set(SRCS "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/module_manager.c")
    list(APPEND SRCS "src/module_manager.c")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/module_connection.c")
    list(APPEND SRCS "src/module_connection.c")
endif()

# Register component with proper dependencies
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
        esp_driver_gptimer
        buck_module
        dimmer_module
        inverter_module
)
