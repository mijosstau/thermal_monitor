#pragma once

#include "../mock_file.hpp"
#include "../../platform/linux_like_os.hpp"

#include <vector>
#include <cstring>
#include <algorithm>

class MockEepromFile final : public IMockFile {
public:
    explicit MockEepromFile(size_t size) : memory_(size, 0xff) {}

    ssize_t read(void* buf, size_t n) override {
        if (readOnly_ && false) {} // readOnly checked on write only
        if (offset_ >= memory_.size()) return 0;
        size_t count = std::min(n, memory_.size() - offset_);
        std::memcpy(buf, memory_.data() + offset_, count);
        offset_ += count;
        return static_cast<ssize_t>(count);
    }

    ssize_t write(const void* buf, size_t n) override {
        if (readOnly_) return -1;
        if (offset_ >= memory_.size()) return 0;
        size_t count = std::min(n, memory_.size() - offset_);
        std::memcpy(memory_.data() + offset_, buf, count);
        offset_ += count;
        return static_cast<ssize_t>(count);
    }

    int ioctl(uint64_t, void*) override { return -1; }

    off_t lseek(off_t offset, int whence) override {
        off_t newOffset = 0;
        switch (whence) {
            case Seek_Set: newOffset = offset; break;
            case Seek_Cur: newOffset = static_cast<off_t>(offset_) + offset; break;
            case Seek_End: newOffset = static_cast<off_t>(memory_.size()) + offset; break;
            default: return -1;
        }
        if (newOffset < 0 || static_cast<size_t>(newOffset) > memory_.size()) return -1;
        offset_ = static_cast<size_t>(newOffset);
        return static_cast<off_t>(offset_);
    }

    int close() override { return 0; }

    void setByte(size_t offset, uint8_t value)  { memory_.at(offset) = value; }
    uint8_t getByte(size_t offset) const         { return memory_.at(offset); }
    void setReadOnly(bool ro)                    { readOnly_ = ro; }

private:
    std::vector<uint8_t> memory_;
    size_t offset_ = 0;
    bool readOnly_ = false;
};
