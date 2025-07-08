idf_component_register(SRCS "module_manager.c" "module_state.c" "dimmer_control.c" ""
                    INCLUDE_DIRS "../modules_include" "../include"
                    PRIV_REQUIRES esp_driver_gpio esp_driver_gptimer esp_adc esp_driver_ledc)