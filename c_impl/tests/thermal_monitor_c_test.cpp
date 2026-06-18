#include <gtest/gtest.h>

extern "C" {
#include "../thermal_monitor.h"
}

#include "mock_linux_os_c_adapter.hpp"
#include "platform/soc/mock_soc.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

static constexpr unsigned int TEST_POLL_MS = 10;
static constexpr auto         SETTLE       = std::chrono::milliseconds{25};

struct ThermalMonitorCTest : ::testing::Test {
    MockSoC soc;
    CMockLinuxOsAdapter osAdapter;
    ThermalMonitor monitor;

    void SetUp() override {
        c_mock_linux_os_adapter_init(&osAdapter, soc.os);
        thermal_monitor_init(&monitor,
                             reinterpret_cast<CLinuxLikeOs*>(&osAdapter),
                             TEST_POLL_MS);
    }

    void TearDown() override {
        thermal_monitor_stop(&monitor);
    }

    bool checkLEDs(bool g, bool y, bool r) {
        return soc.gpioGreen->getValue()  == (g ? 1 : 0) &&
               soc.gpioYellow->getValue() == (y ? 1 : 0) &&
               soc.gpioRed->getValue()    == (r ? 1 : 0);
    }
};

TEST_F(ThermalMonitorCTest, TypeAThresholds) {
    soc.eepromFile->setByte(0, static_cast<uint8_t>(SENSOR_TYPE_A));

    soc.tempSensor->setRawValue(84);
    ASSERT_TRUE(thermal_monitor_start(&monitor, nullptr));
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "84 degC -> Green";

    soc.tempSensor->setRawValue(85);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "85 degC -> Yellow";

    soc.tempSensor->setRawValue(105);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, false, true)) << "105 degC -> Red";

    soc.tempSensor->setRawValue(5);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "5 degC (CRIT_LO boundary) -> Green";

    soc.tempSensor->setRawValue(4);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, false, true)) << "4 degC -> Red (cold)";

    soc.tempSensor->setRawValue(104);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "104 degC (below CRIT_HI) -> Yellow";
}

TEST_F(ThermalMonitorCTest, TypeBThresholds) {
    soc.eepromFile->setByte(0, static_cast<uint8_t>(SENSOR_TYPE_B));

    soc.tempSensor->setRawValue(849);
    ASSERT_TRUE(thermal_monitor_start(&monitor, nullptr));
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "84.9 degC -> Green";

    soc.tempSensor->setRawValue(850);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "85.0 degC -> Yellow";

    soc.tempSensor->setRawValue(1050);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, false, true)) << "105.0 degC -> Red (critical high)";

    soc.tempSensor->setRawValue(50);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "5.0 degC (CRIT_LO boundary) -> Green";

    soc.tempSensor->setRawValue(49);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, false, true)) << "4.9 degC -> Red (critical low)";

    soc.tempSensor->setRawValue(1049);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "104.9 degC (below CRIT_HI) -> Yellow";
}

