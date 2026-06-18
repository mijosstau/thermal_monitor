#include "thermal_monitor.h"

#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TM_SENSOR_ADDR 0x48
#define TM_TEMP_REG    0x00
#define TM_I2C_DEVICE  "/dev/i2c-1"
#define TM_NVMEM_PATH  "/sys/bus/nvmem/devices/eeprom0/nvmem"

static int16_t convert(int16_t raw, CSensorType type) {
    if (type == SENSOR_TYPE_A) return (int16_t)(raw * 10);
    return raw;
}

static void gpio_setup(ThermalMonitor* m) {
    static const struct { const char* num; const char* dir; } gpios[3] = {
        { "10\n", TM_GPIO_GREEN_DIR  },
        { "11\n", TM_GPIO_YELLOW_DIR },
        { "12\n", TM_GPIO_RED_DIR    },
    };
    for (int i = 0; i < 3; ++i) {
        int fd = m->os->open(m->os, TM_GPIO_EXPORT, C_OPEN_WRITE_ONLY);
        if (fd >= 0) { m->os->write(m->os, fd, gpios[i].num, 3); m->os->close(m->os, fd); }
        fd = m->os->open(m->os, gpios[i].dir, C_OPEN_WRITE_ONLY);
        if (fd >= 0) { m->os->write(m->os, fd, "out\n", 4); m->os->close(m->os, fd); }
    }
}

static void gpio_write(ThermalMonitor* m, const char* path, int value) {
    int fd = m->os->open(m->os, path, C_OPEN_WRITE_ONLY);
    if (fd >= 0) {
        const char v[2] = { value ? '1' : '0', '\n' };
        m->os->write(m->os, fd, v, 2);
        m->os->close(m->os, fd);
    }
}

static void set_leds(ThermalMonitor* m, int green, int yellow, int red) {
    gpio_write(m, TM_GPIO_GREEN,  green);
    gpio_write(m, TM_GPIO_YELLOW, yellow);
    gpio_write(m, TM_GPIO_RED,    red);
}

static void evaluate(ThermalMonitor* m, int16_t temp_dC) {
    const int16_t h = m->stability.hysteresis_dC;

    switch (m->led_state) {
        case TM_LED_RED_HIGH:
            if (temp_dC >= (int16_t)(TM_CRIT_HI - h)) break;
            goto classify;
        case TM_LED_RED_LOW:
            if (temp_dC < (int16_t)(TM_CRIT_LO + h)) break;
            goto classify;
        case TM_LED_YELLOW:
            if (temp_dC >= (int16_t)(TM_WARN - h) && temp_dC < TM_CRIT_HI) break;
            goto classify;
        case TM_LED_GREEN:
            goto classify;
    }

    goto apply;

classify:
    if (temp_dC >= TM_CRIT_HI) m->led_state = TM_LED_RED_HIGH;
    else if (temp_dC < TM_CRIT_LO) m->led_state = TM_LED_RED_LOW;
    else if (temp_dC >= TM_WARN) m->led_state = TM_LED_YELLOW;
    else m->led_state = TM_LED_GREEN;

apply:
    switch (m->led_state) {
        case TM_LED_GREEN:
            set_leds(m, 1, 0, 0);
            break;
        case TM_LED_YELLOW:
            set_leds(m, 0, 1, 0);
            break;
        case TM_LED_RED_LOW:
        case TM_LED_RED_HIGH:
            set_leds(m, 0, 0, 1);
            break;
    }
}

static void set_error_state(ThermalMonitor* m) {
    m->error_blink = !m->error_blink;
    set_leds(m, 0, 0, m->error_blink ? 1 : 0);
    if (m->callback) m->callback(-9999);
}

static void sleep_ms(unsigned int ms) {
    struct timespec ts = {
        .tv_sec  = (time_t)(ms / 1000),
        .tv_nsec = (long)(ms % 1000) * 1000000L
    };
    nanosleep(&ts, NULL);
}

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000U) + ((uint64_t)ts.tv_nsec / 1000000U);
}

static int read_sensor_type(ThermalMonitor* m, CSensorType* out) {
    int fd = m->os->open(m->os, TM_NVMEM_PATH, C_OPEN_READ_ONLY);
    if (fd < 0) return -1;

    m->os->lseek(m->os, fd, 0, SEEK_SET);
    uint8_t byte = 0;
    ssize_t n = m->os->read(m->os, fd, &byte, 1);
    m->os->close(m->os, fd);

    if (n < 1) return -1;

    *out = byte == (uint8_t)SENSOR_TYPE_B ? SENSOR_TYPE_B : SENSOR_TYPE_A;
    return 0;
}

static int read_raw_i2c_rdwr(ThermalMonitor* m, int16_t* out) {
    uint8_t reg = TM_TEMP_REG;
    uint8_t buf[2] = {0, 0};
    struct i2c_msg msgs[2] = {
        { .addr = TM_SENSOR_ADDR, .flags = 0, .len = 1, .buf = &reg },
        { .addr = TM_SENSOR_ADDR, .flags = I2C_M_RD, .len = 2, .buf = buf },
    };
    struct i2c_rdwr_ioctl_data data = {
        .msgs = msgs,
        .nmsgs = 2,
    };

    if (m->os->ioctl(m->os, m->i2c_fd, I2C_RDWR, &data) < 0) return -1;
    *out = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    return 0;
}

