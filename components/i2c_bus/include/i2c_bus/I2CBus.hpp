// I2CBus.hpp - Thread-Safe ESP-IDF v6 I2C Master Wrapper
#pragma once

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <cstddef>

namespace Clock {

class I2CBus {
private:
    i2c_port_t port_;
    gpio_num_t sdaPin_;
    gpio_num_t sclPin_;
    i2c_master_bus_handle_t busHandle_{nullptr};
    SemaphoreHandle_t mutex_{nullptr};

public:
    I2CBus(i2c_port_t port, gpio_num_t sda, gpio_num_t scl)
        : port_(port), sdaPin_(sda), sclPin_(scl) {
        mutex_ = xSemaphoreCreateMutex();
    }

    virtual ~I2CBus() {
        // Cleanup if needed (ESP-IDF handles on shutdown)
        if (mutex_) {
            vSemaphoreDelete(mutex_);
            mutex_ = nullptr;
        }
    }

    bool init() {
        i2c_master_bus_config_t bus_config = {};
        bus_config.i2c_port = port_;
        bus_config.sda_io_num = sdaPin_;
        bus_config.scl_io_num = sclPin_;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = true;

        esp_err_t err = i2c_new_master_bus(&bus_config, &busHandle_);
        if (err != ESP_OK) {
            return false;
        }
        return true;
    }

    // Thread-safe I2C write (register-based style)
    bool writeReg(uint8_t devAddr, uint8_t reg, uint8_t data) {
        if (busHandle_ == nullptr) return false;
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return false;

        uint8_t write_buf[2] = { reg, data };
        i2c_device_config_t dev_cfg = {};
        dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dev_cfg.device_address = devAddr;
        dev_cfg.scl_speed_hz = 400000;
        dev_cfg.scl_wait_us = 0;
        dev_cfg.flags.disable_ack_check = 0;

        i2c_master_dev_handle_t dev_handle;
        esp_err_t err = i2c_master_bus_add_device(busHandle_, &dev_cfg, &dev_handle);
        if (err != ESP_OK) {
            xSemaphoreGive(mutex_);
            return false;
        }

        err = i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), pdMS_TO_TICKS(100));
        i2c_master_bus_rm_device(dev_handle);

        xSemaphoreGive(mutex_);
        return (err == ESP_OK);
    }

    // Thread-safe bulk I2C write. The caller supplies the complete payload
    // (including any SSD1306 control byte), so an entire command set or a full
    // display page can be streamed in a single I2C transaction.
    bool writeBytes(uint8_t devAddr, const uint8_t *data, size_t len) {
        if (busHandle_ == nullptr || data == nullptr || len == 0) return false;
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return false;

        i2c_device_config_t dev_cfg = {};
        dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dev_cfg.device_address = devAddr;
        dev_cfg.scl_speed_hz = 400000;
        dev_cfg.scl_wait_us = 0;
        dev_cfg.flags.disable_ack_check = 0;

        i2c_master_dev_handle_t dev_handle;
        esp_err_t err = i2c_master_bus_add_device(busHandle_, &dev_cfg, &dev_handle);
        if (err != ESP_OK) {
            xSemaphoreGive(mutex_);
            return false;
        }

        err = i2c_master_transmit(dev_handle, data, len, pdMS_TO_TICKS(200));
        i2c_master_bus_rm_device(dev_handle);

        xSemaphoreGive(mutex_);
        return (err == ESP_OK);
    }

    // Thread-safe I2C read
    bool readRegs(uint8_t devAddr, uint8_t startReg, uint8_t *buffer, size_t len) {
        if (busHandle_ == nullptr || buffer == nullptr || len == 0) return false;
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return false;

        i2c_device_config_t dev_cfg = {};
        dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dev_cfg.device_address = devAddr;
        dev_cfg.scl_speed_hz = 400000;
        dev_cfg.flags.disable_ack_check = 0;

        i2c_master_dev_handle_t dev_handle;
        esp_err_t err = i2c_master_bus_add_device(busHandle_, &dev_cfg, &dev_handle);
        if (err != ESP_OK) {
            xSemaphoreGive(mutex_);
            return false;
        }

        // Write register address then read
        err = i2c_master_transmit_receive(dev_handle, &startReg, 1, buffer, len, pdMS_TO_TICKS(100));
        i2c_master_bus_rm_device(dev_handle);

        xSemaphoreGive(mutex_);
        return (err == ESP_OK);
    }

};

} // namespace Clock