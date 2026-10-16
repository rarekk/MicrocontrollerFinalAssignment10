// DS1307.hpp - Real Time Clock Driver Header
#pragma once

#include "i2c_bus/I2CBus.hpp"
#include <ctime>
#include <cstdint>

namespace Clock {

struct DateTime {
    uint8_t second;      // 0-59
    uint8_t minute;      // 0-59
    uint8_t hour;        // 0-23 (24-hour mode)
    uint8_t dayOfWeek;   // 1-7 (Sun=1 depending on chip, but we track)
    uint8_t dayOfMonth;  // 1-31
    uint8_t month;       // 1-12
    uint16_t year;       // e.g. 2026
};

class DS1307 {
private:
    I2CBus &bus_;
    static constexpr uint8_t I2C_ADDR = 0x68; // DS1307 7-bit address

    // BCD helpers (DS1307 stores in BCD format)
    inline uint8_t bcd2dec(uint8_t val) const {
        return ((val / 16) * 10 + (val % 16));
    }

    inline uint8_t dec2bcd(uint8_t val) const {
        return ((val / 10) * 16 + (val % 10));
    }

public:
    explicit DS1307(I2CBus &bus) : bus_(bus) {}

    bool init();
    bool getTime(DateTime &dt);
    bool setTime(const DateTime &dt);
};

} // namespace Clock