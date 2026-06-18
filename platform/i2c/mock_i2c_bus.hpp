#pragma once

#include "mock_i2c_peripheral.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>

class MockI2cBus {
public:
    void attach(uint16_t address, std::shared_ptr<IMockI2cPeripheral> device) {
        devices_[address] = std::move(device);
    }

    bool write(uint16_t address, const uint8_t* data, size_t len) {
        auto it = devices_.find(address);
        return it != devices_.end() && it->second->write(data, len);
    }

    bool read(uint16_t address, uint8_t* out, size_t len) {
        auto it = devices_.find(address);
        return it != devices_.end() && it->second->read(out, len);
    }

private:
    std::unordered_map<uint16_t, std::shared_ptr<IMockI2cPeripheral>> devices_;
};
