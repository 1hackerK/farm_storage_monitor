#include "drv_ssr_ptc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "SSR_PTC";
static bool ptc_state = false;

void drv_ssr_ptc_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SSR_PTC_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);

    gpio_set_level(SSR_PTC_GPIO, SSR_PTC_OFF);
    ESP_LOGI(TAG, "PTC SSR IO9 init OK, default OFF");
}

void drv_ssr_ptc_set(uint8_t state)
{
    gpio_set_level(SSR_PTC_GPIO, state);
    ptc_state = state ? true : false;
    ESP_LOGI(TAG, "PTC SSR: %s", state ? "ON" : "OFF");
}

void drv_ssr_ptc_loop_test(void *arg)
{
    while (1)
    {
        drv_ssr_ptc_set(SSR_PTC_ON);
        vTaskDelay(pdMS_TO_TICKS(1000));
        drv_ssr_ptc_set(SSR_PTC_OFF);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

bool drv_ssr_ptc_get_state(void)
{
    return ptc_state; // 返回你保存的PTC继电器状态变量
}
