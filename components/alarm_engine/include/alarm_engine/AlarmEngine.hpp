// AlarmEngine.hpp - Alarm Comparator & Piezo Buzzer
#pragma once

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include <cstdint>

namespace Clock {

class AlarmEngine {
private:
    gpio_num_t buzzerPin_;
    uint8_t alarmHour_{7};
    uint8_t alarmMinute_{0};
    bool enabled_{true};  // default enabled as per HTML
    bool ringing_{false};
    uint32_t ringStartMs_{0};
    static constexpr uint32_t RING_DURATION_MS = 30000; // 30 seconds

public:
    explicit AlarmEngine(gpio_num_t pin) : buzzerPin_(pin) {}

    void init() {
        gpio_set_direction(buzzerPin_, GPIO_MODE_OUTPUT);
        gpio_set_level(buzzerPin_, 0);
        
        // LEDC config for PWM tone (optional for Wokwi buzzer)
        ledc_timer_config_t ledc_timer = {};
        ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
        ledc_timer.duty_resolution = LEDC_TIMER_10_BIT;
        ledc_timer.timer_num = LEDC_TIMER_0;
        ledc_timer.freq_hz = 2000;
        ledc_timer.clk_cfg = LEDC_AUTO_CLK;
        ledc_timer_config(&ledc_timer);
        
        ledc_channel_config_t ledc_channel = {};
        ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
        ledc_channel.channel = LEDC_CHANNEL_0;
        ledc_channel.timer_sel = LEDC_TIMER_0;
        ledc_channel.gpio_num = buzzerPin_;
        ledc_channel.duty = 0;
        ledc_channel.hpoint = 0;
        ledc_channel_config(&ledc_channel);
    }

    void setAlarm(uint8_t h, uint8_t m) {
        alarmHour_ = h % 24;
        alarmMinute_ = m % 60;
    }

    // Bidirectional adjustment.
    //   adjustHour(-1): 07:00 -> 06:00 (hour only)
    //   adjustMinute(-1): 07:00 -> 06:59 (minute borrows from the hour)
    void adjustHour(int delta) {
        int h = (static_cast<int>(alarmHour_) + delta) % 24;
        if (h < 0) h += 24;
        alarmHour_ = static_cast<uint8_t>(h);
    }

    void adjustMinute(int delta) {
        int total = (static_cast<int>(alarmHour_) * 60 + static_cast<int>(alarmMinute_) + delta) % (24 * 60);
        if (total < 0) total += 24 * 60;
        alarmHour_ = static_cast<uint8_t>(total / 60);
        alarmMinute_ = static_cast<uint8_t>(total % 60);
    }

    uint8_t getAlarmHour() const { return alarmHour_; }
    uint8_t getAlarmMinute() const { return alarmMinute_; }

    void enable(bool en) { enabled_ = en; }
    bool isEnabled() const { return enabled_; }
    bool isRinging() const { return ringing_; }

    void checkAlarm(uint8_t h, uint8_t m) {
        const uint32_t nowMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (ringing_) {
            if ((nowMs - ringStartMs_) > RING_DURATION_MS) {
                stopRing();
            }
            return;
        }
        if (enabled_ && h == alarmHour_ && m == alarmMinute_) {
            startRing(nowMs);
        }
    }

    void startRing(uint32_t nowMs) {
        ringing_ = true;
        ringStartMs_ = nowMs;
        // Generate 2kHz tone via LEDC (50% duty ~ 512 for 10-bit)
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }

    void stopRing() {
        ringing_ = false;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }

    void testBuzzer() {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        vTaskDelay(pdMS_TO_TICKS(500));
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
};

} // namespace Clock