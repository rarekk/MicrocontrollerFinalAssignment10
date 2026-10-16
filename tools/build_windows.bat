@echo off
setlocal
rem ==================================================================
rem ESP-IDF v6.1 build launcher for this project (Windows)
rem
rem Why this file exists:
rem   On this machine the stock `install.bat` / `export.bat` scripts
rem   fail, because the default python virtual environment
rem   (%USERPROFILE%\.espressif\python_env\idf6.1_py3.14_env) was never
rem   created. A complete, working ESP-IDF v6.1 toolchain and python
rem   venv already exist under C:\Espressif, so this script wires that
rem   environment up directly and then calls idf.py.
rem
rem Usage:
rem   tools\build_windows.bat          incremental build
rem   tools\build_windows.bat clean    delete build\ then full rebuild
rem ==================================================================

set "PROJECT_DIR=%~dp0.."

rem --- ESP-IDF and tools locations ----------------------------------
set "IDF_PATH=C:\esp\v6.1\esp-idf"
set "IDF_TOOLS_PATH=C:\Espressif"
set "IDF_PYTHON_ENV_PATH=C:\Espressif\tools\python\v6.1\venv"
set "IDF_PYTHON=C:\Espressif\tools\python\v6.1\venv\Scripts\python.exe"

rem --- Values normally exported by export.bat ----------------------
set "ESP_IDF_VERSION=6.1"
set "IDF_VERSION=6.1.0"
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"
set "OPENOCD_SCRIPTS=C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20260703\openocd-esp32\share\openocd\scripts"
set "ESP_ROM_ELF_DIR=C:\Espressif\tools\esp-rom-elfs\20241011"

rem --- Toolchain / utility bin directories -------------------------
set "EXTRA_PATH=C:\Espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin"
set "EXTRA_PATH=%EXTRA_PATH%;C:\Espressif\tools\riscv32-esp-elf\esp-15.2.0_20251204\riscv32-esp-elf\bin"
set "EXTRA_PATH=%EXTRA_PATH%;C:\Espressif\tools\esp32ulp-elf\2.38_20240113\esp32ulp-elf\bin"
set "EXTRA_PATH=%EXTRA_PATH%;C:\Espressif\tools\cmake\4.0.3\bin"
set "EXTRA_PATH=%EXTRA_PATH%;C:\Espressif\tools\ninja\1.12.1"
set "EXTRA_PATH=%EXTRA_PATH%;C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20260703\openocd-esp32\bin"
set "EXTRA_PATH=%EXTRA_PATH%;C:\Espressif\tools\ccache\4.12.1\ccache-4.12.1-windows-x86_64"
set "EXTRA_PATH=%EXTRA_PATH%;C:\Espressif\tools\dfu-util\0.11\dfu-util-0.11-win64"
set "EXTRA_PATH=%EXTRA_PATH%;C:\Espressif\tools\idf-exe\1.0.3"
set "EXTRA_PATH=%EXTRA_PATH%;%IDF_PATH%\tools"
set "PATH=%EXTRA_PATH%;%PATH%"

cd /d "%PROJECT_DIR%"

if "%1"=="clean" (
    echo === Cleaning build directory ===
    rmdir /s /q "%PROJECT_DIR%\build"
)

if not exist "%PROJECT_DIR%\build\build.ninja" (
    echo === STEP 1: set-target esp32 ===
    "%IDF_PYTHON%" -X utf8 "%IDF_PATH%\tools\idf.py" set-target esp32
    if errorlevel 1 (
        echo SET_TARGET_FAILED
        exit /b 1
    )
    echo SET_TARGET_OK
)

echo === STEP 2: build ===
"%IDF_PYTHON%" -X utf8 "%IDF_PATH%\tools\idf.py" build
if errorlevel 1 (
    echo BUILD_FAILED
    exit /b 1
)

echo BUILD_OK
echo Firmware : %PROJECT_DIR%\build\esp32_digital_clock_modular_oop.bin
echo ELF      : %PROJECT_DIR%\build\esp32_digital_clock_modular_oop.elf
