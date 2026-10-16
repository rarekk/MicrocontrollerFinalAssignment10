idf_component_register(SRCS "stopwatch_engine.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES driver freertos esp_timer)