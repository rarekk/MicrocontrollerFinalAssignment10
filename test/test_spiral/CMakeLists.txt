idf_component_register(SRCS "test_spiral_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES unity ds1307 i2c_bus ssd1306)