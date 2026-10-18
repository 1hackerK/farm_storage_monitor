#include "drv_ssr_fan.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "SSR_FAN";
static bool g_fan_state = false;

void drv_ssr_fan_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SSR_FAN_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);

    gpio_set_level(SSR_FAN_GPIO, SSR_FAN_OFF);
    ESP_LOGI(TAG, "FAN SSR IO11 init OK, default OFF");
}

void drv_ssr_fan_set(uint8_t state)
{
    gpio_set_level(SSR_FAN_GPIO, state);
    g_fan_state = state ? true : false;
    ESP_LOGI(TAG, "FAN SSR: %s", state ? "ON" : "OFF");
}

void drv_ssr_fan_loop_test(void *arg)
{
    while (1)
    {
        drv_ssr_fan_set(SSR_FAN_ON);
        vTaskDelay(pdMS_TO_TICKS(10000));
        drv_ssr_fan_set(SSR_FAN_OFF);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

bool drv_ssr_fan_get_state(void)
{
    return g_fan_state;
}