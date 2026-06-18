#include <gtest/gtest.h>
#include "thermal_monitor.h"
#include "platform/soc/mock_soc.hpp"
#include "platform/mock_linux_os.hpp"
#include "platform/i2c/mock_i2c_bus.hpp"
#include "platform/i2c/mock_i2c_dev_file.hpp"
#include "platform/gpio/mock_gpio_value_file.hpp"

#include <thread>
#include <chrono>

static constexpr auto TEST_POLL = std::chrono::milliseconds{10};
static constexpr auto SETTLE    = std::chrono::milliseconds{25};

class ThermalMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        monitor = std::make_unique<ThermalMonitor>(soc.os, TEST_POLL);
    }

    void TearDown() override {
        monitor->stop();
    }

    bool checkLEDs(bool g, bool y, bool r) {
        return soc.gpioGreen->getValue()  == (g ? 1 : 0) &&
               soc.gpioYellow->getValue() == (y ? 1 : 0) &&
               soc.gpioRed->getValue()    == (r ? 1 : 0);
    }

    MockSoC soc;
    std::unique_ptr<ThermalMonitor> monitor;
};

TEST_F(ThermalMonitorTest, TypeAThresholds) {
    soc.eepromFile->setByte(0, static_cast<uint8_t>(SensorType::TypeA_1_Per_Unit));

    soc.tempSensor->setRawValue(84);
    ASSERT_TRUE(monitor->start());
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "84 degC should be Green";

    soc.tempSensor->setRawValue(85);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "85 degC should be Yellow";

    soc.tempSensor->setRawValue(105);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, false, true)) << "105 degC should be Red";

    // Boundary at CRIT_LO: exactly 5 degC (50 deci-degC) -> Green, not Red
    soc.tempSensor->setRawValue(5);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "5 degC (CRIT_LO boundary) should be Green";

    // Below CRIT_LO: 4 degC -> Red
    soc.tempSensor->setRawValue(4);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, false, true)) << "4 degC should be Red";

    // Just below CRIT_HI: 104 degC (1040 deci-degC) -> Yellow, not Red
    soc.tempSensor->setRawValue(104);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "104 degC (below CRIT_HI) should be Yellow";
}

TEST_F(ThermalMonitorTest, TypeBThresholds) {
    soc.eepromFile->setByte(0, static_cast<uint8_t>(SensorType::TypeB_0_1_Per_Unit));

    soc.tempSensor->setRawValue(849);
    ASSERT_TRUE(monitor->start());
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "84.9 degC should be Green";

    soc.tempSensor->setRawValue(850);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "85.0 degC should be Yellow";

    soc.tempSensor->setRawValue(1050);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, false, true)) << "105.0 degC should be Red (critical high)";

    // Boundary at CRIT_LO: exactly 5.0 degC (50 deci-degC) -> Green, not Red
    soc.tempSensor->setRawValue(50);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "5.0 degC (CRIT_LO boundary) should be Green";

    // Below CRIT_LO: 4.9 degC -> Red
    soc.tempSensor->setRawValue(49);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, false, true)) << "4.9 degC should be Red (critical low)";

    // Just below CRIT_HI: 104.9 degC (1049 deci-degC) -> Yellow, not Red
    soc.tempSensor->setRawValue(1049);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "104.9 degC (below CRIT_HI) should be Yellow";
}

