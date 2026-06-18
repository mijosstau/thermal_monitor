#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SENSOR_TYPE_A = 0, SENSOR_TYPE_B = 1 } CSensorType;

typedef enum {
    C_OPEN_READ_ONLY  = 0,
    C_OPEN_WRITE_ONLY = 1,
    C_OPEN_READ_WRITE = 2,
} COpenFlags;

typedef struct CLinuxLikeOs {
    int     (*open) (struct CLinuxLikeOs* self, const char* path, COpenFlags flags);
    ssize_t (*read) (struct CLinuxLikeOs* self, int fd, void* buf, size_t n);
    ssize_t (*write)(struct CLinuxLikeOs* self, int fd, const void* buf, size_t n);
    int     (*ioctl)(struct CLinuxLikeOs* self, int fd, unsigned long request, void* arg);
    off_t   (*lseek)(struct CLinuxLikeOs* self, int fd, off_t offset, int whence);
    int     (*close)(struct CLinuxLikeOs* self, int fd);
} CLinuxLikeOs;

#ifdef __cplusplus
}
#endif
