// StopwatchEngine.hpp - Split Timing Engine (High-Res)
#pragma once

#include "esp_timer.h"
#include <cstddef>
#include <cstdint>

namespace Clock {

class StopwatchEngine {
private:
    int64_t startTimeUs_{0};
    int64_t elapsedUs_{0};
    bool running_{false};
    // Max 5 laps as per design
    int64_t laps_[5];
    size_t lapCount_{0};

public:
    StopwatchEngine() {
        startTimeUs_ = 0;
        elapsedUs_ = 0;
        running_ = false;
        lapCount_ = 0;
        for (int i = 0; i < 5; i++) laps_[i] = 0;
    }

    void start() {
        if (!running_) {
            startTimeUs_ = esp_timer_get_time();
            running_ = true;
        }
    }

    void pause() {
        if (running_) {
            elapsedUs_ += (esp_timer_get_time() - startTimeUs_);
            running_ = false;
        }
    }

    void reset() {
        running_ = false;
        elapsedUs_ = 0;
        lapCount_ = 0;
        for (int i = 0; i < 5; i++) laps_[i] = 0;
    }

    void recordLap() {
        if (lapCount_ < 5) {
            int64_t cur = getElapsedUs();
            laps_[lapCount_++] = cur;
        }
    }

    int64_t getElapsedUs() const {
        if (running_) {
            return elapsedUs_ + (esp_timer_get_time() - startTimeUs_);
        } else {
            return elapsedUs_;
        }
    }

    int64_t getElapsedMs() const {
        return getElapsedUs() / 1000;
    }

    bool isRunning() const { return running_; }
    size_t getLapCount() const { return lapCount_; }
    int64_t getLap(size_t idx) const { return (idx < lapCount_) ? laps_[idx] : 0; }
};

} // namespace Clock