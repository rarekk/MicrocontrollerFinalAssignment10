// time_manager.cpp - Wi-Fi + SNTP + Time Sync
#include "time_manager/TimeManager.hpp"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <sys/time.h>

static const char *TAG_TM = "TIME_MGR";

namespace Clock {

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // IP obtained; SNTP will be configured separately
    }
}

void TimeManager::initWifiNTP(const char *ssid, const char *pass) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
    
    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    wifiConnected_ = true; // optimistic for simulation; real HW may differ
    
    // SNTP config
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_init();
    
    ntpSynced_ = false;
    lastNtpSyncMs_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    ESP_LOGI(TAG_TM, "Wi-Fi STA init done. SNTP to pool.ntp.org");
}

void TimeManager::taskNtpLoop() {
    // SNTP sync check
    if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        if (!ntpSynced_) {
            ntpSynced_ = true;
            // Sync RTC from system time after NTP
            syncRtcFromSystem();
            ESP_LOGI(TAG_TM, "NTP sync completed. RTC updated.");
        }
    } else {
        // Attempt sync if not completed; esp_sntp handles periodic
        // Try to sync system from RTC as fallback
        if (!ntpSynced_) {
            syncSystemFromRtc();
        }
    }
    
    // Periodic RTC->System refresh if no NTP
    if (!ntpSynced_) {
        syncSystemFromRtc();
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void TimeManager::requestNtpSync() {
    // Force an immediate SNTP poll (mirrors the HTML "Force NTP Sync" button).
    // esp_sntp_restart() is an alias of sntp_restart() in ESP-IDF v6.x.
    ESP_LOGI(TAG_TM, "Manual NTP resync requested -> restarting SNTP");
    ntpSynced_ = false;
    esp_sntp_restart();
}

void TimeManager::syncRtcFromSystem() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    DateTime dt;
    dt.second = timeinfo.tm_sec;
    dt.minute = timeinfo.tm_min;
    dt.hour = timeinfo.tm_hour;
    dt.dayOfWeek = timeinfo.tm_wday == 0 ? 7 : timeinfo.tm_wday; // optional mapping
    dt.dayOfMonth = timeinfo.tm_mday;
    dt.month = timeinfo.tm_mon + 1;
    dt.year = 1900 + timeinfo.tm_year;
    rtc_.setTime(dt);
}

void TimeManager::syncSystemFromRtc() {
    DateTime dt;
    if (!rtc_.getTime(dt)) return;
    
    struct tm timeinfo = {};
    timeinfo.tm_sec = dt.second;
    timeinfo.tm_min = dt.minute;
    timeinfo.tm_hour = dt.hour;
    timeinfo.tm_mday = dt.dayOfMonth;
    timeinfo.tm_mon = dt.month - 1;
    timeinfo.tm_year = dt.year - 1900;
    
    time_t t = mktime(&timeinfo);
    struct timeval tv = {t, 0};
    settimeofday(&tv, NULL);
}

void TimeManager::renderTime(SSD1306 &display) {
    DateTime dt;
    if (rtc_.getTime(dt)) {
        char buf[20];
        // HH:MM:SS (24h) or hh:MM:SS AM/PM (12h)
        int hour = dt.hour;
        bool pm = false;
        if (!use24Hour_) {
            pm = (hour >= 12);
            hour %= 12;
            if (hour == 0) hour = 12;
        }
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, dt.minute, dt.second);
        display.drawString(32, 20, buf, false);
        if (!use24Hour_) {
            display.drawString(84, 20, pm ? "PM" : "AM", false);
        }
        // Date DD/MM/YYYY
        snprintf(buf, sizeof(buf), "%02d/%02d/%04d", dt.dayOfMonth, dt.month, dt.year);
        display.drawString(28, 36, buf, false);
    } else {
        display.drawString(16, 20, "--:--:--", true);
        display.drawString(16, 36, "RTC ERR", true);
    }
}

} // namespace Clock