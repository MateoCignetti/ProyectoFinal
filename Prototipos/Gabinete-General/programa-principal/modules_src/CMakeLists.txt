idf_component_register(SRCS "module_manager.c" "module_connection.c" "dimmer_control.c" ""
                    INCLUDE_DIRS "../modules_include"
                    PRIV_REQUIRES freertos esp_driver_gpio esp_driver_gptimer esp_adc esp_driver_ledc)