TEST_F(ThermalMonitorCTest, SensorI2cError) {
    soc.eepromFile->setByte(0, static_cast<uint8_t>(SENSOR_TYPE_A));
    soc.tempSensor->setRawValue(80);
    ASSERT_TRUE(thermal_monitor_start(&monitor, nullptr));
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "80 degC -> Green";

    soc.tempSensor->injectError(true);
    bool sawOn = false, sawOff = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{150};
    while (std::chrono::steady_clock::now() < deadline) {
        if (soc.gpioRed->getValue() == 1) sawOn  = true;
        if (soc.gpioRed->getValue() == 0) sawOff = true;
        if (sawOn && sawOff) break;
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    EXPECT_EQ(soc.gpioGreen->getValue(),  0);
    EXPECT_EQ(soc.gpioYellow->getValue(), 0);
    EXPECT_TRUE(sawOn && sawOff) << "Red must blink on I2C error";

    soc.tempSensor->injectError(false);
    soc.tempSensor->setRawValue(80);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "Recovery -> Green";
}

TEST(ThermalMonitorCEepromTest, EepromMissingAtStart) {
    MockLinuxOs os;
    MockI2cBus bus;
    auto sensor = std::make_shared<MockTempSensorPeripheral>();
    sensor->setRawValue(25);
    bus.attach(0x48, sensor);
    os.registerPath("/dev/i2c-1", std::make_shared<MockI2cDevFile>(bus));

    auto gpioGreen  = std::make_shared<MockGpioValueFile>();
    auto gpioYellow = std::make_shared<MockGpioValueFile>();
    auto gpioRed    = std::make_shared<MockGpioValueFile>();
    os.registerPath(TM_GPIO_GREEN, gpioGreen);
    os.registerPath(TM_GPIO_YELLOW, gpioYellow);
    os.registerPath(TM_GPIO_RED, gpioRed);
    os.registerPath(TM_GPIO_EXPORT, std::make_shared<MockSinkFile>());
    os.registerPath(TM_GPIO_GREEN_DIR, std::make_shared<MockSinkFile>());
    os.registerPath(TM_GPIO_YELLOW_DIR, std::make_shared<MockSinkFile>());
    os.registerPath(TM_GPIO_RED_DIR, std::make_shared<MockSinkFile>());

    CMockLinuxOsAdapter osAdapter;
    c_mock_linux_os_adapter_init(&osAdapter, os);

    ThermalMonitor monitor;
    thermal_monitor_init(&monitor,
                         reinterpret_cast<CLinuxLikeOs*>(&osAdapter),
                         TEST_POLL_MS);
    ASSERT_TRUE(thermal_monitor_start(&monitor, nullptr));

    bool sawOn = false, sawOff = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{150};
    while (std::chrono::steady_clock::now() < deadline) {
        if (gpioRed->getValue() == 1) sawOn  = true;
        if (gpioRed->getValue() == 0) sawOff = true;
        if (sawOn && sawOff) break;
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    EXPECT_EQ(gpioGreen->getValue(),  0);
    EXPECT_EQ(gpioYellow->getValue(), 0);
    EXPECT_TRUE(sawOn && sawOff) << "Red must blink on EEPROM error";

    thermal_monitor_stop(&monitor);
}

TEST_F(ThermalMonitorCTest, SensorTypeReadOnceAtStart) {
    soc.eepromFile->setByte(0, static_cast<uint8_t>(SENSOR_TYPE_B));
    soc.tempSensor->setRawValue(900);
    ASSERT_TRUE(thermal_monitor_start(&monitor, nullptr));
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "TypeB raw 900 -> Yellow";

    soc.eepromFile->setByte(0, static_cast<uint8_t>(SENSOR_TYPE_A));
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "Config change after start ignored";
}

TEST_F(ThermalMonitorCTest, TemperatureCallback) {
    soc.eepromFile->setByte(0, static_cast<uint8_t>(SENSOR_TYPE_A));
    soc.tempSensor->setRawValue(25);

    static std::atomic<int16_t>* g_captured;
    std::atomic<int16_t> captured{-999};
    g_captured = &captured;

    ASSERT_TRUE(thermal_monitor_start(&monitor, [](int16_t t) {
        g_captured->store(t);
    }));
    std::this_thread::sleep_for(SETTLE);
    EXPECT_EQ(captured.load(), 250) << "25 degC TypeA -> 250 deci-degC";
}

TEST_F(ThermalMonitorCTest, MedianFilterSuppressesOutlier) {
    soc.eepromFile->setByte(0, static_cast<uint8_t>(SENSOR_TYPE_A));
    thermal_monitor_set_stability_config(&monitor, ThermalStabilityConfig{
        3,
        1,
        0
    });

    soc.tempSensor->setRawSequence({84, 130, 84});
    ASSERT_TRUE(thermal_monitor_start(&monitor, nullptr));
    std::this_thread::sleep_for(std::chrono::milliseconds{40});

    EXPECT_TRUE(checkLEDs(true, false, false)) << "median(84, 130, 84) degC -> Green";
}

TEST_F(ThermalMonitorCTest, HysteresisUsesConvertedTypeBTemperature) {
    soc.eepromFile->setByte(0, static_cast<uint8_t>(SENSOR_TYPE_B));
    thermal_monitor_set_stability_config(&monitor, ThermalStabilityConfig{
        1,
        0,
        20
    });

    soc.tempSensor->setRawValue(850);
    ASSERT_TRUE(thermal_monitor_start(&monitor, nullptr));
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "85.0 degC enters Yellow";

    soc.tempSensor->setRawValue(840);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "84.0 degC remains Yellow";

    soc.tempSensor->setRawValue(829);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "82.9 degC exits Yellow";

    soc.tempSensor->setRawValue(1050);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, false, true)) << "105.0 degC enters Red high";

    soc.tempSensor->setRawValue(1040);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, false, true)) << "104.0 degC remains Red high";

    soc.tempSensor->setRawValue(1029);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "102.9 degC exits Red high to Yellow";

    soc.tempSensor->setRawValue(49);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, false, true)) << "4.9 degC enters Red low";

    soc.tempSensor->setRawValue(60);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, false, true)) << "6.0 degC remains Red low";

    soc.tempSensor->setRawValue(70);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "7.0 degC exits Red low";
}

TEST_F(ThermalMonitorCTest, StartStop) {
    EXPECT_FALSE(thermal_monitor_running(&monitor));
    ASSERT_TRUE(thermal_monitor_start(&monitor, nullptr));
    EXPECT_TRUE(thermal_monitor_running(&monitor));
    thermal_monitor_stop(&monitor);
    EXPECT_FALSE(thermal_monitor_running(&monitor));
}
