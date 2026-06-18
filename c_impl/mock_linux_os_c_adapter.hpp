#pragma once

extern "C" {
#include "interfaces.h"
}

#include "../platform/linux_like_os.hpp"

#include <cstdint>

struct CMockLinuxOsAdapter {
    CLinuxLikeOs base{};
    ILinuxLikeOs* os = nullptr;
};

inline OpenFlags toOpenFlags(COpenFlags flags)
{
    switch (flags) {
        case C_OPEN_READ_ONLY:  return OpenFlags::ReadOnly;
        case C_OPEN_WRITE_ONLY: return OpenFlags::WriteOnly;
        case C_OPEN_READ_WRITE: return OpenFlags::ReadWrite;
    }
    return OpenFlags::ReadOnly;
}

inline int cMockOpen(CLinuxLikeOs* self, const char* path, COpenFlags flags)
{
    auto* adapter = reinterpret_cast<CMockLinuxOsAdapter*>(self);
    return adapter->os->open(path, toOpenFlags(flags));
}

inline ssize_t cMockRead(CLinuxLikeOs* self, int fd, void* buf, size_t n)
{
    auto* adapter = reinterpret_cast<CMockLinuxOsAdapter*>(self);
    return adapter->os->read(fd, buf, n);
}

inline ssize_t cMockWrite(CLinuxLikeOs* self, int fd, const void* buf, size_t n)
{
    auto* adapter = reinterpret_cast<CMockLinuxOsAdapter*>(self);
    return adapter->os->write(fd, buf, n);
}

inline int cMockIoctl(CLinuxLikeOs* self, int fd, unsigned long request, void* arg)
{
    auto* adapter = reinterpret_cast<CMockLinuxOsAdapter*>(self);
    return adapter->os->ioctl(fd, request, arg);
}

inline off_t cMockLseek(CLinuxLikeOs* self, int fd, off_t offset, int whence)
{
    auto* adapter = reinterpret_cast<CMockLinuxOsAdapter*>(self);
    return adapter->os->lseek(fd, offset, whence);
}

inline int cMockClose(CLinuxLikeOs* self, int fd)
{
    auto* adapter = reinterpret_cast<CMockLinuxOsAdapter*>(self);
    return adapter->os->close(fd);
}

inline void c_mock_linux_os_adapter_init(CMockLinuxOsAdapter* adapter, ILinuxLikeOs& os)
{
    adapter->base.open = cMockOpen;
    adapter->base.read = cMockRead;
    adapter->base.write = cMockWrite;
    adapter->base.ioctl = cMockIoctl;
    adapter->base.lseek = cMockLseek;
    adapter->base.close = cMockClose;
    adapter->os = &os;
}
