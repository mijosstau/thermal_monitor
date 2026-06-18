cmake_minimum_required(VERSION 3.16)
project(ThermalSimulator VERSION 1.0 LANGUAGES C CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ============================================================================
# Core Library — pure C++, no Qt dependencies (usable on embedded targets)
# ============================================================================
add_library(ThermalMonitorCore STATIC
    thermal_monitor.h  thermal_monitor.cpp
    interfaces.h
)
target_include_directories(ThermalMonitorCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ============================================================================
# GUI Application — Qt6 wrapper around core library
# ============================================================================
find_package(Qt6 QUIET COMPONENTS Core Widgets)
if(NOT Qt6_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS Core Widgets)
    set(QT_MAJOR 5)
else()
    set(QT_MAJOR 6)
endif()

add_executable(ThermalSimulator
    main.cpp
    mainwindow.h mainwindow.cpp
    c_impl/qt_adapter.hpp c_impl/qt_adapter.cpp
)
target_link_libraries(ThermalSimulator PRIVATE
    ThermalMonitorCore
    ThermalMonitorC
    Qt${QT_MAJOR}::Core
    Qt${QT_MAJOR}::Widgets
)

# ============================================================================
# Tests — Google Test (no Qt dependencies)
# ============================================================================
option(BUILD_TESTS "Build unit tests with Google Test" ON)
if(BUILD_TESTS)
    find_package(GTest REQUIRED)

    add_executable(thermal_monitor_tests
        tests/thermal_monitor_test.cpp
    )
    target_link_libraries(thermal_monitor_tests PRIVATE
        ThermalMonitorCore
        GTest::gtest
        GTest::gtest_main
        pthread
    )

    include(GoogleTest)
    gtest_discover_tests(thermal_monitor_tests)
endif()

# ============================================================================
# Real-hardware demo — uses RealLinuxOs with i2c-stub kernel module
# ============================================================================
add_executable(demo_real_hw demo_real_hw.cpp)
target_link_libraries(demo_real_hw PRIVATE ThermalMonitorCore pthread)

# ============================================================================
# C Implementation (Spec requirement: C version with mocks)
# ============================================================================
add_subdirectory(c_impl)
