idf_component_register(SRCS "alarm_engine.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES driver freertos esp_driver_gpio esp_driver_ledc)