idf_component_register(SRCS "module_dimmer.c" ""
                    INCLUDE_DIRS "../modules_include"
                    PRIV_REQUIRES esp_driver_gpio esp_driver_gptimer esp_adc esp_driver_ledc)