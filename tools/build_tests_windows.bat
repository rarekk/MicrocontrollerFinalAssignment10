@echo off
setlocal
rem ==================================================================
rem ESP-IDF v6.1 Unity test-app build launcher (Windows)
rem
rem Builds the Spiral unit tests (test/test_spiral) as a standalone app
rem so they are actually compiled / linkable. Uses the same toolchain
rem environment as tools\build_windows.bat.
rem
rem Usage:  tools\build_tests_windows.bat
rem ==================================================================

set "PROJECT_DIR=%~dp0.."

set "IDF_PATH=C:\esp\v6.1\esp-idf"
set "IDF_TOOLS_PATH=C:\Espressif"
set "IDF_PYTHON_ENV_PATH=C:\Espressif\tools\python\v6.1\venv"
set "IDF_PYTHON=C:\Espressif\tools\python\v6.1\venv\Scripts\python.exe"
set "ESP_IDF_VERSION=6.1"
set "IDF_VERSION=6.1.0"
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"
set "OPENOCD_SCRIPTS=C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20260703\openocd-esp32\share\openocd\scripts"
set "ESP_ROM_ELF_DIR=C:\Espressif\tools\esp-rom-elfs\20241011"

set "EXTRA_PATH=C:\Espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin"
set "EXTRA_PATH=%EXTRA_PATH%;C:\Espressif\tools\riscv32-esp-elf\esp-15.2.0_20251204\riscv32-esp-elf\bin"
set "EXTRA_PATH=%EXTRA_PATH%;C:\Espressif\tools\esp32ulp-elf\2.38_20240113\esp32ulp-elf\bin"
set "EXTRA_PATH=%EXTRA_PATH%;C:\Espressif\tools\cmake\4.0.3\bin"
set "EXTRA_PATH=%EXTRA_PATH%;C:\Espressif\tools\ninja\1.12.1"
set "EXTRA_PATH=%EXTRA_PATH%;C:\Espressif\tools\ccache\4.12.1\ccache-4.12.1-windows-x86_64"
set "EXTRA_PATH=%EXTRA_PATH%;%IDF_PATH%\tools"
set "PATH=%EXTRA_PATH%;%PATH%"

cd /d "%PROJECT_DIR%\test_app"

if not exist "build\build.ninja" (
    echo === STEP 1: set-target esp32 ===
    "%IDF_PYTHON%" -X utf8 "%IDF_PATH%\tools\idf.py" set-target esp32
    if errorlevel 1 ( echo SET_TARGET_FAILED & exit /b 1 )
)

echo === STEP 2: build tests ===
"%IDF_PYTHON%" -X utf8 "%IDF_PATH%\tools\idf.py" build
if errorlevel 1 ( echo TEST_BUILD_FAILED & exit /b 1 )

echo TEST_BUILD_OK
echo Test app: %PROJECT_DIR%\test_app\build\clock_unit_tests.bin
