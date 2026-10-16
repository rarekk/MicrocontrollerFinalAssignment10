// DS1307.cpp - DS1307 Implementation
#include "ds1307/DS1307.hpp"

namespace Clock {

bool DS1307::init() {
    uint8_t secondsReg = 0;
    if (bus_.readRegs(I2C_ADDR, 0x00, &secondsReg, 1)) {
        // Clear CH (Clock Halt) bit if set
        if (secondsReg & 0x80) {
            uint8_t cleared = secondsReg & 0x7F;
            return bus_.writeReg(I2C_ADDR, 0x00, cleared);
        }
        return true;
    }
    return false;
}

bool DS1307::getTime(DateTime &dt) {
    uint8_t raw[7] = {0};
    if (!bus_.readRegs(I2C_ADDR, 0x00, raw, 7)) {
        return false;
    }
    dt.second = bcd2dec(raw[0] & 0x7F);
    dt.minute = bcd2dec(raw[1] & 0x7F);
    dt.hour = bcd2dec(raw[2] & 0x3F);
    dt.dayOfWeek = bcd2dec(raw[3] & 0x07);
    dt.dayOfMonth = bcd2dec(raw[4] & 0x3F);
    dt.month = bcd2dec(raw[5] & 0x1F);
    dt.year = 2000 + bcd2dec(raw[6]);
    return true;
}

bool DS1307::setTime(const DateTime &dt) {
    uint8_t raw[7];
    raw[0] = dec2bcd(dt.second) & 0x7F;
    raw[1] = dec2bcd(dt.minute) & 0x7F;
    raw[2] = dec2bcd(dt.hour) & 0x3F;
    raw[3] = dec2bcd(dt.dayOfWeek) & 0x07;
    raw[4] = dec2bcd(dt.dayOfMonth) & 0x3F;
    raw[5] = dec2bcd(dt.month) & 0x1F;
    raw[6] = dec2bcd(dt.year - 2000);
    
    // Write register by register or bulk? Use writeReg chain via bus semantics
    // Simple approach: write start reg 0x00 with buffer - but writeReg writes single; 
    // For completeness do sequential (bus is thread-safe)
    for (int i = 0; i < 7; i++) {
        if (!bus_.writeReg(I2C_ADDR, (uint8_t)(0x00 + i), raw[i])) {
            return false;
        }
    }
    return true;
}

} // namespace Clock