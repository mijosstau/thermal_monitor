#include "thermal_monitor.h"
#include "platform/i2c/i2c_types.hpp"

#include <thread>
#include <chrono>
#include <algorithm>
#include <cassert>

ThermalMonitor::ThermalMonitor(ILinuxLikeOs& os,
                               std::chrono::milliseconds pollInterval)
    : os_(os), pollInterval_(pollInterval) {}

ThermalMonitor::~ThermalMonitor() { stop(); }

void ThermalMonitor::setOnTemperatureCallback(TemperatureCallback cb) {
    std::lock_guard lock(callbackMutex_);
    tempCallback_ = std::move(cb);
}

void ThermalMonitor::setStabilityConfig(ThermalStabilityConfig config) {
    if (config.sampleCount == 0) config.sampleCount = 1;
    if (config.sampleCount > MAX_FILTER_SAMPLES) config.sampleCount = MAX_FILTER_SAMPLES;
    if (config.hysteresisDeciCelsius < 0) config.hysteresisDeciCelsius = 0;
    stability_ = config;
}

bool ThermalMonitor::start(TemperatureCallback cb) {
    if (running_.exchange(true)) return true;

    if (cb) {
        std::lock_guard lock(callbackMutex_);
        tempCallback_ = std::move(cb);
    }

    i2cFd_ = os_.open("/dev/i2c-1", OpenFlags::ReadWrite);
    if (i2cFd_ < 0) {
        running_.store(false);
        return false;
    }

    setupGpios();
    thread_ = std::thread([this] { pollingLoop(); });
    return true;
}

void ThermalMonitor::setupGpios() {
    struct { const char* num; const char* dir; } gpios[] = {
        { "10\n", GPIO_GREEN_DIR  },
        { "11\n", GPIO_YELLOW_DIR },
        { "12\n", GPIO_RED_DIR    },
    };
    for (auto& g : gpios) {
        Fd fd = os_.open(GPIO_EXPORT, OpenFlags::WriteOnly);
        if (fd >= 0) { os_.write(fd, g.num, 3); os_.close(fd); }
        fd = os_.open(g.dir, OpenFlags::WriteOnly);
        if (fd >= 0) { os_.write(fd, "out\n", 4); os_.close(fd); }
    }
}

void ThermalMonitor::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
    os_.close(i2cFd_);
    i2cFd_ = -1;
}

void ThermalMonitor::writeGpio(const char* path, bool on) {
    Fd fd = os_.open(path, OpenFlags::WriteOnly);
    if (fd >= 0) {
        const char v[2] = { on ? '1' : '0', '\n' };
        os_.write(fd, v, 2);
        os_.close(fd);
    }
}

void ThermalMonitor::setGpioLEDs(bool green, bool yellow, bool red) {
    writeGpio(GPIO_GREEN,  green);
    writeGpio(GPIO_YELLOW, yellow);
    writeGpio(GPIO_RED,    red);
}

void ThermalMonitor::pollingLoop() {
    const auto typeOpt = readSensorType();

    while (running_.load()) {
        const auto loopStart = std::chrono::steady_clock::now();

        if (!typeOpt.has_value()) {
            setErrorState();
        } else {
            auto temp = readFilteredTemperature(typeOpt.value());
            if (!temp.has_value()) {
                setErrorState();
            } else {
                int16_t temp_dC = temp.value();
                evaluateAndUpdate(temp_dC);
                TemperatureCallback cb;
                { std::lock_guard lock(callbackMutex_); cb = tempCallback_; }
                if (cb) cb(temp_dC);
            }
        }

        const auto elapsed = std::chrono::steady_clock::now() - loopStart;
        if (elapsed < pollInterval_)
            std::this_thread::sleep_for(pollInterval_ - elapsed);
    }
}

std::optional<SensorType> ThermalMonitor::readSensorType() {
    Fd fd = os_.open("/sys/bus/nvmem/devices/eeprom0/nvmem", OpenFlags::ReadOnly);
    if (fd < 0) return std::nullopt;

    os_.lseek(fd, 0, Seek_Set);
    uint8_t byte = 0;
    ssize_t n = os_.read(fd, &byte, 1);
    os_.close(fd);

    if (n < 1) return std::nullopt;

    return (byte == static_cast<uint8_t>(SensorType::TypeB_0_1_Per_Unit))
        ? SensorType::TypeB_0_1_Per_Unit
        : SensorType::TypeA_1_Per_Unit;
}

