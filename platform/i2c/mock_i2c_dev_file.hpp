#pragma once

#include "../mock_file.hpp"
#include "mock_i2c_bus.hpp"
#include "i2c_types.hpp"

class MockI2cDevFile final : public IMockFile {
public:
    explicit MockI2cDevFile(MockI2cBus& bus) : bus_(bus) {}

    ssize_t read(void* buf, size_t n) override {
        if (currentAddr_ < 0) return -1;
        return bus_.read(static_cast<uint16_t>(currentAddr_),
                         static_cast<uint8_t*>(buf), n) ? static_cast<ssize_t>(n) : -1;
    }

    ssize_t write(const void* buf, size_t n) override {
        if (currentAddr_ < 0) return -1;
        return bus_.write(static_cast<uint16_t>(currentAddr_),
                          static_cast<const uint8_t*>(buf), n) ? static_cast<ssize_t>(n) : -1;
    }

    int ioctl(unsigned long request, void* arg) override {
        switch (request) {
            case I2C_SLAVE:
                currentAddr_ = static_cast<int>(reinterpret_cast<uintptr_t>(arg));
                return 0;
            case I2C_RDWR:
                return rdwr(*static_cast<i2c_rdwr_data*>(arg));
            default:
                return -1;
        }
    }

    off_t lseek(off_t, int) override { return -1; }
    int   close()          override { currentAddr_ = -1; return 0; }

private:
    int rdwr(i2c_rdwr_data& data) {
        for (uint32_t i = 0; i < data.nmsgs; ++i) {
            i2c_msg& msg = data.msgs[i];
            bool ok = (msg.flags & I2C_M_RD)
                ? bus_.read (msg.addr, msg.buf, msg.len)
                : bus_.write(msg.addr, msg.buf, msg.len);
            if (!ok) return -1;
        }
        return static_cast<int>(data.nmsgs);
    }

    MockI2cBus& bus_;
    int currentAddr_ = -1;
};
