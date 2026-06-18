cmake_minimum_required(VERSION 3.16)

# Qt AUTOMOC is set globally in the root; disable it for all C targets here
set(CMAKE_AUTOMOC OFF)

# ── Core C library ────────────────────────────────────────────────────────────
add_library(ThermalMonitorC STATIC
    thermal_monitor.h  thermal_monitor.c
    interfaces.h
)
target_include_directories(ThermalMonitorC PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ThermalMonitorC PUBLIC pthread)

# ── Linux hardware adapters ───────────────────────────────────────────────────
add_library(ThermalMonitorCLinux STATIC
    linux_adapters.h   linux_adapters.c
)
target_include_directories(ThermalMonitorCLinux PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ThermalMonitorCLinux PUBLIC ThermalMonitorC)

add_executable(thermal_monitor_c_demo_real
    demo_real_hw_c.c
)
target_link_libraries(thermal_monitor_c_demo_real PRIVATE
    ThermalMonitorC
    ThermalMonitorCLinux
)

# ── GTest suite (C++ test file linking against the C library) ─────────────────
if(BUILD_TESTS)
    find_package(GTest REQUIRED)

    add_executable(thermal_monitor_c_tests
        tests/thermal_monitor_c_test.cpp
    )
    target_include_directories(thermal_monitor_c_tests PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(thermal_monitor_c_tests PRIVATE
        ThermalMonitorC
        GTest::gtest
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(thermal_monitor_c_tests)
endif()
