# ESP32 Modular Digital Clock - Project Creation Report

**Project:** ESP32 Digital Clock Modular OOP (ESP-IDF v6.1 + FreeRTOS)  
**Target:** ESP32 DevKit C v4 (Wokwi Simulation)  
**Date:** October 17, 2026  
**Author:** ESP-IDF Expert Assistant

## 1. Project Backgrounds & Objectives

This project implements a modular, object-oriented digital clock firmware for ESP32 using ESP-IDF v6.1 and FreeRTOS. Key features:
- DS1307 RTC (I2C 0x68) with BCD timekeeping and battery backup simulation
- SSD1306 OLED (128x64, I2C 0x3C) for UI rendering
- 6 tactile push buttons (MODE, SET, ALARM, LAP, UP, DOWN) driving a 5-view UI (clock / date / stopwatch / alarm / manual time-set)
- Bidirectional adjustment via dedicated single-press buttons (`UP`/`DOWN` = hour +/-, `ALARM`/`LAP` = minute +/-), with correct wrap/borrow (e.g. 07:00 -> 06:00 for hour, 07:00 -> 06:59 for minute)
- Piezo buzzer (GPIO 25) driven by LEDC PWM for alarm tones
- Wi-Fi Station (SSID: `iotexpo`, PASS: `12345678`) with SNTP sync to `pool.ntp.org`, plus a manual NTP resync (`UP` on CLOCK)
- FreeRTOS multitasking with priority-based scheduling and I2C mutex for thread safety
- Bottom-Up Spiral Methodology (Spirals 1-5) with 12 Unity unit tests

## 2. File Structure Creation Steps

Created ESP-IDF standard layout:
```
C:\microcontrollerPJ\FinalAssignment10\
├── CMakeLists.txt (root project)
├── diagram.json (Wokwi hardware)
├── wokwi.toml (firmware mapping)
├── partitions.csv (Flash layout)
├── sdkconfig.defaults (defaults)
├── main/ (app_main + task orchestration)
├── components/ (7 modular components: i2c_bus, ds1307, ssd1306, button_manager, time_manager, alarm_engine, stopwatch_engine)
├── test/test_spiral/ (Unity test runner Spirals 1-5)
└── docs/REPORT.md
```

Each component uses `include/<component_name>/` header placement and proper `REQUIRES` in CMakeLists.

## 3. Wokwi Project Setup

- `diagram.json`: ESP32 DevKit C V4 connected to DS1307 RTC, SSD1306 OLED, 4 push buttons and a buzzer
- `wokwi.toml`: points at the build artifacts
- `partitions.csv`: 1 MB `factory` app partition at `0x10000`

### 3.1 Wiring pin-name corrections (buttons appeared "not connected")

The diagram originally copied the wiring from the supplied HTML examples. Those used
legacy / invalid Wokwi pin names, so the parts were electrically **disconnected** in the
simulator. Corrected against the official Wokwi part definitions:

| Part | Old (invalid) | New (valid) | Reference |
| --- | --- | --- | --- |
| `wokwi-pushbutton` (x6) | `1.A`, `2.A` | `1.l`, `2.l` | Wokwi pushbutton pins are `1.l` / `1.r` / `2.l` / `2.r` |
| OLED | `wokwi-ssd1306` (deprecated; pins `DATA`/`CLK`/`VIN`) | `board-ssd1306` (pins `SDA`/`SCL`/`VCC`/`GND`) | Wokwi `board-ssd1306` reference |
| `wokwi-buzzer` | `buzzer:1` -> GPIO25, `buzzer:2` -> GND | `buzzer:2` (positive) -> GPIO25, `buzzer:1` (negative) -> GND | Wokwi buzzer: pin 1 = negative, pin 2 = positive |

The ESP32 board pin names (`21`, `22`, `18`, `19`, `4`, `5`, `26`, `27`, `25`, `GND.1`, `GND.2`,
`GND.3`, `3V3`, `5V`) and the DS1307 pins (`GND`, `5V`, `SDA`, `SCL`) were already correct - they
were checked against `wokwi-boards/boards/esp32-devkit-c-v4/board.json` and the `wokwi-ds1307`
reference. All 22 connections were then validated programmatically against the official pin
sets (0 errors).