static int read_raw_smbus(ThermalMonitor* m, int16_t* out) {
    union i2c_smbus_data smbus_data;
    struct i2c_smbus_ioctl_data args;
    memset(&smbus_data, 0, sizeof(smbus_data));
    memset(&args, 0, sizeof(args));

    if (m->os->ioctl(m->os, m->i2c_fd, I2C_SLAVE, (void*)(uintptr_t)TM_SENSOR_ADDR) < 0)
        return -1;

    args.read_write = I2C_SMBUS_READ;
    args.command = TM_TEMP_REG;
    args.size = I2C_SMBUS_WORD_DATA;
    args.data = &smbus_data;

    if (m->os->ioctl(m->os, m->i2c_fd, I2C_SMBUS, &args) < 0) return -1;
    *out = (int16_t)smbus_data.word;
    return 0;
}

static int read_raw_from_i2c(ThermalMonitor* m, int16_t* out) {
    if (read_raw_i2c_rdwr(m, out) == 0) return 0;
    if (read_raw_smbus(m, out) == 0) return 0;
    return -1;
}

static int16_t median(int16_t* values, size_t count) {
    for (size_t i = 1; i < count; ++i) {
        int16_t key = values[i];
        size_t j = i;
        while (j > 0 && values[j - 1] > key) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = key;
    }
    for (size_t i = 1; i < count; ++i) {
        assert(values[i - 1] <= values[i]);
    }
    return values[count / 2];
}

static int read_filtered_temperature(ThermalMonitor* m, CSensorType type, int16_t* out) {
    int16_t values[TM_MAX_FILTER_SAMPLES] = {0};
    size_t count = m->stability.sample_count;
    if (count < 1) count = 1;
    if (count > TM_MAX_FILTER_SAMPLES) count = TM_MAX_FILTER_SAMPLES;

    for (size_t i = 0; i < count; ++i) {
        int16_t raw = 0;
        if (read_raw_from_i2c(m, &raw) != 0) return -1;
        values[i] = convert(raw, type);

        if (i + 1 < count && m->stability.sample_spacing_ms > 0) {
            const uint64_t deadline = now_ms() + m->stability.sample_spacing_ms;
            while (atomic_load(&m->running) && now_ms() < deadline) {
                sleep_ms(1);
            }
            if (!atomic_load(&m->running)) return -1;
        }
    }

    *out = median(values, count);
    return 0;
}

static void* polling_loop(void* arg) {
    ThermalMonitor* m = (ThermalMonitor*)arg;

    CSensorType type;
    bool type_ok = (read_sensor_type(m, &type) == 0);

    while (atomic_load(&m->running)) {
        const uint64_t loop_start = now_ms();

        if (!type_ok) {
            set_error_state(m);
        } else {
            int16_t temp_dC = 0;
            if (read_filtered_temperature(m, type, &temp_dC) != 0) {
                set_error_state(m);
            } else {
                evaluate(m, temp_dC);
                if (m->callback) m->callback(temp_dC);
            }
        }

        const uint64_t elapsed = now_ms() - loop_start;
        if (elapsed < m->poll_ms) sleep_ms((unsigned int)(m->poll_ms - elapsed));
    }

    return NULL;
}

void thermal_monitor_init(ThermalMonitor* m,
                          CLinuxLikeOs*   os,
                          unsigned int    poll_ms) {
    m->os          = os;
    m->poll_ms     = poll_ms ? poll_ms : TM_POLL_MS;
    m->callback    = NULL;
    m->error_blink = false;
    m->i2c_fd      = -1;
    m->led_state   = TM_LED_GREEN;
    m->stability.sample_count = 1;
    m->stability.sample_spacing_ms = 0;
    m->stability.hysteresis_dC = 0;
    atomic_store(&m->running, 0);
}

void thermal_monitor_set_stability_config(ThermalMonitor* m,
                                           ThermalStabilityConfig config) {
    if (config.sample_count == 0) config.sample_count = 1;
    if (config.sample_count > TM_MAX_FILTER_SAMPLES)
        config.sample_count = TM_MAX_FILTER_SAMPLES;
    if (config.hysteresis_dC < 0) config.hysteresis_dC = 0;
    m->stability = config;
}

bool thermal_monitor_start(ThermalMonitor* m, TemperatureCallback cb) {
    if (atomic_load(&m->running)) return true;
    atomic_store(&m->running, 1);
    m->callback = cb;

    m->i2c_fd = m->os->open(m->os, TM_I2C_DEVICE, C_OPEN_READ_WRITE);
    if (m->i2c_fd < 0) {
        atomic_store(&m->running, 0);
        return false;
    }

    gpio_setup(m);
    if (pthread_create(&m->thread, NULL, polling_loop, m) != 0) {
        m->os->close(m->os, m->i2c_fd);
        m->i2c_fd = -1;
        atomic_store(&m->running, 0);
        return false;
    }
    return true;
}

void thermal_monitor_stop(ThermalMonitor* m) {
    if (!atomic_load(&m->running)) return;
    atomic_store(&m->running, 0);
    pthread_join(m->thread, NULL);
    if (m->i2c_fd >= 0) {
        m->os->close(m->os, m->i2c_fd);
        m->i2c_fd = -1;
    }
}

bool thermal_monitor_running(const ThermalMonitor* m) {
    return atomic_load(&m->running) != 0;
}
