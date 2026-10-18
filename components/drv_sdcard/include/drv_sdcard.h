#ifndef __DRV_SDCARD_H
#define __DRV_SDCARD_H

#include "esp_err.h"
#include <time.h>

esp_err_t drv_sdcard_init(void);
esp_err_t drv_sdcard_deinit(void);
void drv_sdcard_append_csv(const char *line);
void drv_test_write_csv(void);

#endif