### 3.2 Button wiring and keyboard shortcuts

The buttons use the ESP32 internal pull-ups (`GPIO_PULLUP_ENABLE`) and are active-LOW, so
each button simply connects its GPIO to GND:

```
btn_x:1.l  ->  GPIO (18 / 19 / 4 / 5 / 26 / 27)
btn_x:2.l  ->  GND
```

Six buttons are wired: `MODE` (18), `SET` (19), `ALARM` (4), `LAP` (5), `UP` (26) and
`DOWN` (27). `UP`/`DOWN` were added so every adjustment has a **dedicated single-press**
button and no long-press/hold is required.

Keyboard shortcuts were added so the buttons can also be driven from the keyboard while the
simulation has focus: **M** = MODE, **S** = SET, **A** = ALARM, **L** = LAP, **U** = UP,
**D** = DOWN.

### 3.3 Firmware button handling (switches were previously inert)

`ButtonManager` already pushed debounced `ButtonEvent`s into a FreeRTOS queue, but **no task
consumed that queue**, so pressing a button had no visible effect even with correct wiring.
`main.cpp` now drains the queue in `vTaskButtonHandler` and maps presses onto a 5-view UI:

| Button | Short press | Long press |
| --- | --- | --- |
| MODE | cycle view: CLOCK -> DATE -> STOPWATCH -> ALARM -> TIME_SET | reset stopwatch |
| SET | STOPWATCH: start/pause; ALARM: toggle alarm on/off; TIME_SET: write edited time to RTC | CLOCK: toggle 12h/24h format |
| ALARM | ALARM/TIME_SET: **minute +1**; otherwise toggle alarm on/off | otherwise piezo buzzer self-test (2 kHz) |
| LAP | STOPWATCH: record lap; ALARM/TIME_SET: **minute -1** | - |
| UP | ALARM/TIME_SET: **hour +1**; CLOCK: force NTP resync | - |
| DOWN | ALARM/TIME_SET: **hour -1** | - |

In the editing views (ALARM / TIME_SET) every change is a **single short press**: 7:00 with
`DOWN` becomes 6:00, and 7:00 with `LAP` becomes 6:59. The OLED footer shows the hint
`HR:UP/DN MIN:A/L` while editing. On the CLOCK view the footer shows `CLOCK 24H   U=NTP` and the
armed alarm time is drawn under the date (dashboard parity).

The display task renders the active view and prints a view/hint line in the footer (`MODE:<name>`,
or the edit/NTP hints above), so button presses are immediately visible on the OLED.
`AlarmEngine` gained `getAlarmHour()` / `getAlarmMinute()` getters for this.

### 3.4 Inter-task signalling (previously unused APIs now wired in)

Three facilities existed in the components but were never called. They are now part of the
running behaviour and were verified by a clean rebuild:

- **`TimeManager::renderTime(display)`** - the CLOCK view in `vTaskClockDisplay` now delegates
  to this method instead of duplicating the layout in `main.cpp` (it also handles the RTC-error
  fallback).
- **`AlarmEngine::testBuzzer()`** - a **long press of ALARM** now fires the 2 kHz piezo self-test.
- **FreeRTOS event group** (`s_sysEvents`) - created in `app_main` and used to publish state
  between tasks, matching the Spiral 5 "Event Groups" primitive listed in the HTML design:
  - `SYS_WIFI_CONNECTED_BIT` / `SYS_NTP_SYNCED_BIT` are set by `vTaskNTPSync`
    (via `timeMgr.isWifiConnected()` / `isNtpSynced()`).
  - `SYS_ALARM_RINGING_BIT` is set/cleared by `vTaskAlarmCheck` from `alarmEngine.isRinging()`.
  - `vTaskClockDisplay` snapshots the bits to choose the header indicator (`NTP[OK]` vs `NTP--`)
    and the alarm `!` marker in the ALARM view.

