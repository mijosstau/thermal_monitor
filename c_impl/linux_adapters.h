#pragma once

#include "interfaces.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    CLinuxLikeOs base;
} LinuxOs;

void linux_os_init(LinuxOs* os);
int  linux_gpio_read_value(const char* path);

#ifdef __cplusplus
}
#endif
