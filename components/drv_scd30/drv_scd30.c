#include "drv_scd30.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

// 全局变量，UI层直接读取
float scd30_co2_val = 0.0f;
float scd30_temp_val = 0.0f;
float scd30_humi_val = 0.0f;

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

// SCD30 I2C命令
#define SCD30_CMD_START_CONTINUOUS_MEAS 0x0010
#define SCD30_CMD_GET_DATA_READY 0x0202
#define SCD30_CMD_READ_MEASUREMENT 0x0300
#define SCD30_CMD_SET_MEAS_INTERVAL 0x4600
#define SCD30_CMD_STOP_MEAS 0x0104
#define SCD30_CMD_SOFT_RESET 0xD304

static uint8_t crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t bit = 8; bit > 0; bit--)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}

static esp_err_t scd30_write_cmd(uint16_t cmd, uint16_t arg)
{
    uint8_t buf[5];
    buf[0] = cmd >> 8;
    buf[1] = cmd & 0xFF;
    buf[2] = arg >> 8;
    buf[3] = arg & 0xFF;
    buf[4] = crc8(&buf[2], 2);

    return i2c_master_write_to_device(I2C_MASTER_NUM, SCD30_I2C_ADDR, buf, 5, pdMS_TO_TICKS(100));
}

static esp_err_t scd30_read_reg(uint16_t cmd, uint8_t *data, size_t len)
{
    uint8_t cmd_buf[2] = {cmd >> 8, cmd & 0xFF};
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, SCD30_I2C_ADDR, cmd_buf, 2, pdMS_TO_TICKS(100));
    if (ret != ESP_OK)
        return ret;
    vTaskDelay(pdMS_TO_TICKS(5));
    return i2c_master_read_from_device(I2C_MASTER_NUM, SCD30_I2C_ADDR, data, len, pdMS_TO_TICKS(100));
}

esp_err_t scd30_init(void)
{
    // I2C初始化，SDA=IO14，SCL=IO13
    i2c_port_t i2c_port = I2C_MASTER_NUM;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SCD30_I2C_SDA_PIN,
        .scl_io_num = SCD30_I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t ret = i2c_param_config(i2c_port, &conf);
    if (ret != ESP_OK)
        return ret;
    ret = i2c_driver_install(i2c_port, conf.mode, 0, 0, 0);
    if (ret != ESP_OK)
        return ret;

    // 设置测量周期2秒（SCD30最小2s）
    scd30_write_cmd(SCD30_CMD_SET_MEAS_INTERVAL, 5);
    vTaskDelay(pdMS_TO_TICKS(50));
    // 启动连续测量，0=关闭气压补偿
    scd30_write_cmd(SCD30_CMD_START_CONTINUOUS_MEAS, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

bool scd30_data_ready(void)
{
    uint8_t data[3];
    esp_err_t ret = scd30_read_reg(SCD30_CMD_GET_DATA_READY, data, 3);
    if (ret != ESP_OK)
        return false;
    uint16_t ready = (data[0] << 8) | data[1];
    return (ready == 1);
}

esp_err_t scd30_read_measurement(void)
{
    uint8_t data[18];
    esp_err_t ret = scd30_read_reg(SCD30_CMD_READ_MEASUREMENT, data, 18);
    if (ret != ESP_OK)
        return ret;

    // 校验CRC
    for (int i = 0; i < 18; i += 3)
    {
        if (crc8(&data[i], 2) != data[i + 2])
        {
            return ESP_ERR_INVALID_CRC;
        }
    }

    // CO2 float
    uint32_t co2_raw = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[3] << 8) | data[4];
    scd30_co2_val = *(float *)&co2_raw;

    // Temperature float
    uint32_t temp_raw = ((uint32_t)data[6] << 24) | ((uint32_t)data[7] << 16) | ((uint32_t)data[9] << 8) | data[10];
    scd30_temp_val = *(float *)&temp_raw;

    // Humidity float
    uint32_t humi_raw = ((uint32_t)data[12] << 24) | ((uint32_t)data[13] << 16) | ((uint32_t)data[15] << 8) | data[16];
    scd30_humi_val = *(float *)&humi_raw;

    return ESP_OK;
}