### 3.5 Bidirectional adjustment & manual time-set (new)

Adjustment was forward-only. Every adjustable field now supports both directions, each with a
**dedicated single short-press button** (no long-press/hold required):

- **Alarm** (`AlarmEngine::adjustHour()` / `adjustMinute()`):
  `UP` = hour +1, `DOWN` = hour -1, `ALARM` = minute +1, `LAP` = minute -1.
  `adjustMinute()` carries/borrows across the hour, so `07:00` + `LAP` -> `06:59` and
  `07:00` + `DOWN` -> `06:00`. (The two extra switches `UP`/`DOWN` were added on GPIO 26/27.)
- **Manual time-set (new `TIME_SET` view)**: `MODE` now cycles
  `CLOCK -> DATE -> STOPWATCH -> ALARM -> TIME_SET`. On entry the edit buffer is seeded from the
  RTC; `UP`/`DOWN` adjust the hour and `ALARM`/`LAP` adjust the minute (both directions); `SET`
  writes the edited time back through `DS1307::setTime()` (seconds zeroed) and re-derives the
  POSIX system clock via `TimeManager::syncSystemFromRtc()`. This makes the previously under-used
  `DS1307::setTime()` reachable from the UI, completing the "manual time adjust" mode that both
  HTML designs only stubbed. The shared helper `adjustClockTime()` does the modular wrap/borrow.
