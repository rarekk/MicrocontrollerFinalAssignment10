// SSD1306.hpp - 128x64 Monochrome OLED Display Driver
#pragma once

#include "i2c_bus/I2CBus.hpp"
#include <cstddef>
#include <cstdint>

namespace Clock {

class SSD1306 {
private:
    I2CBus &bus_;
    uint8_t addr_;
    // 128 x 64 pixels = 1024 bytes
    uint8_t buffer_[1024];

    bool sendCommands(const uint8_t *cmds, size_t len);
    bool sendBuffer();

public:
    SSD1306(I2CBus &bus, uint8_t addr = 0x3C)
        : bus_(bus), addr_(addr) {}

    bool init();
    void clear();
    void setPixel(int x, int y, bool color);
    void drawString(int x, int y, const char *str, bool invert = false);
    void drawChar(int x, int y, char c, bool invert = false);
    void renderHeader(bool wifiConnected);
    void flush();
};

} // namespace Clock