#pragma once

#include <cstdint>

// Same values as Linux <linux/i2c-dev.h> — swap for real headers on target
constexpr unsigned long I2C_SLAVE = 0x0703;
constexpr unsigned long I2C_SMBUS = 0x0720;
constexpr unsigned long I2C_RDWR  = 0x0707;
constexpr uint16_t I2C_M_RD  = 0x0001;

constexpr uint8_t I2C_SMBUS_WRITE      = 0;
constexpr uint8_t I2C_SMBUS_READ       = 1;
constexpr int     I2C_SMBUS_WORD_DATA  = 3;

struct i2c_msg {
    uint16_t addr;
    uint16_t flags;
    uint16_t len;
    uint8_t* buf;
};

struct i2c_rdwr_data {
    i2c_msg* msgs;
    uint32_t nmsgs;
};

union i2c_smbus_data {
    uint8_t  byte;
    uint16_t word;
    uint8_t  block[34];
};

struct i2c_smbus_ioctl_data {
    uint8_t read_write;
    uint8_t command;
    int     size;
    i2c_smbus_data* data;
};
