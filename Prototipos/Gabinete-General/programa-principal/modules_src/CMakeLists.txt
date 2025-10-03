idf_component_register(SRCS "dimmer_control.c"
                            "buck_interface.c"
                            "buck_control.c"

                    INCLUDE_DIRS "../modules_include"
                                "../include"
                                "../modules_include/ui_buck"
                    PRIV_REQUIRES freertos
                                esp_driver_gpio
                                esp_driver_gptimer
                                esp_adc
                                esp_driver_ledc
                                lvgl__lvgl
                                ui_buck)