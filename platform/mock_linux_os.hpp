#pragma once

#include "linux_like_os.hpp"
#include "mock_file.hpp"

#include <unordered_map>
#include <memory>
#include <mutex>

class MockLinuxOs final : public ILinuxLikeOs {
public:
    void registerPath(const std::string& path, std::shared_ptr<IMockFile> file) {
        std::lock_guard lock(mutex_);
        pathRegistry_[path] = std::move(file);
    }

    Fd open(const std::string& path, OpenFlags) override {
        std::lock_guard lock(mutex_);
        auto it = pathRegistry_.find(path);
        if (it == pathRegistry_.end()) return -1;
        Fd fd = nextFd_++;
        openFiles_[fd] = it->second;
        return fd;
    }

    ssize_t read(Fd fd, void* buf, size_t n) override {
        auto f = get(fd);
        return f ? f->read(buf, n) : -1;
    }

    ssize_t write(Fd fd, const void* buf, size_t n) override {
        auto f = get(fd);
        return f ? f->write(buf, n) : -1;
    }

    int ioctl(Fd fd, unsigned long request, void* arg) override {
        auto f = get(fd);
        return f ? f->ioctl(request, arg) : -1;
    }

    off_t lseek(Fd fd, off_t offset, int whence) override {
        auto f = get(fd);
        return f ? f->lseek(offset, whence) : -1;
    }

    int close(Fd fd) override {
        std::lock_guard lock(mutex_);
        openFiles_.erase(fd);
        return 0;
    }

private:
    std::shared_ptr<IMockFile> get(Fd fd) {
        std::lock_guard lock(mutex_);
        auto it = openFiles_.find(fd);
        return it != openFiles_.end() ? it->second : nullptr;
    }

    std::mutex mutex_;
    Fd nextFd_ = 3;
    std::unordered_map<std::string, std::shared_ptr<IMockFile>> pathRegistry_;
    std::unordered_map<Fd, std::shared_ptr<IMockFile>> openFiles_;
};
