idf_component_register(SRCS "ssd1306.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES driver freertos i2c_bus)