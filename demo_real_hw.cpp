#include "thermal_monitor.h"
#include "platform/real_linux_os.hpp"
#include "platform/i2c/i2c_types.hpp"

#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>

// Prepare the fake NVMEM file. GPIO is expected to be provided by the kernel.
static bool setup_demo_fs(const char* nvmem_path)
{
    // NVMEM: TypeA = 0x00
    int fd = ::open(nvmem_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open nvmem"); return false; }
    uint8_t type_a = 0x00;
    ssize_t written = ::write(fd, &type_a, 1);
    ::close(fd);
    return written == 1;
}

static void print_gpio(const char* label, const char* path) {
    int fd = ::open(path, O_RDONLY);
    if (fd < 0) return;
    char buf[4] = {};
    ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
    ::close(fd);
    if (n < 1) return;
    buf[strcspn(buf, "\n")] = '\0';
    printf("  %-8s = %s  (%s)\n", label, buf, path);
}

static int read_gpio_value(const char* path) {
    int fd = ::open(path, O_RDONLY);
    if (fd < 0) return -1;
    char value = '\0';
    ssize_t n = ::read(fd, &value, 1);
    ::close(fd);
    if (n < 1) return -1;
    return value == '1' ? 1 : 0;
}

static bool write_stub_raw_value(int16_t raw) {
    int i2c_fd = ::open("/dev/i2c-1", O_RDWR);
    if (i2c_fd < 0) {
        perror("open /dev/i2c-1");
        return false;
    }

    if (::ioctl(i2c_fd, I2C_SLAVE, reinterpret_cast<void*>(static_cast<uintptr_t>(0x48))) < 0) {
        perror("ioctl I2C_SLAVE");
        ::close(i2c_fd);
        return false;
    }

    i2c_smbus_data smbusData{};
    smbusData.word = static_cast<uint16_t>(raw);
    i2c_smbus_ioctl_data smbusArgs{
        I2C_SMBUS_WRITE,
        0x00,
        I2C_SMBUS_WORD_DATA,
        &smbusData
    };

    if (::ioctl(i2c_fd, I2C_SMBUS, &smbusArgs) < 0) {
        perror("ioctl I2C_SMBUS write word");
        ::close(i2c_fd);
        return false;
    }

    ::close(i2c_fd);
    return true;
}

int main() {
    printf("=== ThermalMonitor Real-Linux Demo ===\n\n");

    // Temporary NVMEM path; GPIO uses the real kernel sysfs paths.
    const char* NVMEM  = "/tmp/demo_eeprom.nvmem";
    const char* G_GRN  = "/sys/class/gpio/gpio10/value";
    const char* G_YEL  = "/sys/class/gpio/gpio11/value";
    const char* G_RED  = "/sys/class/gpio/gpio12/value";

    if (!setup_demo_fs(NVMEM)) return 1;

    // RealLinuxOs: every open/read/write/ioctl/lseek/close is a real syscall
    RealLinuxOs realOs;

    // Wrap RealLinuxOs and redirect only NVMEM to a temporary file.
    // /dev/i2c-1 passes through to the real kernel i2c-stub device.
    struct PatchedOs : public ILinuxLikeOs {
        RealLinuxOs real;

        std::string translate(const std::string& path) {
            if (path == "/sys/bus/nvmem/devices/eeprom0/nvmem") return "/tmp/demo_eeprom.nvmem";
            return path;  // /dev/i2c-1 passes through to real kernel
        }
        Fd      open (const std::string& path, OpenFlags f) override { return real.open(translate(path), f); }
        ssize_t read (Fd fd, void* b, size_t n)             override { return real.read(fd, b, n); }
        ssize_t write(Fd fd, const void* b, size_t n)       override { return real.write(fd, b, n); }
        int     ioctl(Fd fd, unsigned long req, void* arg)  override { return real.ioctl(fd, req, arg); }
        off_t   lseek(Fd fd, off_t off, int wh)             override { return real.lseek(fd, off, wh); }
        int     close(Fd fd)                                override { return real.close(fd); }
    } os;

    printf("I2C device:  /dev/i2c-1  (real kernel i2c-stub)\n");
    printf("NVMEM:       %s  (tmpfs)\n", NVMEM);
    printf("GPIO:        /sys/class/gpio/gpio1{0,1,2}/value  (kernel GPIO sysfs)\n\n");

    ThermalMonitor monitor(os, std::chrono::milliseconds{200});

    int16_t last_temp = -9999;
    monitor.setOnTemperatureCallback([&](int16_t t) { last_temp = t; });

    if (!monitor.start()) {
        fprintf(stderr, "ERROR: could not open /dev/i2c-1 — is i2c-stub loaded?\n");
        fprintf(stderr, "Run: sudo modprobe i2c-stub chip_addr=0x48 && sudo modprobe i2c-dev\n");
        return 1;
    }

    printf("Monitor running. Reading from /dev/i2c-1 (I2C_RDWR ioctl)...\n\n");

    // Simulate changing temperature via i2cset values
    struct {
        int16_t raw;
        int16_t expected_dC;
        int green;
        int yellow;
        int red;
        const char* desc;
    } temps[] = {
        {  25,  250, 1, 0, 0, "25 degC  -> Green  (normal)"},
        {  90,  900, 0, 1, 0, "90 degC  -> Yellow (warning)"},
        { 110, 1100, 0, 0, 1, "110 degC -> Red    (critical)"},
        {   3,   30, 0, 0, 1, "3 degC   -> Red    (cold)"},
        {  50,  500, 1, 0, 0, "50 degC  -> Green  (boundary CRIT_LO)"},
    };

    bool ok = true;
    for (auto& t : temps) {
        if (!write_stub_raw_value(t.raw)) {
            ok = false;
            continue;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{400});

        printf("Raw %5d  (%s)\n", t.raw, t.desc);
        printf("  Callback dezi-C = %d\n", last_temp);
        print_gpio("Green",  G_GRN);
        print_gpio("Yellow", G_YEL);
        print_gpio("Red",    G_RED);

        int green = read_gpio_value(G_GRN);
        int yellow = read_gpio_value(G_YEL);
        int red = read_gpio_value(G_RED);
        bool sampleOk = last_temp == t.expected_dC &&
                        green == t.green &&
                        yellow == t.yellow &&
                        red == t.red;
        printf("  Verdict  = %s\n", sampleOk ? "PASS" : "FAIL");
        if (!sampleOk) ok = false;
        printf("\n");
    }

    monitor.stop();
    printf("Done: %s.\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
