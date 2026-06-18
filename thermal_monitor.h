#pragma once

#include "interfaces.h"
#include "platform/linux_like_os.hpp"

#include <atomic>
#include <optional>
#include <chrono>
#include <functional>
#include <cstdint>
#include <mutex>
#include <thread>

constexpr int16_t CRIT_LO = 50;
constexpr int16_t WARN    = 850;
constexpr int16_t CRIT_HI = 1050;

struct ThermalStabilityConfig {
    uint8_t sampleCount = 1;
    std::chrono::milliseconds sampleSpacing{0};
    int16_t hysteresisDeciCelsius = 0;
};

class ThermalMonitor {
public:
    using TemperatureCallback = std::function<void(int16_t tempDeciCelsius)>;

    ThermalMonitor(ILinuxLikeOs& os,
                   std::chrono::milliseconds pollInterval = std::chrono::milliseconds{100});
    ~ThermalMonitor();

    bool start(TemperatureCallback cb = nullptr);
    void stop();
    bool isRunning() const { return running_.load(); }

    void setOnTemperatureCallback(TemperatureCallback cb);
    void setStabilityConfig(ThermalStabilityConfig config);

private:
    enum class LedState { Green, Yellow, RedLow, RedHigh };

    void pollingLoop();

    std::optional<SensorType>  readSensorType();
    std::optional<int16_t>     readRawFromI2c();
    std::optional<int16_t>     readFilteredTemperature(SensorType type);
    int16_t                    convertToDeciCelsius(int16_t raw, SensorType type);
    int16_t                    median(int16_t* values, size_t count);
    void                       evaluateAndUpdate(int16_t temp_dC);
    LedState                   classify(int16_t temp_dC) const;
    void                       applyLedState(LedState state);
    void                       setErrorState();
    void                       setupGpios();
    void                       writeGpio(const char* path, bool on);
    void                       setGpioLEDs(bool green, bool yellow, bool red);

    ILinuxLikeOs& os_;
    std::chrono::milliseconds pollInterval_;

    std::thread         thread_;
    std::atomic<bool>   running_{false};
    TemperatureCallback tempCallback_;
    mutable std::mutex  callbackMutex_;

    Fd   i2cFd_      = -1;
    bool errorBlink_ = false;
    LedState ledState_ = LedState::Green;
    ThermalStabilityConfig stability_;

    static constexpr uint16_t    SENSOR_ADDR = 0x48;
    static constexpr uint8_t     TEMP_REG    = 0x00;
    static constexpr size_t      MAX_FILTER_SAMPLES = 9;
    static constexpr const char* GPIO_EXPORT     = "/sys/class/gpio/export";
    static constexpr const char* GPIO_GREEN      = "/sys/class/gpio/gpio10/value";
    static constexpr const char* GPIO_YELLOW     = "/sys/class/gpio/gpio11/value";
    static constexpr const char* GPIO_RED        = "/sys/class/gpio/gpio12/value";
    static constexpr const char* GPIO_GREEN_DIR  = "/sys/class/gpio/gpio10/direction";
    static constexpr const char* GPIO_YELLOW_DIR = "/sys/class/gpio/gpio11/direction";
    static constexpr const char* GPIO_RED_DIR    = "/sys/class/gpio/gpio12/direction";
};
