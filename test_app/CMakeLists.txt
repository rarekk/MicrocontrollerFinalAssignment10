cmake_minimum_required(VERSION 3.16)

# Standalone Unity test-app project. It reuses the firmware's components and the
# test_spiral test component (which provides app_main). Build with:
#     tools\build_tests_windows.bat
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../components"
    "${CMAKE_CURRENT_LIST_DIR}/../test/test_spiral")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(clock_unit_tests)
