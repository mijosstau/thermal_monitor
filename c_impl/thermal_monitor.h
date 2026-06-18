#pragma once

#include "interfaces.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TM_CRIT_LO    50
#define TM_WARN       850
#define TM_CRIT_HI    1050
#define TM_POLL_MS    100
#define TM_MAX_FILTER_SAMPLES 9

#define TM_GPIO_EXPORT     "/sys/class/gpio/export"
#define TM_GPIO_GREEN      "/sys/class/gpio/gpio10/value"
#define TM_GPIO_YELLOW     "/sys/class/gpio/gpio11/value"
#define TM_GPIO_RED        "/sys/class/gpio/gpio12/value"
#define TM_GPIO_GREEN_DIR  "/sys/class/gpio/gpio10/direction"
#define TM_GPIO_YELLOW_DIR "/sys/class/gpio/gpio11/direction"
#define TM_GPIO_RED_DIR    "/sys/class/gpio/gpio12/direction"

typedef void (*TemperatureCallback)(int16_t temp_dC);

typedef enum {
    TM_LED_GREEN = 0,
    TM_LED_YELLOW,
    TM_LED_RED_LOW,
    TM_LED_RED_HIGH,
} ThermalLedState;

typedef struct {
    uint8_t      sample_count;
    unsigned int sample_spacing_ms;
    int16_t      hysteresis_dC;
} ThermalStabilityConfig;

typedef struct {
    CLinuxLikeOs*   os;
    unsigned int    poll_ms;

    pthread_t       thread;
#ifdef __cplusplus
    int             running;
#else
    _Atomic int     running;
#endif
    bool            error_blink;
    int             i2c_fd;
    ThermalLedState led_state;
    ThermalStabilityConfig stability;

    TemperatureCallback callback;
} ThermalMonitor;

void thermal_monitor_init    (ThermalMonitor* m,
                              CLinuxLikeOs*  os,
                              unsigned int   poll_ms);
void thermal_monitor_set_stability_config(ThermalMonitor* m,
                                           ThermalStabilityConfig config);

bool thermal_monitor_start   (ThermalMonitor* m, TemperatureCallback cb);
void thermal_monitor_stop    (ThermalMonitor* m);
bool thermal_monitor_running (const ThermalMonitor* m);

#ifdef __cplusplus
}
#endif
