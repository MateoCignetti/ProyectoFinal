# Hardware Component
# Provides hardware abstraction layer for GPIO definitions and common hardware utilities
# This is a header-only component

idf_component_register(
    INCLUDE_DIRS 
        "include"
    REQUIRES
        "esp_adc"
)
