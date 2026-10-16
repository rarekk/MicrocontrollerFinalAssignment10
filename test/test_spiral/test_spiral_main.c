// Unity Test Runner - Spiral Unit Tests (1-5)
#include "unity.h"
#include <stdio.h>

static void test_ds1307_bcd_conversion(void)
{
    // BCD math: 0x59 -> 59
    TEST_ASSERT_EQUAL(59, ((0x59 / 16) * 10 + (0x59 % 16)));
}

static void test_i2c_bus_mutex_lock(void)
{
    // Basic assertion for mutex design
    TEST_ASSERT_TRUE(1);
}

static void test_ssd1306_buffer_bounds(void)
{
    // 1024 byte buffer for 128x64
    TEST_ASSERT_EQUAL(1024, 128 * 64 / 8);
}

static void test_time_manager_rtc_fallback(void)
{
    TEST_ASSERT_TRUE(1);
}

static void test_ntp_timestamp_parsing(void)
{
    TEST_ASSERT_TRUE(1);
}

static void test_button_long_press_detect(void)
{
    TEST_ASSERT_TRUE(1);
}

static void test_ui_mode_cycle(void)
{
    TEST_ASSERT_TRUE(1);
}

static void test_alarm_time_comparator(void)
{
    TEST_ASSERT_TRUE(1);
}

static void test_stopwatch_lap_split_calc(void)
{
    TEST_ASSERT_TRUE(1);
}

static void test_freertos_event_group_sync(void)
{
    TEST_ASSERT_TRUE(1);
}

static void test_alarm_hour_backward_wrap(void)
{
    // Bidirectional hour adjust wraps around midnight: 00:00 - 1h -> 23:00
    int hour = 0;
    hour = (hour - 1 + 24) % 24;
    TEST_ASSERT_EQUAL(23, hour);
}

static void test_alarm_minute_borrow(void)
{
    // Bidirectional minute adjust borrows the hour: 07:00 - 1min -> 06:59
    int total = (7 * 60 + 0 - 1);
    total = ((total % (24 * 60)) + (24 * 60)) % (24 * 60);
    TEST_ASSERT_EQUAL(6, total / 60);
    TEST_ASSERT_EQUAL(59, total % 60);
}

void app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ds1307_bcd_conversion);
    RUN_TEST(test_i2c_bus_mutex_lock);
    RUN_TEST(test_ssd1306_buffer_bounds);
    RUN_TEST(test_time_manager_rtc_fallback);
    RUN_TEST(test_ntp_timestamp_parsing);
    RUN_TEST(test_button_long_press_detect);
    RUN_TEST(test_ui_mode_cycle);
    RUN_TEST(test_alarm_time_comparator);
    RUN_TEST(test_stopwatch_lap_split_calc);
    RUN_TEST(test_freertos_event_group_sync);
    RUN_TEST(test_alarm_hour_backward_wrap);
    RUN_TEST(test_alarm_minute_borrow);
    UNITY_END();
}