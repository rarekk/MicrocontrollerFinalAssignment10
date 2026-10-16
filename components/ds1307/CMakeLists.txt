idf_component_register(SRCS "ds1307.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES driver freertos i2c_bus)