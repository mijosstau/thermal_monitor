#include "linux_adapters.h"
#include "thermal_monitor.h"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define TEMP_REG 0x00

static atomic_int g_last_temp;

typedef struct {
    CLinuxLikeOs base;
    LinuxOs real;
    const char* nvmem_path;
} PatchedLinuxOs;

static void on_temperature(int16_t temp_dC)
{
    atomic_store(&g_last_temp, temp_dC);
}

static bool write_file(const char* path, const void* data, size_t n)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror(path);
        return false;
    }
    ssize_t written = write(fd, data, n);
    close(fd);
    return written == (ssize_t)n;
}

static bool setup_demo_fs(const char* nvmem_path)
{
    uint8_t type_a = 0x00;
    if (!write_file(nvmem_path, &type_a, 1)) return false;
    return true;
}

static void print_gpio(const char* label, const char* path)
{
    int value = linux_gpio_read_value(path);
    printf("  %-8s = %d  (%s)\n", label, value, path);
}

static const char* translate_path(PatchedLinuxOs* os, const char* path)
{
    if (strcmp(path, "/sys/bus/nvmem/devices/eeprom0/nvmem") == 0)
        return os->nvmem_path;
    return path;
}

static int patched_open(CLinuxLikeOs* self, const char* path, COpenFlags flags)
{
    PatchedLinuxOs* os = (PatchedLinuxOs*)self;
    return os->real.base.open((CLinuxLikeOs*)&os->real, translate_path(os, path), flags);
}

static ssize_t patched_read(CLinuxLikeOs* self, int fd, void* buf, size_t n)
{
    PatchedLinuxOs* os = (PatchedLinuxOs*)self;
    return os->real.base.read((CLinuxLikeOs*)&os->real, fd, buf, n);
}

static ssize_t patched_write(CLinuxLikeOs* self, int fd, const void* buf, size_t n)
{
    PatchedLinuxOs* os = (PatchedLinuxOs*)self;
    return os->real.base.write((CLinuxLikeOs*)&os->real, fd, buf, n);
}

static int patched_ioctl(CLinuxLikeOs* self, int fd, unsigned long request, void* arg)
{
    PatchedLinuxOs* os = (PatchedLinuxOs*)self;
    return os->real.base.ioctl((CLinuxLikeOs*)&os->real, fd, request, arg);
}

static off_t patched_lseek(CLinuxLikeOs* self, int fd, off_t offset, int whence)
{
    PatchedLinuxOs* os = (PatchedLinuxOs*)self;
    return os->real.base.lseek((CLinuxLikeOs*)&os->real, fd, offset, whence);
}

static int patched_close(CLinuxLikeOs* self, int fd)
{
    PatchedLinuxOs* os = (PatchedLinuxOs*)self;
    return os->real.base.close((CLinuxLikeOs*)&os->real, fd);
}

static void patched_linux_os_init(PatchedLinuxOs* os, const char* nvmem_path)
{
    linux_os_init(&os->real);
    os->base.open = patched_open;
    os->base.read = patched_read;
    os->base.write = patched_write;
    os->base.ioctl = patched_ioctl;
    os->base.lseek = patched_lseek;
    os->base.close = patched_close;
    os->nvmem_path = nvmem_path;
}

static bool write_stub_raw_value(int fd, uint16_t address, int16_t raw)
{
    union i2c_smbus_data smbus_data;
    struct i2c_smbus_ioctl_data args;
    memset(&smbus_data, 0, sizeof(smbus_data));
    memset(&args, 0, sizeof(args));

    if (ioctl(fd, I2C_SLAVE, address) < 0) return false;

    smbus_data.word = (uint16_t)raw;
    args.read_write = I2C_SMBUS_WRITE;
    args.command = TEMP_REG;
    args.size = I2C_SMBUS_WORD_DATA;
    args.data = &smbus_data;

    return ioctl(fd, I2C_SMBUS, &args) >= 0;
}

int main(void)
{
    const char* nvmem = "/tmp/demo_eeprom.nvmem";
    const char* gpio_green = TM_GPIO_GREEN;
    const char* gpio_yellow = TM_GPIO_YELLOW;
    const char* gpio_red = TM_GPIO_RED;

    printf("=== ThermalMonitor C Real-Linux Demo ===\n\n");

    if (!setup_demo_fs(nvmem)) {
        return 1;
    }

    int i2c_fd = open("/dev/i2c-1", O_RDWR);
    if (i2c_fd < 0) {
        fprintf(stderr, "ERROR: could not open /dev/i2c-1\n");
        return 1;
    }

    PatchedLinuxOs os;
    ThermalMonitor monitor;

    patched_linux_os_init(&os, nvmem);

    thermal_monitor_init(&monitor,
                         (CLinuxLikeOs*)&os,
                         100);
    atomic_store(&g_last_temp, -9999);

    printf("I2C device:  /dev/i2c-1  (real kernel i2c-stub)\n");
    printf("NVMEM:       %s  (tmpfs)\n", nvmem);
    printf("GPIO:        /sys/class/gpio/gpio1{0,1,2}/value  (kernel GPIO sysfs)\n\n");

    if (!thermal_monitor_start(&monitor, on_temperature)) {
        fprintf(stderr, "ERROR: could not start C thermal monitor\n");
        close(i2c_fd);
        return 1;
    }

    struct {
        int16_t raw;
        int16_t expected_dC;
        int green;
        int yellow;
        int red;
        const char* desc;
    } samples[] = {
        {  25,  250, 1, 0, 0, "25 degC  -> Green  (normal)" },
        {  90,  900, 0, 1, 0, "90 degC  -> Yellow (warning)" },
        { 110, 1100, 0, 0, 1, "110 degC -> Red    (critical)" },
        {   3,   30, 0, 0, 1, "3 degC   -> Red    (cold)" },
        {  50,  500, 1, 0, 0, "50 degC  -> Green  (boundary CRIT_LO)" },
    };

    bool ok = true;
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        if (!write_stub_raw_value(i2c_fd, 0x48, samples[i].raw)) {
            fprintf(stderr, "ERROR: could not write raw value %d\n", samples[i].raw);
            ok = false;
            continue;
        }

        usleep(400000);

        int last_temp = atomic_load(&g_last_temp);
        int green = linux_gpio_read_value(gpio_green);
        int yellow = linux_gpio_read_value(gpio_yellow);
        int red = linux_gpio_read_value(gpio_red);

        printf("Raw %5d  (%s)\n", samples[i].raw, samples[i].desc);
        printf("  Callback dezi-C = %d\n", last_temp);
        print_gpio("Green", gpio_green);
        print_gpio("Yellow", gpio_yellow);
        print_gpio("Red", gpio_red);

        bool sample_ok = last_temp == samples[i].expected_dC &&
                         green == samples[i].green &&
                         yellow == samples[i].yellow &&
                         red == samples[i].red;
        printf("  Verdict  = %s\n\n", sample_ok ? "PASS" : "FAIL");
        if (!sample_ok) ok = false;
    }

    thermal_monitor_stop(&monitor);
    close(i2c_fd);

    printf("Done: %s.\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