TEST_F(ThermalMonitorTest, SensorI2cError) {
    soc.eepromFile->setByte(0, static_cast<uint8_t>(SensorType::TypeA_1_Per_Unit));

    soc.tempSensor->setRawValue(80);
    ASSERT_TRUE(monitor->start());
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "80 degC should be Green";

    soc.tempSensor->injectError(true);

    bool sawRedOn = false, sawRedOff = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{150};
    while (std::chrono::steady_clock::now() < deadline) {
        if (soc.gpioRed->getValue() == 1) sawRedOn  = true;
        if (soc.gpioRed->getValue() == 0) sawRedOff = true;
        if (sawRedOn && sawRedOff) break;
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    EXPECT_EQ(soc.gpioGreen->getValue(),  0) << "Green must be off in error";
    EXPECT_EQ(soc.gpioYellow->getValue(), 0) << "Yellow must be off in error";
    EXPECT_TRUE(sawRedOn  ) << "Red must turn on during blink";
    EXPECT_TRUE(sawRedOff ) << "Red must turn off during blink";

    soc.tempSensor->injectError(false);
    soc.tempSensor->setRawValue(80);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "Recovery should be Green";
}

TEST_F(ThermalMonitorTest, SensorTypeReadOnceAtStart) {
    soc.eepromFile->setByte(0, static_cast<uint8_t>(SensorType::TypeB_0_1_Per_Unit));
    soc.tempSensor->setRawValue(900);
    ASSERT_TRUE(monitor->start());
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "TypeB raw 900 (90.0 degC) -> Yellow";

    soc.eepromFile->setByte(0, static_cast<uint8_t>(SensorType::TypeA_1_Per_Unit));
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "EEPROM change after start must be ignored";
}

TEST_F(ThermalMonitorTest, TemperatureCallback) {
    soc.eepromFile->setByte(0, static_cast<uint8_t>(SensorType::TypeA_1_Per_Unit));
    soc.tempSensor->setRawValue(25);

    std::atomic<int16_t> captured{-999};
    monitor->setOnTemperatureCallback([&](int16_t t) { captured = t; });

    ASSERT_TRUE(monitor->start());
    std::this_thread::sleep_for(SETTLE);

    EXPECT_EQ(captured.load(), 250) << "25 degC TypeA -> 250 deci-degC";
}

TEST_F(ThermalMonitorTest, MedianFilterSuppressesOutlier) {
    soc.eepromFile->setByte(0, static_cast<uint8_t>(SensorType::TypeA_1_Per_Unit));
    monitor->setStabilityConfig(ThermalStabilityConfig{
        3,
        std::chrono::milliseconds{1},
        0
    });

    soc.tempSensor->setRawSequence({84, 130, 84});
    ASSERT_TRUE(monitor->start());
    std::this_thread::sleep_for(std::chrono::milliseconds{40});

    EXPECT_TRUE(checkLEDs(true, false, false)) << "median(84, 130, 84) degC should stay Green";
}

TEST_F(ThermalMonitorTest, HysteresisUsesConvertedTypeBTemperature) {
    soc.eepromFile->setByte(0, static_cast<uint8_t>(SensorType::TypeB_0_1_Per_Unit));
    monitor->setStabilityConfig(ThermalStabilityConfig{
        1,
        std::chrono::milliseconds{0},
        20
    });

    soc.tempSensor->setRawValue(850);
    ASSERT_TRUE(monitor->start());
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "85.0 degC enters Yellow";

    soc.tempSensor->setRawValue(840);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(false, true, false)) << "84.0 degC remains Yellow with 2.0 degC hysteresis";

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
    EXPECT_TRUE(checkLEDs(false, false, true)) << "6.0 degC remains Red low with 2.0 degC hysteresis";

    soc.tempSensor->setRawValue(70);
    std::this_thread::sleep_for(SETTLE);
    EXPECT_TRUE(checkLEDs(true, false, false)) << "7.0 degC exits Red low";
}

TEST_F(ThermalMonitorTest, StartStop) {
    EXPECT_FALSE(monitor->isRunning());
    ASSERT_TRUE(monitor->start());
    EXPECT_TRUE(monitor->isRunning());
    monitor->stop();
    EXPECT_FALSE(monitor->isRunning());
}

TEST(ThermalMonitorEepromTest, EepromMissingAtStart) {
    MockLinuxOs os;
    MockI2cBus  bus;
    auto sensor = std::make_shared<MockTempSensorPeripheral>();
    sensor->setRawValue(25);
    bus.attach(0x48, sensor);
    os.registerPath("/dev/i2c-1", std::make_shared<MockI2cDevFile>(bus));

    auto gpioGreen  = std::make_shared<MockGpioValueFile>();
    auto gpioYellow = std::make_shared<MockGpioValueFile>();
    auto gpioRed    = std::make_shared<MockGpioValueFile>();
    os.registerPath("/sys/class/gpio/gpio10/value", gpioGreen);
    os.registerPath("/sys/class/gpio/gpio11/value", gpioYellow);
    os.registerPath("/sys/class/gpio/gpio12/value", gpioRed);

    ThermalMonitor monitor(os, std::chrono::milliseconds{10});
    ASSERT_TRUE(monitor.start());

    bool sawOn = false, sawOff = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{150};
    while (std::chrono::steady_clock::now() < deadline) {
        if (gpioRed->getValue() == 1) sawOn  = true;
        if (gpioRed->getValue() == 0) sawOff = true;
        if (sawOn && sawOff) break;
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    EXPECT_EQ(gpioGreen->getValue(),  0) << "Green must be off";
    EXPECT_EQ(gpioYellow->getValue(), 0) << "Yellow must be off";
    EXPECT_TRUE(sawOn  ) << "Red must blink on";
    EXPECT_TRUE(sawOff ) << "Red must blink off";

    monitor.stop();
}
