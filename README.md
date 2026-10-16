# ESP32 Digital Clock (Modular OOP)

นาฬิกาดิจิทัลบน **ESP32 DevKit C V4** ด้วย **ESP-IDF v6.1 + FreeRTOS** เขียนแบบ **OOP แยกโมดูล**
รันจำลองบน **Wokwi** ครบทุกฟังก์ชัน (ตั้งเวลา, ตั้งปลุก, จับเวลา, ตั้งนาฬิกาเอง)

## ทำอะไรไปบ้าง
- แยก 7 component: `i2c_bus`, `ds1307`, `ssd1306`, `button_manager`, `time_manager`, `alarm_engine`, `stopwatch_engine`
- ใช้ 4 FreeRTOS task (Button P5, NTP P1, Alarm P4, Display P3) + I2C mutex กันข้อมูลชนกัน
- UI 5 หน้า: **CLOCK / DATE / STOPWATCH / ALARM / TIME_SET**
- **ปรับค่าได้ทั้ง 2 ทิศทาง** (เพิ่ม/ลด) ด้วยปุ่มกดครั้งเดียว ไม่ต้องกดค้าง
- ตั้งเวลาเอง (TIME_SET) เขียนลง RTC จริงผ่าน `DS1307::setTime()`
- สลับรูปแบบ 12/24 ชม., ปิดเสียงปลุกด้วยปุ่มใดก็ได้, โชว์ lap split
- ปุ่ม `UP` บนหน้า CLOCK = สั่ง sync NTP ใหม่ทันที

## โครงสร้าง
```
components/   7 โมดูล (header อยู่ที่ include/<ชื่อ>/)
main/main.cpp ต่อทุกอย่างเข้าด้วยกัน (UI + ปุ่ม + task)
diagram.json  สายไฟ Wokwi (22 connections)
test/         Unity unit tests (12 เทสต์)
test_app/     โปรเจกต์แยกสำหรับ build/รันเทสต์
tools/        สคริปต์ build (firmware + tests) สำหรับ Windows
docs/REPORT.md รายงานฉบับเต็ม
```

## การ Build และรัน
1. เปิดโปรเจกต์ด้วย VS Code + ESP-IDF Extension (v6.1)
2. Build: รัน `tools\build_windows.bat` (หรือ `idf.py build`)
3. รันจำลอง: กด **Start Simulation** ของ Wokwi (ใช้ไฟล์ `build/esp32_digital_clock_modular_oop.bin`)
4. ไฟล์ที่ได้: `build/esp32_digital_clock_modular_oop.bin` และ `.elf`
5. (ทางเลือก) build unit tests: รัน `tools\build_tests_windows.bat`

## ปุ่มควบคุม (กดครั้งเดียว)
| ปุ่ม | GPIO | หน้า CLOCK | หน้า ALARM / TIME_SET |
| --- | --- | --- | --- |
| MODE | 18 | เปลี่ยนหน้า (กดค้าง = reset stopwatch) | เปลี่ยนหน้า |
| SET | 19 | (กดค้าง = สลับ 12/24 ชม.) | ALARM = เปิด/ปิดปลุก ; TIME_SET = เขียนลง RTC |
| ALARM | 4 | เปิด/ปิดปลุก | **นาที +1** |
| LAP | 5 | — | **นาที −1** |
| UP | 26 | **สั่ง sync NTP** | **ชั่วโมง +1** |
| DOWN | 27 | — | **ชั่วโมง −1** |

คีย์บอร์ดใน Wokwi: **M / S / A / L / U / D** ตรงกับปุ่มด้านบน

ตัวอย่าง: ค่าเริ่มต้นปลุก 07:00 → กด **D** = 06:00, กด **L** = 06:59 (ยืมชั่วโมง)

## ผลการทดสอบ
- **Build:** ผ่าน (`BUILD_OK`) ไม่มี warning, ขนาด firmware `0xBB0F0` (~748 KB) เหลือพื้นที่ 27%
- **Unit tests (Unity):** 12 เทสต์ build/link ผ่าน (`TEST_BUILD_OK` ด้วย `tools\build_tests_windows.bat`)
  ครอบคลุม BCD conversion, framebuffer bounds, ปุ่ม/คิว, จับเวลา/lap, การลดค่าแบบ wrap
  (00:00 −1ชม. = 23:00) และการยืมชั่วโมง (07:00 −1นาที = 06:59) — assertion เป็นคณิตศาสตร์ผลแน่นอน
- **Wokwi:** OLED, DS1307, ปุ่มทั้ง 6, buzzer ทำงานถูกต้อง

## ปัญหาที่เจอ และวิธีแก้
| ปัญหา | วิธีแก้ |
| --- | --- |
| ชื่อขาใน `diagram.json` ผิด (เช่น `1.A`, `wokwi-ssd1306`) ทำให้ต่อสายไม่ได้ | เปลี่ยนเป็น `1.l`/`2.l` และใช้ `board-ssd1306` แล้วตรวจชื่อขาทั้ง 22 เส้น |
| ปลุกตั้งย้อนหลังไม่ได้ (เดิมบวกอย่างเดียว) และการกดค้างใน Wokwi ไม่นิ่ง | เพิ่มปุ่ม `UP`/`DOWN` (GPIO 26/27) ให้ปรับลดได้ด้วยการกดครั้งเดียว |
| เวลาที่ตั้งเองไม่ถูกบันทึก | ทำหน้า `TIME_SET` เขียนลง RTC จริง + sync system clock |
| OLED อัปเดตช้า (เขียนทีละไบต์ 1030 ครั้ง) | ใช้ I2C ส่งทีละ page (9 ครั้ง) เร็วขึ้นมาก |
| API บางตัวเปลี่ยนใน IDF v6.1 (เช่น `esp_sntp` ย้ายเข้า lwip, `ledc`, `i2c`) | แก้ `CMakeLists`/โค้ดตาม API ใหม่ จน build ผ่าน |
| Wokwi ต่อ Wi-Fi `iotexpo` ไม่ได้ (AP จริงคือ `Wokwi-GUEST`) | ระบบ fallback ไปใช้เวลาจาก RTC แทน แสดง `NTP--` (ไม่กระทบการทำงาน) |

> หมายเหตุ: ถ้าต้องการ sync NTP จริงใน Wokwi ให้เปลี่ยน SSID เป็น `Wokwi-GUEST` (รหัสว่าง)
