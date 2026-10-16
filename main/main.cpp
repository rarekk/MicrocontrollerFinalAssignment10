// main.cpp - ESP32 Digital Clock Application Entry Point (ESP-IDF v6.1)
// Modular OOP + FreeRTOS Architecture inspired by HTML Suite/Dashboard

#include <stdio.h>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_sntp.h"

#include "i2c_bus/I2CBus.hpp"
#include "ds1307/DS1307.hpp"
#include "ssd1306/SSD1306.hpp"
#include "button_manager/ButtonManager.hpp"
#include "time_manager/TimeManager.hpp"
#include "alarm_engine/AlarmEngine.hpp"
#include "stopwatch_engine/StopwatchEngine.hpp"

static const char *TAG = "MAIN_APP";

using namespace Clock;

// Shared system objects
static I2CBus i2cBus(I2C_NUM_0, GPIO_NUM_21, GPIO_NUM_22);
static DS1307 rtcDriver(i2cBus);
static SSD1306 display(i2cBus, 0x3C);
static ButtonManager buttonMgr;
static TimeManager timeMgr(rtcDriver);
static AlarmEngine alarmEngine(GPIO_NUM_25);
static StopwatchEngine stopwatchEngine;

// FreeRTOS Task Handlers (optional)
static TaskHandle_t hTaskButton = nullptr;
static TaskHandle_t hTaskNtp = nullptr;
static TaskHandle_t hTaskAlarm = nullptr;
static TaskHandle_t hTaskDisplay = nullptr;

// Inter-task synchronisation event group (Spiral 5: FreeRTOS integration)
static EventGroupHandle_t s_sysEvents = nullptr;
#define SYS_WIFI_CONNECTED_BIT  (1U << 0)
#define SYS_NTP_SYNCED_BIT      (1U << 1)
#define SYS_ALARM_RINGING_BIT   (1U << 2)

// UI mode driven by the MODE push button
enum class UiMode : int {
    CLOCK = 0,
    DATE  = 1,
    STOPWATCH = 2,
    ALARM = 3,
    TIME_SET = 4,
    COUNT = 5
};
static volatile UiMode g_uiMode = UiMode::CLOCK;

// Manual time-set edit buffer (TIME_SET view); seeded from the RTC on entry
static Clock::DateTime g_setTime = {};
static bool g_setTimeValid = false;

// Wi-Fi credentials (matching HTML: iotexpo / 12345678)
#define WIFI_SSID      "iotexpo"
#define WIFI_PASS      "12345678"
#define NTP_SYNC_INTERVAL_HOURS 1

// Forward declarations
static void task_button_handler(void *pvParameters);
static void task_ntp_sync(void *pvParameters);
static void task_alarm_check(void *pvParameters);
static void task_display_refresh(void *pvParameters);
static void init_nvs_wifi(void);
static void handleButtonEvent(const Clock::ButtonEvent &ev);
static void adjustClockTime(Clock::DateTime &t, int deltaMinutes, bool hourOnly);
static void drawCentered(SSD1306 &d, int y, const char *s, bool invert = false);
static const char *uiModeName(UiMode m);

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Initializing Modular C++ OOP Digital Clock System [ESP-IDF v6.1]");
    
    // NVS + Wi-Fi stack
    init_nvs_wifi();
    
    // Spiral 1: HAL Layer Initialization
    ESP_LOGI(TAG, "[SPIRAL 1] Initializing I2C Bus, DS1307 RTC, SSD1306 OLED...");
    if (!i2cBus.init()) {
        ESP_LOGE(TAG, "I2C Bus initialization FAILED!");
    } else {
        ESP_LOGI(TAG, "I2C Bus initialized successfully (SDA:21, SCL:22)");
    }
    
    if (!rtcDriver.init()) {
        ESP_LOGE(TAG, "DS1307 RTC initialization FAILED!");
    } else {
        ESP_LOGI(TAG, "DS1307 RTC initialized successfully (0x68)");
    }
    
    if (!display.init()) {
        ESP_LOGE(TAG, "SSD1306 OLED initialization FAILED!");
    } else {
        ESP_LOGI(TAG, "SSD1306 OLED initialized successfully (128x64, 0x3C)");
    }
    
    // Spiral 2-3: Input/Time/Engines
    ESP_LOGI(TAG, "[SPIRAL 2/3] Initializing Button Manager & Time Manager...");
    buttonMgr.init();
    timeMgr.initWifiNTP(WIFI_SSID, WIFI_PASS);
    alarmEngine.init();

    // FreeRTOS event group used to publish WiFi / NTP / alarm state between tasks
    s_sysEvents = xEventGroupCreate();
    configASSERT(s_sysEvents != nullptr);
    
    // Spawn FreeRTOS Tasks
    xTaskCreatePinnedToCore(task_button_handler, "vTaskButtonHandler", 3072, NULL, 5, &hTaskButton, 0);
    xTaskCreatePinnedToCore(task_ntp_sync, "vTaskNTPSync", 6144, NULL, 1, &hTaskNtp, 0);
    xTaskCreatePinnedToCore(task_alarm_check, "vTaskAlarmCheck", 2048, NULL, 4, &hTaskAlarm, 1);
    xTaskCreatePinnedToCore(task_display_refresh, "vTaskClockDisplay", 4096, NULL, 3, &hTaskDisplay, 1);
    
    ESP_LOGI(TAG, "All FreeRTOS tasks created. System running...");
    
    // Main idle hook (app_main can yield)
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void init_nvs_wifi(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    ESP_LOGI(TAG, "NVS + TCP/IP stack initialized");
}