std::optional<int16_t> ThermalMonitor::readRawFromI2c() {
    uint8_t regAddr = TEMP_REG;
    uint8_t buf[2]  = {};

    i2c_msg msgs[2] = {
        { SENSOR_ADDR, 0,        1, &regAddr },
        { SENSOR_ADDR, I2C_M_RD, 2, buf      },
    };
    i2c_rdwr_data data = { msgs, 2 };

    if (os_.ioctl(i2cFd_, I2C_RDWR, &data) >= 0)
        return static_cast<int16_t>((buf[0] << 8) | buf[1]);

    // Linux i2c-stub exposes SMBus operations but not full I2C_RDWR transfers.
    if (os_.ioctl(i2cFd_, I2C_SLAVE, reinterpret_cast<void*>(static_cast<uintptr_t>(SENSOR_ADDR))) < 0)
        return std::nullopt;

    i2c_smbus_data smbusData{};
    i2c_smbus_ioctl_data smbusArgs{
        I2C_SMBUS_READ,
        TEMP_REG,
        I2C_SMBUS_WORD_DATA,
        &smbusData
    };

    if (os_.ioctl(i2cFd_, I2C_SMBUS, &smbusArgs) < 0)
        return std::nullopt;

    return static_cast<int16_t>(smbusData.word);
}

int16_t ThermalMonitor::convertToDeciCelsius(int16_t raw, SensorType type) {
    if (type == SensorType::TypeA_1_Per_Unit) return raw * 10;
    return raw;
}

std::optional<int16_t> ThermalMonitor::readFilteredTemperature(SensorType type) {
    int16_t values[MAX_FILTER_SAMPLES]{};
    const size_t count = std::max<size_t>(1, std::min<size_t>(stability_.sampleCount, MAX_FILTER_SAMPLES));

    for (size_t i = 0; i < count; ++i) {
        auto raw = readRawFromI2c();
        if (!raw.has_value()) return std::nullopt;
        values[i] = convertToDeciCelsius(raw.value(), type);

        if (i + 1 < count && stability_.sampleSpacing.count() > 0) {
            const auto until = std::chrono::steady_clock::now() + stability_.sampleSpacing;
            while (running_.load() && std::chrono::steady_clock::now() < until)
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            if (!running_.load()) return std::nullopt;
        }
    }

    return median(values, count);
}

int16_t ThermalMonitor::median(int16_t* values, size_t count) {
    std::sort(values, values + count);
    for (size_t i = 1; i < count; ++i) {
        assert(values[i - 1] <= values[i]);
    }
    return values[count / 2];
}

void ThermalMonitor::evaluateAndUpdate(int16_t temp_dC) {
    ledState_ = classify(temp_dC);
    applyLedState(ledState_);
}

ThermalMonitor::LedState ThermalMonitor::classify(int16_t temp_dC) const {
    const int16_t h = stability_.hysteresisDeciCelsius;

    switch (ledState_) {
        case LedState::RedHigh:
            if (temp_dC >= static_cast<int16_t>(CRIT_HI - h)) return LedState::RedHigh;
            break;
        case LedState::RedLow:
            if (temp_dC < static_cast<int16_t>(CRIT_LO + h)) return LedState::RedLow;
            break;
        case LedState::Yellow:
            if (temp_dC >= static_cast<int16_t>(WARN - h) && temp_dC < CRIT_HI) return LedState::Yellow;
            break;
        case LedState::Green:
            break;
    }

    if (temp_dC >= CRIT_HI) return LedState::RedHigh;
    if (temp_dC < CRIT_LO) return LedState::RedLow;
    if (temp_dC >= WARN) return LedState::Yellow;
    return LedState::Green;
}

void ThermalMonitor::applyLedState(LedState state) {
    switch (state) {
        case LedState::Green:
            setGpioLEDs(true, false, false);
            break;
        case LedState::Yellow:
            setGpioLEDs(false, true, false);
            break;
        case LedState::RedLow:
        case LedState::RedHigh:
            setGpioLEDs(false, false, true);
            break;
    }
}

void ThermalMonitor::setErrorState() {
    errorBlink_ = !errorBlink_;
    setGpioLEDs(false, false, errorBlink_);
    TemperatureCallback cb;
    { std::lock_guard lock(callbackMutex_); cb = tempCallback_; }
    if (cb) cb(-9999);
}
