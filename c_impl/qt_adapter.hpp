#pragma once

#include "../interfaces.h"
#include <cstdint>
#include <memory>

class CThermalMonitorAdapter {
public:
    CThermalMonitorAdapter();
    ~CThermalMonitorAdapter();

    bool  start();
    void  stop();
    bool  isRunning() const;

    void  setRawValue(int16_t v);
    void  setSensorType(SensorType type);
    void  injectSensorError(bool err);
    void  setStabilityConfig(uint8_t sampleCount,
                             unsigned int sampleSpacingMs,
                             int16_t hysteresisDeciCelsius);

    int   getGpioValue(int gpio_num);  // 0 or 1; gpio 10=green 11=yellow 12=red

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
