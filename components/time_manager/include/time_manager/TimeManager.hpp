// TimeManager.hpp - POSIX System & NTP Sync Subsystem
#pragma once

#include "ds1307/DS1307.hpp"
#include "ssd1306/SSD1306.hpp"
#include "esp_sntp.h"
#include <ctime>
#include <cstdint>

namespace Clock {

class TimeManager {
private:
    DS1307 &rtc_;
    bool ntpSynced_{false};
    bool wifiConnected_{false};
    bool use24Hour_{true}; // display format: 24h (default) or 12h with AM/PM
    uint32_t lastNtpSyncMs_{0};

public:
    explicit TimeManager(DS1307 &rtc) : rtc_(rtc) {}

    void initWifiNTP(const char *ssid, const char *pass);
    void taskNtpLoop();
    void requestNtpSync(); // manual (button-triggered) NTP resynchronisation
    bool isNtpSynced() const { return ntpSynced_; }
    bool isWifiConnected() const { return wifiConnected_; }
    void set24Hour(bool en) { use24Hour_ = en; }
    bool is24Hour() const { return use24Hour_; }
    void renderTime(SSD1306 &display);
    void syncRtcFromSystem();
    void syncSystemFromRtc();
};

} // namespace Clock