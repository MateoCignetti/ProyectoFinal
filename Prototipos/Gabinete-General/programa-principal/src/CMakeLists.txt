idf_component_register(SRCS "module_manager.c" "module_connection.c"
                    INCLUDE_DIRS "../include"
                    PRIV_REQUIRES freertos esp_driver_gpio esp_driver_gptimer esp_adc modules_src)