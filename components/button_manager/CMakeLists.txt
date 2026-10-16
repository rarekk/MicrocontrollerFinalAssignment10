idf_component_register(SRCS "button_manager.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES driver freertos esp_driver_gpio)