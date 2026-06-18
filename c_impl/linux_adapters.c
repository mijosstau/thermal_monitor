#include "linux_adapters.h"

#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int to_posix_flags(COpenFlags flags)
{
    switch (flags) {
        case C_OPEN_READ_ONLY:  return O_RDONLY;
        case C_OPEN_WRITE_ONLY: return O_WRONLY;
        case C_OPEN_READ_WRITE: return O_RDWR;
    }
    return O_RDONLY;
}

static int linux_open(CLinuxLikeOs* self, const char* path, COpenFlags flags)
{
    (void)self;
    return open(path, to_posix_flags(flags));
}

static ssize_t linux_read(CLinuxLikeOs* self, int fd, void* buf, size_t n)
{
    (void)self;
    return read(fd, buf, n);
}

static ssize_t linux_write(CLinuxLikeOs* self, int fd, const void* buf, size_t n)
{
    (void)self;
    return write(fd, buf, n);
}

static int linux_ioctl(CLinuxLikeOs* self, int fd, unsigned long request, void* arg)
{
    (void)self;
    return ioctl(fd, request, arg);
}

static off_t linux_lseek(CLinuxLikeOs* self, int fd, off_t offset, int whence)
{
    (void)self;
    return lseek(fd, offset, whence);
}

static int linux_close(CLinuxLikeOs* self, int fd)
{
    (void)self;
    return close(fd);
}

void linux_os_init(LinuxOs* os)
{
    os->base.open = linux_open;
    os->base.read = linux_read;
    os->base.write = linux_write;
    os->base.ioctl = linux_ioctl;
    os->base.lseek = linux_lseek;
    os->base.close = linux_close;
}

int linux_gpio_read_value(const char* path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    char value = '\0';
    ssize_t n = read(fd, &value, 1);
    close(fd);
    if (n < 1) return -1;
    return value == '1' ? 1 : 0;
}