static void task_button_handler(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "vTaskButtonHandler started [Priority:5, Core:0]");

    Clock::ButtonEvent ev;
    while (true) {
        // Sample the GPIOs / debounce and push events into the queue
        buttonMgr.taskLoop();

        // Drain and act on every queued press
        while (xQueueReceive(buttonMgr.getQueue(), &ev, 0) == pdTRUE) {
            handleButtonEvent(ev);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ---------------------------------------------------------------------------
// Push-button behaviour
//   (any)       : if the alarm is ringing, any press dismisses it
//   MODE  short : cycle CLOCK -> DATE -> STOPWATCH -> ALARM -> TIME_SET -> CLOCK
//   MODE  long  : reset the stopwatch
//   SET   short : STOPWATCH -> start/pause ; ALARM -> toggle alarm on/off
//                 TIME_SET  -> write the edited time to the RTC
//   SET   long  : CLOCK -> toggle 12h/24h display format
//   ALARM short : ALARM/TIME_SET -> minute+1 ; else toggle alarm on/off
//   ALARM long  : (non-edit views only) buzzer self-test
//   LAP   short : STOPWATCH -> record lap ; ALARM/TIME_SET -> minute-1
//   UP    short : ALARM/TIME_SET -> hour+1
//   DOWN  short : ALARM/TIME_SET -> hour-1
// (Each edit action is a single short press, so no long-press/hold is needed.)
// ---------------------------------------------------------------------------
static const char *uiModeName(UiMode m)
{
    switch (m) {
    case UiMode::CLOCK:     return "CLOCK";
    case UiMode::DATE:      return "DATE";
    case UiMode::STOPWATCH: return "STOPWATCH";
    case UiMode::ALARM:     return "ALARM";
    case UiMode::TIME_SET:  return "TIME_SET";
    default:                return "?";
    }
}

// Bidirectional clock edit helper.
//   hourOnly=true : adjust whole hours (+/-1h)
//   hourOnly=false: adjust minutes (+/-1min), carrying/borrowing the hour
static void adjustClockTime(Clock::DateTime &t, int deltaMinutes, bool hourOnly)
{
    int total = static_cast<int>(t.hour) * 60 + static_cast<int>(t.minute);
    total += hourOnly ? deltaMinutes * 60 : deltaMinutes;
    total %= (24 * 60);
    if (total < 0) total += (24 * 60);
    t.hour = static_cast<uint8_t>(total / 60);
    t.minute = static_cast<uint8_t>(total % 60);
}

static void handleButtonEvent(const Clock::ButtonEvent &ev)
{
    const bool isLong = (ev.type == Clock::PressType::LONG);

    // A ringing alarm is dismissed by any button press (the engine also
    // auto-stops it after RING_DURATION_MS).
    if (alarmEngine.isRinging()) {
        alarmEngine.stopRing();
        ESP_LOGI(TAG, "[BTN] button %d pressed -> alarm dismissed",
                 static_cast<int>(ev.id));
        return;
    }

    switch (ev.id) {
    case Clock::ButtonId::MODE:
        if (isLong) {
            stopwatchEngine.reset();
            ESP_LOGI(TAG, "[BTN] MODE long  -> stopwatch reset");
        } else {
            g_uiMode = static_cast<UiMode>((static_cast<int>(g_uiMode) + 1) %
                                           static_cast<int>(UiMode::COUNT));
            if (g_uiMode == UiMode::TIME_SET) {
                // Seed the manual-set buffer from the current RTC time
                g_setTimeValid = rtcDriver.getTime(g_setTime);
                ESP_LOGI(TAG, "[BTN] MODE short -> view: TIME_SET (RTC seed %s)",
                         g_setTimeValid ? "ok" : "failed");
            } else {
                ESP_LOGI(TAG, "[BTN] MODE short -> view: %s", uiModeName(g_uiMode));
            }
        }
        break;

    case Clock::ButtonId::SET:
        if (isLong) {
            // CLOCK view: toggle the 12h / 24h display format
            if (g_uiMode == UiMode::CLOCK) {
                timeMgr.set24Hour(!timeMgr.is24Hour());
                ESP_LOGI(TAG, "[BTN] SET long -> %s-hour display",
                         timeMgr.is24Hour() ? "24" : "12");
            }
            break;
        }
        switch (g_uiMode) {
        case UiMode::ALARM:
            alarmEngine.enable(!alarmEngine.isEnabled());
            ESP_LOGI(TAG, "[BTN] SET  -> alarm %s", alarmEngine.isEnabled() ? "ON" : "OFF");
            break;
        case UiMode::TIME_SET:
            if (g_setTimeValid) {
                g_setTime.second = 0;
                if (rtcDriver.setTime(g_setTime)) {
                    timeMgr.syncSystemFromRtc();
                    ESP_LOGI(TAG, "[BTN] SET  -> RTC written %02d:%02d:00",
                             g_setTime.hour, g_setTime.minute);
                } else {
                    ESP_LOGE(TAG, "[BTN] SET  -> RTC write FAILED");
                }
            } else {
                ESP_LOGW(TAG, "[BTN] SET  -> nothing to write (RTC read failed)");
            }
            break;
        case UiMode::STOPWATCH:
            if (stopwatchEngine.isRunning()) {
                stopwatchEngine.pause();
                ESP_LOGI(TAG, "[BTN] SET  -> stopwatch paused");
            } else {
                stopwatchEngine.start();
                ESP_LOGI(TAG, "[BTN] SET  -> stopwatch started");
            }
            break;
        default:
            break; // CLOCK / DATE: no action
        }
        break;

    case Clock::ButtonId::ALARM:
        // In the edit views ALARM moves the MINUTES forward (single press)
        if (g_uiMode == UiMode::ALARM) {
            alarmEngine.adjustMinute(+1);
            ESP_LOGI(TAG, "[BTN] ALARM -> minute+1 = %02d:%02d",
                     alarmEngine.getAlarmHour(), alarmEngine.getAlarmMinute());
        } else if (g_uiMode == UiMode::TIME_SET) {
            if (g_setTimeValid) adjustClockTime(g_setTime, +1, false);
            ESP_LOGI(TAG, "[BTN] TIME_SET minute+1 -> %02d:%02d", g_setTime.hour, g_setTime.minute);
        } else if (isLong) {
            alarmEngine.testBuzzer();
            ESP_LOGI(TAG, "[BTN] ALARM long -> buzzer test (GPIO25 PWM @2kHz)");
        } else {
            alarmEngine.enable(!alarmEngine.isEnabled());
            ESP_LOGI(TAG, "[BTN] ALARM -> alarm %s", alarmEngine.isEnabled() ? "ON" : "OFF");
        }
        break;

    case Clock::ButtonId::LAP:
        // In the edit views LAP moves the MINUTES backward (single press)
        if (g_uiMode == UiMode::ALARM) {
            alarmEngine.adjustMinute(-1);
            ESP_LOGI(TAG, "[BTN] LAP -> minute-1 = %02d:%02d",
                     alarmEngine.getAlarmHour(), alarmEngine.getAlarmMinute());
        } else if (g_uiMode == UiMode::TIME_SET) {
            if (g_setTimeValid) adjustClockTime(g_setTime, -1, false);
            ESP_LOGI(TAG, "[BTN] TIME_SET minute-1 -> %02d:%02d", g_setTime.hour, g_setTime.minute);
        } else {
            stopwatchEngine.recordLap();
            ESP_LOGI(TAG, "[BTN] LAP  -> lap %u", static_cast<unsigned>(stopwatchEngine.getLapCount()));
        }
        break;

    case Clock::ButtonId::UP:
        // Dedicated HOUR +1 button (single press)
        if (g_uiMode == UiMode::ALARM) {
            alarmEngine.adjustHour(+1);
            ESP_LOGI(TAG, "[BTN] UP -> hour+1 = %02d:%02d",
                     alarmEngine.getAlarmHour(), alarmEngine.getAlarmMinute());
        } else if (g_uiMode == UiMode::TIME_SET) {
            if (g_setTimeValid) adjustClockTime(g_setTime, +1, true);
            ESP_LOGI(TAG, "[BTN] TIME_SET hour+1 -> %02d:%02d", g_setTime.hour, g_setTime.minute);
        } else if (g_uiMode == UiMode::CLOCK) {
            // CLOCK view: force an NTP resynchronisation (HTML "Force NTP Sync")
            timeMgr.requestNtpSync();
            ESP_LOGI(TAG, "[BTN] UP (CLOCK) -> forced NTP resync");
        }
        break;

    case Clock::ButtonId::DOWN:
        // Dedicated HOUR -1 button (single press)
        if (g_uiMode == UiMode::ALARM) {
            alarmEngine.adjustHour(-1);
            ESP_LOGI(TAG, "[BTN] DOWN -> hour-1 = %02d:%02d",
                     alarmEngine.getAlarmHour(), alarmEngine.getAlarmMinute());
        } else if (g_uiMode == UiMode::TIME_SET) {
            if (g_setTimeValid) adjustClockTime(g_setTime, -1, true);
            ESP_LOGI(TAG, "[BTN] TIME_SET hour-1 -> %02d:%02d", g_setTime.hour, g_setTime.minute);
        }
        break;
    }
}

static void task_ntp_sync(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "vTaskNTPSync started [Priority:1, Core:0]");
    bool lastSynced = false;
    while (true) {
        timeMgr.taskNtpLoop();

        // Publish connectivity / NTP state for the other tasks (event group)
        if (timeMgr.isWifiConnected()) {
            xEventGroupSetBits(s_sysEvents, SYS_WIFI_CONNECTED_BIT);
        }
        if (timeMgr.isNtpSynced()) {
            xEventGroupSetBits(s_sysEvents, SYS_NTP_SYNCED_BIT);
            if (!lastSynced) {
                ESP_LOGI(TAG, "[NTP] system time synchronised, event group updated");
                lastSynced = true;
            }
        }

        // Sync every NTP_SYNC_INTERVAL_HOURS
        vTaskDelay(pdMS_TO_TICKS(NTP_SYNC_INTERVAL_HOURS * 3600 * 1000));
    }
}

static void task_alarm_check(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "vTaskAlarmCheck started [Priority:4, Core:1]");
    while (true) {
        // Get current time from RTC/TimeManager and check
        Clock::DateTime dt;
        rtcDriver.getTime(dt);
        alarmEngine.checkAlarm(dt.hour, dt.minute);

        // Publish the ringing state for other tasks / the renderer
        if (alarmEngine.isRinging()) {
            xEventGroupSetBits(s_sysEvents, SYS_ALARM_RINGING_BIT);
        } else {
            xEventGroupClearBits(s_sysEvents, SYS_ALARM_RINGING_BIT);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void drawCentered(SSD1306 &d, int y, const char *s, bool invert)
{
    int len = static_cast<int>(strlen(s));
    int x = (128 - len * 6) / 2;
    if (x < 0) x = 0;
    d.drawString(x, y, s, invert);
}

static void task_display_refresh(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "vTaskClockDisplay started [Priority:3, Core:1]");

    char buf[48];
    Clock::DateTime dt;

    while (true) {
        bool rtcOk = rtcDriver.getTime(dt);

        // Snapshot the state published by the other tasks via the event group
        EventBits_t sysBits = xEventGroupGetBits(s_sysEvents);
        const bool ntpOk   = (sysBits & SYS_NTP_SYNCED_BIT) != 0;
        const bool ringing = (sysBits & SYS_ALARM_RINGING_BIT) != 0;

        display.clear();
        display.renderHeader(ntpOk);

        switch (g_uiMode) {
        case UiMode::CLOCK:
            // Delegate the main clock view to the TimeManager (reuses the
            // library layout incl. the RTC-error fallback)
            timeMgr.renderTime(display);
            // Show the armed alarm time inline (dashboard parity)
            if (alarmEngine.isEnabled()) {
                snprintf(buf, sizeof(buf), "ALARM %02d:%02d",
                         alarmEngine.getAlarmHour(), alarmEngine.getAlarmMinute());
                drawCentered(display, 44, buf, false);
            }
            break;

        case UiMode::DATE:
            if (rtcOk) {
                snprintf(buf, sizeof(buf), "%02d/%02d/%04d", dt.dayOfMonth, dt.month, dt.year);
            } else {
                snprintf(buf, sizeof(buf), "--/--/----");
            }
            drawCentered(display, 22, buf, !rtcOk);
            snprintf(buf, sizeof(buf), "TIME %02d:%02d:%02d",
                     rtcOk ? dt.hour : 0, rtcOk ? dt.minute : 0, rtcOk ? dt.second : 0);
            drawCentered(display, 40, buf, false);
            break;

        case UiMode::STOPWATCH: {
            int64_t ms = stopwatchEngine.getElapsedMs();
            int minutes = static_cast<int>((ms / 60000) % 100);
            int seconds = static_cast<int>((ms / 1000) % 60);
            int cents   = static_cast<int>((ms / 10) % 100);
            snprintf(buf, sizeof(buf), "%02d:%02d.%02d", minutes, seconds, cents);
            drawCentered(display, 22, buf, false);

            const size_t lapCount = stopwatchEngine.getLapCount();
            if (lapCount > 0) {
                // Show the most recent lap split (last lap timestamp - previous)
                const int64_t last = stopwatchEngine.getLap(lapCount - 1);
                const int64_t prev = (lapCount >= 2) ? stopwatchEngine.getLap(lapCount - 2) : 0;
                const int64_t split = last - prev;
                const int lm = static_cast<int>((split / 60000) % 100);
                const int ls = static_cast<int>((split / 1000) % 60);
                const int lc = static_cast<int>((split / 10) % 100);
                snprintf(buf, sizeof(buf), "LAP%u %02d:%02d.%02d",
                         static_cast<unsigned>(lapCount), lm, ls, lc);
            } else {
                snprintf(buf, sizeof(buf), "%s  LAP:%u",
                         stopwatchEngine.isRunning() ? "RUN " : "STOP",
                         static_cast<unsigned>(lapCount));
            }
            drawCentered(display, 40, buf, false);
            break;
        }

        case UiMode::ALARM:
            snprintf(buf, sizeof(buf), "%02d:%02d",
                     alarmEngine.getAlarmHour(), alarmEngine.getAlarmMinute());
            drawCentered(display, 22, buf, false);
            snprintf(buf, sizeof(buf), "ALARM %s%s",
                     alarmEngine.isEnabled() ? "ON " : "OFF",
                     ringing ? "!" : " ");
            drawCentered(display, 40, buf, false);
            break;

        case UiMode::TIME_SET:
            if (g_setTimeValid) {
                snprintf(buf, sizeof(buf), "%02d:%02d", g_setTime.hour, g_setTime.minute);
                drawCentered(display, 22, buf, false);
                drawCentered(display, 40, "SET=WRITE", false);
            } else {
                drawCentered(display, 22, "--:--", true);
                drawCentered(display, 40, "RTC ERR", true);
            }
            break;

        default:
            break;
        }

        // Footer shows the active view / edit-key hints
        if (g_uiMode == UiMode::CLOCK) {
            snprintf(buf, sizeof(buf), "CLOCK %sH   U=NTP", timeMgr.is24Hour() ? "24" : "12");
        } else if (g_uiMode == UiMode::ALARM || g_uiMode == UiMode::TIME_SET) {
            snprintf(buf, sizeof(buf), "HR:UP/DN MIN:A/L");
        } else {
            snprintf(buf, sizeof(buf), "MODE:%s", uiModeName(g_uiMode));
        }
        display.drawString(0, 54, buf, false);

        display.flush();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}