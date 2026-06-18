#include "qt_adapter.hpp"

extern "C" {
#include "thermal_monitor.h"
}

#include "mock_linux_os_c_adapter.hpp"
#include "../platform/soc/mock_soc.hpp"

struct CThermalMonitorAdapter::Impl {
    MockSoC soc;
    CMockLinuxOsAdapter osAdapter;
    ThermalMonitor monitor;
};

CThermalMonitorAdapter::CThermalMonitorAdapter()
    : impl_(std::make_unique<Impl>())
{
    c_mock_linux_os_adapter_init(&impl_->osAdapter, impl_->soc.os);
    thermal_monitor_init(&impl_->monitor,
                         reinterpret_cast<CLinuxLikeOs*>(&impl_->osAdapter),
                         TM_POLL_MS);
}

CThermalMonitorAdapter::~CThermalMonitorAdapter() {
    stop();
}

bool CThermalMonitorAdapter::start() {
    return thermal_monitor_start(&impl_->monitor, nullptr);
}

void CThermalMonitorAdapter::stop() {
    thermal_monitor_stop(&impl_->monitor);
}

bool CThermalMonitorAdapter::isRunning() const {
    return thermal_monitor_running(&impl_->monitor);
}

void CThermalMonitorAdapter::setRawValue(int16_t v) {
    impl_->soc.tempSensor->setRawValue(v);
}

void CThermalMonitorAdapter::setSensorType(SensorType type) {
    impl_->soc.eepromFile->setByte(0, static_cast<uint8_t>(type));
}

void CThermalMonitorAdapter::injectSensorError(bool err) {
    impl_->soc.tempSensor->injectError(err);
}

void CThermalMonitorAdapter::setStabilityConfig(uint8_t sampleCount,
                                                unsigned int sampleSpacingMs,
                                                int16_t hysteresisDeciCelsius) {
    thermal_monitor_set_stability_config(&impl_->monitor,
                                         ThermalStabilityConfig{
                                             sampleCount,
                                             sampleSpacingMs,
                                             hysteresisDeciCelsius
                                         });
}

int CThermalMonitorAdapter::getGpioValue(int gpio_num) {
    if (gpio_num == 10) return impl_->soc.gpioGreen->getValue();
    if (gpio_num == 11) return impl_->soc.gpioYellow->getValue();
    if (gpio_num == 12) return impl_->soc.gpioRed->getValue();
    return 0;
}
