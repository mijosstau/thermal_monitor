#pragma once

#include "../mock_linux_os.hpp"
#include "../i2c/mock_i2c_bus.hpp"
#include "../i2c/mock_i2c_peripheral.hpp"
#include "../i2c/mock_i2c_dev_file.hpp"
#include "../eeprom/mock_eeprom_file.hpp"
#include "../gpio/mock_gpio_value_file.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

class MockTempSensorPeripheral final : public IMockI2cPeripheral {
public:
    void setRawValue(int16_t v) {
        std::lock_guard lock(mutex_);
        sequence_.clear();
        sequenceIndex_ = 0;
        setRawValueLocked(v);
        error_ = false;
    }

    void setRawSequence(std::vector<int16_t> values) {
        std::lock_guard lock(mutex_);
        sequence_ = std::move(values);
        sequenceIndex_ = 0;
        if (!sequence_.empty()) setRawValueLocked(sequence_.front());
        error_ = false;
    }

    void injectError(bool err) {
        std::lock_guard lock(mutex_);
        error_ = err;
    }

    bool write(const uint8_t* data, size_t len) override {
        std::lock_guard lock(mutex_);
        if (error_) return false;
        if (len >= 1) regPtr_ = data[0];
        return true;
    }

    bool read(uint8_t* out, size_t len) override {
        std::lock_guard lock(mutex_);
        if (error_) return false;
        if (regPtr_ == 0 && !sequence_.empty()) {
            const int16_t value = sequence_[sequenceIndex_];
            setRawValueLocked(value);
            if (sequenceIndex_ + 1 < sequence_.size()) ++sequenceIndex_;
        }
        for (size_t i = 0; i < len; ++i)
            out[i] = regs_[regPtr_++];
        return true;
    }

private:
    void setRawValueLocked(int16_t v) {
        regs_[0] = static_cast<uint8_t>(v >> 8);
        regs_[1] = static_cast<uint8_t>(v & 0xFF);
    }

    uint8_t regs_[256]{};
    uint8_t regPtr_ = 0;
    bool error_ = false;
    std::vector<int16_t> sequence_;
    size_t sequenceIndex_ = 0;
    std::mutex mutex_;
};

struct MockSoC {
    MockLinuxOs os;
    MockI2cBus  i2cBus;

    std::shared_ptr<MockTempSensorPeripheral> tempSensor;
    std::shared_ptr<MockEepromFile>           eepromFile;
    std::shared_ptr<MockGpioValueFile>        gpioGreen;
    std::shared_ptr<MockGpioValueFile>        gpioYellow;
    std::shared_ptr<MockGpioValueFile>        gpioRed;

    MockSoC() {
        tempSensor = std::make_shared<MockTempSensorPeripheral>();
        i2cBus.attach(0x48, tempSensor);
        os.registerPath("/dev/i2c-1", std::make_shared<MockI2cDevFile>(i2cBus));

        eepromFile = std::make_shared<MockEepromFile>(256);
        eepromFile->setByte(0, 0x00);
        os.registerPath("/sys/bus/nvmem/devices/eeprom0/nvmem", eepromFile);

        gpioGreen  = std::make_shared<MockGpioValueFile>();
        gpioYellow = std::make_shared<MockGpioValueFile>();
        gpioRed    = std::make_shared<MockGpioValueFile>();
        os.registerPath("/sys/class/gpio/gpio10/value",  gpioGreen);
        os.registerPath("/sys/class/gpio/gpio11/value",  gpioYellow);
        os.registerPath("/sys/class/gpio/gpio12/value",  gpioRed);

        // Export and direction — write-only sinks, no state needed in tests
        auto exportFile = std::make_shared<MockSinkFile>();
        os.registerPath("/sys/class/gpio/export",              exportFile);
        os.registerPath("/sys/class/gpio/gpio10/direction", std::make_shared<MockSinkFile>());
        os.registerPath("/sys/class/gpio/gpio11/direction", std::make_shared<MockSinkFile>());
        os.registerPath("/sys/class/gpio/gpio12/direction", std::make_shared<MockSinkFile>());
    }
};
