#ifndef __DRV_SCD30_H
#define __DRV_SCD30_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// ========== 引脚定义 ==========
#define SCD30_I2C_SDA_PIN GPIO_NUM_14
#define SCD30_I2C_SCL_PIN GPIO_NUM_13
#define SCD30_I2C_ADDR 0x61

// 对外暴露的传感器数据
extern float scd30_co2_val;
extern float scd30_temp_val;
extern float scd30_humi_val;

esp_err_t scd30_init(void);
bool scd30_data_ready(void);
esp_err_t scd30_read_measurement(void);

#endif
