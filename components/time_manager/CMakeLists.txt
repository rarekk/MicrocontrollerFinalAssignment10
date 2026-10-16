idf_component_register(SRCS "time_manager.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES driver freertos esp_wifi esp_event nvs_flash lwip esp_netif ds1307 ssd1306)