- **Alarm dismissal**: a ringing alarm is stopped immediately by any button press (in addition to
  the engine's 30-second `RING_DURATION_MS` auto-stop).

### 3.6 Remaining functions completed (new)

- **12h / 24h display format**: `TimeManager` now holds `use24Hour_` with `set24Hour()` /
  `is24Hour()`. `renderTime()` renders `HH:MM:SS` (24h) or `hh:MM:SS AM/PM` (12h), and a
  **long press of `SET` on the CLOCK view** toggles the format. The clock-view footer shows the
  active format and the NTP shortcut (`CLOCK 24H   U=NTP` / `CLOCK 12H   U=NTP`).
- **Stopwatch lap split**: the STOPWATCH view now shows the most recent lap split
  (`LAP<n> MM:SS.CC`), exercising the previously unused `StopwatchEngine::getLap()`.
- **OLED flush optimisation**: `SSD1306::flush()` wrote the 1024-byte framebuffer with **1030
  separate single-byte I2C transactions** (each adding and removing the I2C device). It now issues
  **9 bulk transactions** - one `sendCommands()` window-set plus eight `sendBuffer()` page
  transfers - via the new `I2CBus::writeBytes()` bulk helper. `init()` likewise sends its command
  list with a single `sendCommands()` call.
- **Dead declarations removed**: `I2CBus::isReady()`, `DS1307::isRunning()`,
  `SSD1306::sendCommand()`, and the stray file-local `writeByte()` helper were unused and have
  been deleted. The previously declared-but-unimplemented `SSD1306::sendCommands()` /
  `sendBuffer()` are now real and used.
- **Manual NTP resync (HTML parity)**: `TimeManager::requestNtpSync()` calls `esp_sntp_restart()`
  and clears the synced flag. It is triggered by `UP` on the CLOCK view, matching the dashboard's
  `INC`-in-clock action and the suite's *Force NTP Sync* button.
- **Armed-alarm line on CLOCK (dashboard parity)**: the CLOCK view now draws `ALARM HH:MM` below
  the date whenever the alarm is enabled.
- **Test count**: the Unity suite was extended to **12** tests (added backward hour-wrap and
  minute-borrow assertions), matching the dashboard's advertised 12.

## 4. Wokwi Firmware Error - Root Cause and Fix

**Symptom reported by Wokwi:**

- `firmware binary build/esp32_digital_clock_modular_oop.bin not found in workspace`
- `firmware binary build/flasher_args.json not found`

**Root cause:** Both files are produced only by an ESP-IDF build. The `build/` folder did not exist yet. Wokwi never compiles the project - it only loads pre-built artifacts listed in `wokwi.toml`. In addition, on this machine the ESP-IDF toolchain itself was not usable from a plain shell:

| Problem | Cause | Fix applied |
| --- | --- | --- |
| `export.bat` fails: virtual environment not found | `C:\Users\NITRO\.espressif\python_env\idf6.1_py3.14_env` was empty | Used the existing, fully populated venv at `C:\Espressif\tools\python\v6.1\venv` and pointed `IDF_PYTHON_ENV_PATH` at it |
| `espidf.constraints.v6.1.txt doesn't exist` | File was in `C:\Espressif\tools\` but `idf_tools.py` looks in `IDF_TOOLS_PATH` root | Copied it to `C:\Espressif\` |
| `TypeError: expected string ... got 'NoneType'` (component manager) | `ESP_IDF_VERSION` not set (normally set by `export`) | Set `ESP_IDF_VERSION=6.1`, `IDF_VERSION=6.1.0` |
| `xtensa-esp32-elf-gcc is not a full path and was not found in PATH` | Toolchain bin dirs are nested one level deeper than expected and were not exported | Added the real bin dirs (`...\esp-15.2.0_20251204\xtensa-esp-elf\bin`, `...\riscv32-esp-elf\...\bin`, ninja, cmake, openocd, ccache, dfu-util) to `PATH` |
| `ESP_ROM_ELF_DIR environment variable is not defined` (warning) | Not exported | Set `ESP_ROM_ELF_DIR=C:\Espressif\tools\esp-rom-elfs\20241011` |

Build is now driven by a self-contained launcher script, kept with the project: `tools\build_windows.bat`. It sets `IDF_PATH`, `IDF_TOOLS_PATH`, `IDF_PYTHON_ENV_PATH`, `ESP_IDF_VERSION` and `PATH`, then runs `idf.py set-target esp32` followed by `idf.py build`. Run `tools\build_windows.bat` for an incremental build, or `tools\build_windows.bat clean` for a full rebuild.

## 5. Source Fixes Required for ESP-IDF v6.1

The project was originally written against older component names. ESP-IDF v6.1 renamed/removed several components, so these were corrected:

| File | Before | After | Reason |
| --- | --- | --- | --- |
| `components/time_manager/CMakeLists.txt` | `REQUIRES ... esp_sntp ...` | removed `esp_sntp` | The `esp_sntp` component was merged into `lwip` in v6.x; `esp_sntp.h` is still provided by `lwip` |
| `components/alarm_engine/CMakeLists.txt` | `REQUIRES ... ledc` | `REQUIRES ... esp_driver_gpio esp_driver_ledc` | `ledc` component was renamed; `driver/gpio.h` needs `esp_driver_gpio` |
| `components/i2c_bus/CMakeLists.txt` | `REQUIRES driver freertos` | `REQUIRES driver freertos esp_driver_i2c` | `driver/i2c_master.h` is provided by `esp_driver_i2c` |
| `components/button_manager/CMakeLists.txt` | `REQUIRES driver freertos` | `REQUIRES driver freertos esp_driver_gpio` | same reason as above |
| `components/i2c_bus/include/i2c_bus/I2CBus.hpp` | `dev_cfg.flags = 0;` | `dev_cfg.flags.disable_ack_check = 0;` | In v6.1 `i2c_device_config_t::flags` is a struct with bitfields, not an integer |
| `components/alarm_engine/include/alarm_engine/AlarmEngine.hpp` | `ledc_channel.intr_type = LEDC_INTR_DISABLE;` | removed | `ledc_channel_config_t::intr_type` is deprecated in v6.1 (defaults to disabled anyway) |
| `components/alarm_engine/include/alarm_engine/AlarmEngine.hpp` | - | added `getAlarmHour()` / `getAlarmMinute()` | lets `main.cpp` adjust the alarm from the buttons |

## 6. Build Results

Build completed successfully:

- `BUILD_OK` - "Project build complete"
- Firmware: `build/esp32_digital_clock_modular_oop.bin` (size `0xBB0F0` = 766,192 bytes, ~748 KB)
- ELF symbols: `build/esp32_digital_clock_modular_oop.elf`
- Flash metadata: `build/flasher_args.json`
- App partition: 1 MB at `0x10000`, **27% free** (0x44F10 bytes)
- Flash settings: DIO mode, 40 MHz, 2 MB
- No compiler warnings or errors

This resolves the two Wokwi errors, because `wokwi.toml` now points at files that really exist.

## 7. Simulation Readiness Checklist

- [x] ESP-IDF v6.1 project structure
- [x] C++ OOP modular design (HAL → Drivers → Managers/Engines → Tasks)
- [x] FreeRTOS tasks: Button (P5 Core0), NTP (P1 Core0), Alarm (P4 Core1), Display (P3 Core1)
- [x] Thread-safe I2C via mutex
- [x] DS1307 BCD helpers + init with CH clear
- [x] SSD1306 5x7 font + header + time rendering
- [x] Button debouncing with short/long press detection
- [x] SNTP + RTC fallback
- [x] LEDC buzzer PWM
- [x] High-res stopwatch (esp_timer)
- [x] Unity scaffold tests (12 tests, Spirals 1-5, incl. backward-wrap / borrow math)
- [x] Wokwi files present
- [x] Wokwi wiring uses valid pin names for every part (22/22 connections validated)
- [x] Button events consumed by firmware: MODE/SET/ALARM/LAP/UP/DOWN drive a 5-view UI
- [x] Dedicated single-press adjust buttons (UP/DOWN = hour +/-; ALARM/LAP = minute +/-)
- [x] Inter-task event group (WiFi/NTP/alarm-ringing bits) wired into the renderer
- [x] `TimeManager::renderTime()` and `AlarmEngine::testBuzzer()` reachable from the UI
- [x] Bidirectional adjust via single presses (UP/DOWN = hour, ALARM/LAP = minute) for alarm and manual time-set
- [x] Manual `TIME_SET` view writes to the RTC via `DS1307::setTime()`
- [x] STOPWATCH view shows the latest lap split (exercises `StopwatchEngine::getLap()`)
- [x] A ringing alarm is dismissible with any button (auto-stop after 30 s retained)
- [x] 12h/24h display toggle (`SET` long on CLOCK) with footer format indicator
- [x] `SSD1306` framebuffer streamed in bulk I2C transfers (9 vs 1030 transactions)
- [x] Unused declarations removed (`I2CBus::isReady`, `DS1307::isRunning`, `SSD1306::sendCommand`)
- [x] Manual NTP resync from `UP` on the CLOCK view (HTML "Force NTP Sync" parity)
- [x] Armed-alarm time shown on the CLOCK view (dashboard parity)
- [x] Thai `README.md` (what was done, test results, problems and fixes)

## 8. How to Run the Simulation

1. Open the project in VS Code with the ESP-IDF Extension (v6.1 toolchain)
2. Build is **already done** - the firmware exists. To rebuild, run `tools\build_windows.bat` (or `idf.py build` from a working ESP-IDF terminal)
3. Simulate: click the Wokwi extension "Start Simulation", or run `idf.py wokwi-start`
4. Monitor: Wokwi's built-in serial terminal (or `idf.py monitor` on real hardware)
5. Tests: `cd test/test_spiral && idf.py build flash monitor` (Unity test app)

### Expected serial output

```
I (xxx) MAIN_APP: Initializing Modular C++ OOP Digital Clock System [ESP-IDF v6.1]
I (xxx) MAIN_APP: I2C Bus initialized successfully (SDA:21, SCL:22)
I (xxx) MAIN_APP: DS1307 RTC initialized successfully (0x68)
I (xxx) MAIN_APP: SSD1306 OLED initialized successfully (128x64, 0x3C)
I (xxx) MAIN_APP: vTaskButtonHandler started [Priority:5, Core:0]
I (xxx) MAIN_APP: vTaskNTPSync started [Priority:1, Core:0]
I (xxx) MAIN_APP: vTaskAlarmCheck started [Priority:4, Core:1]
I (xxx) MAIN_APP: vTaskClockDisplay started [Priority:3, Core:1]
I (xxx) MAIN_APP: All FreeRTOS tasks created. System running...
I (xxx) MAIN_APP: [BTN] MODE short -> view: STOPWATCH
I (xxx) MAIN_APP: [BTN] SET  -> stopwatch started
I (xxx) MAIN_APP: [BTN] LAP  -> lap 1
```

The SSD1306 panel shows the active view, read from the emulated DS1307 once per 100 ms.
Press **M** on the keyboard (or click the MODE button) to cycle
`CLOCK -> DATE -> STOPWATCH -> ALARM -> TIME_SET`; the footer shows the active view.

In the **ALARM** / **TIME_SET** views (footer hint `HR:UP/DN MIN:A/L`) every change is a single
short press - keyboard **U**/**D** = hour +/- and **A**/**L** = minute +/-:

```
default 07:00  +  D  ->  06:00     (hour -1)
default 07:00  +  L  ->  06:59     (minute -1, borrows the hour)
default 07:00  +  A  ->  07:01     (minute +1)
default 07:00  +  U  ->  08:00     (hour +1)
```

On the **CLOCK** view the footer reads `CLOCK 24H   U=NTP`; pressing **U** there forces an
immediate SNTP resynchronisation, and the armed alarm time is printed under the date.

## 9. Known Notes

- Path must not contain Thai characters/spaces (Project at `C:\microcontrollerPJ\FinalAssignment10\`)
- `sdkconfig.defaults` sets defaults; `menuconfig` may override
- SSD1306 flush streams the framebuffer in 8 bulk I2C page transfers (plus one window-set command), instead of the original 1030 single-byte writes
- Wi-Fi is configured for `iotexpo/12345678` (as in the HTML design). In Wokwi the virtual AP is `Wokwi-GUEST`, so the connection attempt will fail and NTP will not sync. This is harmless: `TimeManager` falls back to `syncSystemFromRtc()`, so the OLED still displays RTC time. To get NTP sync inside Wokwi, change `WIFI_SSID` to `Wokwi-GUEST` with an empty password.
- Firmware is built for 2 MB flash while Wokwi's DevKit-C V4 model has 4 MB. This is compatible because the app sits at `0x10000` inside the 1 MB partition. To match exactly, add `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y` to `sdkconfig.defaults` and rebuild from a clean `sdkconfig`.
- `test/test_spiral` is not part of the main build (it is not in `EXTRA_COMPONENT_DIRS`), so it is built separately as a standalone Unity app via `tools\build_tests_windows.bat` (project in `test_app/`, 12 tests link cleanly: `TEST_BUILD_OK`).

## 10. Validation Against HTML Requirements

- Hardware pins match dashboard/suite: I2C 21/22, buttons 18/19/4/5 plus adjust buttons 26/27, buzzer 25 ✓
- Wi-Fi credentials `iotexpo/12345678` ✓
- Spiral tasks/priorities align with design ✓
- Modular components mirror class names ✓
- Wokwi diagram wiring validated: 22/22 connections use valid pin names ✓
- Push buttons are wired, pull-up/active-LOW, and now consumed by the firmware ✓
- Build succeeds against ESP-IDF v6.1 with no warnings ✓
- HTML parity: manual NTP resync (`UP` on CLOCK), armed-alarm line on CLOCK, 12 Unity tests, and a Thai `README.md` ✓

**Status:** Project builds successfully. All Wokwi pin-name errors are fixed (buttons, OLED, buzzer), the push buttons are functional in firmware, and the firmware artifacts (`build/esp32_digital_clock_modular_oop.bin`, `.elf`, `build/flasher_args.json`) are present. Rebuild any time with `tools\build_windows.bat`.