idf_component_register(SRCS "dimmer_control.c"
                    INCLUDE_DIRS "../modules_include" "../include"
                    PRIV_REQUIRES freertos esp_driver_gpio esp_driver_gptimer esp_adc esp_driver_ledc)