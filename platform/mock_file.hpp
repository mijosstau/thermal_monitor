#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/types.h>

class IMockFile {
public:
    virtual ~IMockFile() = default;

    virtual ssize_t read  (void* buf, size_t n)             = 0;
    virtual ssize_t write (const void* buf, size_t n)       = 0;
    virtual int     ioctl (unsigned long request, void* arg) = 0;
    virtual off_t   lseek (off_t offset, int whence)        = 0;
    virtual int     close ()                                 = 0;
};
