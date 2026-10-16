// ButtonManager.hpp - GPIO ISR Switch Debouncer
#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <cstdint>

namespace Clock {

enum class ButtonId {
    MODE = 0,
    SET  = 1,
    ALARM = 2,
    LAP  = 3,
    UP   = 4,
    DOWN = 5
};

enum class PressType {
    SHORT = 0,
    LONG  = 1
};

struct ButtonEvent {
    ButtonId id;
    PressType type;
    uint32_t tick;
};

class ButtonManager {
private:
    QueueHandle_t eventQueue_;
    static constexpr gpio_num_t PIN_MODE  = GPIO_NUM_18;
    static constexpr gpio_num_t PIN_SET   = GPIO_NUM_19;
    static constexpr gpio_num_t PIN_ALARM = GPIO_NUM_4;
    static constexpr gpio_num_t PIN_LAP   = GPIO_NUM_5;
    static constexpr gpio_num_t PIN_UP    = GPIO_NUM_26;
    static constexpr gpio_num_t PIN_DOWN  = GPIO_NUM_27;

    static constexpr int NUM_BUTTONS = 6;

    // Debounce tracking
    uint32_t lastPressTick[NUM_BUTTONS] = {0, 0, 0, 0, 0, 0};
    bool lastState[NUM_BUTTONS] = {true, true, true, true, true, true}; // active LOW with pull-up

    // Long press threshold (ms)
    static constexpr uint32_t LONG_PRESS_MS = 1000;

public:
    ButtonManager() : eventQueue_(nullptr) {}

    void init() {
        eventQueue_ = xQueueCreate(16, sizeof(ButtonEvent));
        if (eventQueue_ == nullptr) return;

        // Configure GPIOs as input with pull-up
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_ANYEDGE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << PIN_MODE) | (1ULL << PIN_SET) |
                               (1ULL << PIN_ALARM) | (1ULL << PIN_LAP) |
                               (1ULL << PIN_UP) | (1ULL << PIN_DOWN);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);

        // Initialize states
        const gpio_num_t pins[NUM_BUTTONS] = {
            PIN_MODE, PIN_SET, PIN_ALARM, PIN_LAP, PIN_UP, PIN_DOWN
        };
        for (int i = 0; i < NUM_BUTTONS; i++) {
            lastState[i] = gpio_get_level(pins[i]);
            lastPressTick[i] = xTaskGetTickCount() * portTICK_PERIOD_MS;
        }
    }

    void taskLoop() {
        if (eventQueue_ == nullptr) {
            vTaskDelay(pdMS_TO_TICKS(50));
            return;
        }

        const uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        const uint32_t DEBOUNCE_MS = 50;

        // Check each button
        const struct {
            gpio_num_t pin;
            ButtonId id;
        } btns[NUM_BUTTONS] = {
            {PIN_MODE, ButtonId::MODE},
            {PIN_SET, ButtonId::SET},
            {PIN_ALARM, ButtonId::ALARM},
            {PIN_LAP, ButtonId::LAP},
            {PIN_UP, ButtonId::UP},
            {PIN_DOWN, ButtonId::DOWN}
        };

        for (int i = 0; i < NUM_BUTTONS; i++) {
            bool current = gpio_get_level(btns[i].pin); // 1 = released (pull-up), 0 = pressed
            if (lastState[i] != current) {
                // Debounce
                if ((now - lastPressTick[i]) > DEBOUNCE_MS) {
                    lastState[i] = current;
                    lastPressTick[i] = now;
                    // Falling edge = pressed
                    if (current == 0) {
                        lastPressTick[i] = now; // press start
                    } else {
                        // Rising edge = released
                        uint32_t pressDuration = now - lastPressTick[i];
                        ButtonEvent ev;
                        ev.id = btns[i].id;
                        ev.type = (pressDuration >= LONG_PRESS_MS) ? PressType::LONG : PressType::SHORT;
                        ev.tick = now;
                        xQueueSend(eventQueue_, &ev, 0);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    QueueHandle_t getQueue() const {
        return eventQueue_;
    }
};

} // namespace Clock
