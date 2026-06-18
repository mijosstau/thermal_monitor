#pragma once

#include <cstdint>
#include <cstddef>

class IMockI2cPeripheral {
public:
    virtual ~IMockI2cPeripheral() = default;

    virtual bool write(const uint8_t* data, size_t len) = 0;
    virtual bool read (uint8_t* out,        size_t len) = 0;
};
