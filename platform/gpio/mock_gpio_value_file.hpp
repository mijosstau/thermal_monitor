#pragma once

#include "../mock_file.hpp"
#include <mutex>
#include <cstring>
#include <algorithm>

// Write-only sink — accepts any write, discards it (used for /export and /direction)
class MockSinkFile final : public IMockFile {
public:
    ssize_t read(void*, size_t) override { return -1; }
    ssize_t write(const void*, size_t n) override { return static_cast<ssize_t>(n); }
    int   ioctl(unsigned long, void*) override { return -1; }
    off_t lseek(off_t, int)          override { return -1; }
    int   close()                    override { return 0; }
};

class MockGpioValueFile final : public IMockFile {
public:
    ssize_t read(void* buf, size_t n) override {
        std::lock_guard lock(mutex_);
        const char* v = value_ ? "1\n" : "0\n";
        size_t len = std::min(n, size_t(2));
        std::memcpy(buf, v, len);
        return static_cast<ssize_t>(len);
    }

    ssize_t write(const void* buf, size_t n) override {
        std::lock_guard lock(mutex_);
        if (n > 0) value_ = (static_cast<const char*>(buf)[0] == '1');
        return static_cast<ssize_t>(n);
    }

    int   ioctl(unsigned long, void*) override { return -1; }
    off_t lseek(off_t, int)          override { return 0; }
    int   close()                    override { return 0; }

    int getValue() const {
        std::lock_guard lock(mutex_);
        return value_ ? 1 : 0;
    }

private:
    mutable std::mutex mutex_;
    bool value_ = false;
};
