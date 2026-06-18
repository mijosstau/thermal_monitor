#pragma once

#include "linux_like_os.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

// Wraps the actual Linux syscalls — OpenFlags values match O_RDONLY/O_WRONLY/O_RDWR exactly.
class RealLinuxOs : public ILinuxLikeOs {
public:
    Fd open(const std::string& path, OpenFlags flags) override {
        return ::open(path.c_str(), static_cast<int>(flags));
    }
    ssize_t read(Fd fd, void* buf, size_t n) override {
        return ::read(fd, buf, n);
    }
    ssize_t write(Fd fd, const void* buf, size_t n) override {
        return ::write(fd, buf, n);
    }
    int ioctl(Fd fd, unsigned long request, void* arg) override {
        return ::ioctl(fd, request, arg);
    }
    off_t lseek(Fd fd, off_t offset, int whence) override {
        return ::lseek(fd, offset, whence);
    }
    int close(Fd fd) override {
        return ::close(fd);
    }
};
