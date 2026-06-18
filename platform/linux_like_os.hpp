#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <sys/types.h>

using Fd = int;

enum class OpenFlags : uint32_t {
    ReadOnly  = 0,   // O_RDONLY
    WriteOnly = 1,   // O_WRONLY
    ReadWrite = 2,   // O_RDWR
};

// SEEK_SET / SEEK_CUR / SEEK_END — same values as POSIX
inline constexpr int Seek_Set = 0;
inline constexpr int Seek_Cur = 1;
inline constexpr int Seek_End = 2;

class ILinuxLikeOs {
public:
    virtual ~ILinuxLikeOs() = default;

    virtual Fd      open  (const std::string& path, OpenFlags flags)              = 0;
    virtual ssize_t read  (Fd fd, void* buf, size_t n)                            = 0;
    virtual ssize_t write (Fd fd, const void* buf, size_t n)                      = 0;
    virtual int     ioctl (Fd fd, unsigned long request, void* arg)               = 0;
    virtual off_t   lseek (Fd fd, off_t offset, int whence)                       = 0;
    virtual int     close (Fd fd)                                                  = 0;
};
