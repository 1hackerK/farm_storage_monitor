#include "drv_ctrl.h"
#include <string.h>
#include "esp_log.h"
#include "drv_ssr_ptc.h"
#include "drv_ssr_fan.h"

static const char *TAG = "DRV_CTRL";
#define BEEP_GPIO GPIO_NUM_15

ZigbeeNode_t zb_nodes[MAX_ZB_NODE] = {0};
float g_co2 = 0.0f;

// 阈值
float temp_low = 20.0f;
float temp_high = 28.0f;
float humi_low = 40.0f;
float humi_high = 70.0f;
float co2_high = 1200.0f;

bool beep_flag = 0;

void drv_ctrl_init(void)
{
    memset(zb_nodes, 0, sizeof(zb_nodes));
    g_co2 = 0.0f;
    ESP_LOGI(TAG, "drv_ctrl init done");
}

void drv_beep_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << BEEP_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);
    // 初始状态关闭蜂鸣器，防止上电就响
    gpio_set_level(BEEP_GPIO, 0);
    ESP_LOGI("BEEP", "beep gpio init ok, gpio:%d", BEEP_GPIO);
}

void drv_beep_on(void)
{
    gpio_set_level(BEEP_GPIO, 1);
}

void drv_beep_off(void)
{
    gpio_set_level(BEEP_GPIO, 0);
}

void drv_beep_test(void)
{
    drv_beep_on();
    drv_beep_off();
}

void drv_ctrl_set_threshold(CtrlThreshold_t *thr)
{
    if (thr == NULL)
        return;

    // 参数范围保护，防止下发非法值
    if (thr->temp_low >= 0.0f && thr->temp_low <= 60.0f)
    {
        temp_low = thr->temp_low;
    }
    // 必须保证 temp_high > temp_low
    if (thr->temp_high > temp_low && thr->temp_high <= 60.0f)
    {
        temp_high = thr->temp_high;
    }
    if (thr->humi_high >= 0.0f && thr->humi_high <= 100.0f)
    {
        humi_high = thr->humi_high;
    }
    if (thr->humi_low >= 0.0f && thr->humi_low <= 100.0f)
    {
        humi_low = thr->humi_low;
    }
    if (thr->co2_high >= 400.0f && thr->co2_high <= 5000.0f)
    {
        co2_high = thr->co2_high;
    }

    ESP_LOGI(TAG, "更新阈值：temp_low=%.1f temp_high=%.1f humi_low=%.1f humi_high=%.1f co2_high=%.1f",
             temp_low, temp_high, humi_low, humi_high, co2_high);
}

void drv_ctrl_get_threshold(CtrlThreshold_t *thr)
{
    if (thr == NULL)
        return;
    thr->temp_low = temp_low;
    thr->temp_high = temp_high;
    thr->humi_low = humi_low;
    thr->humi_high = humi_high;
    thr->co2_high = co2_high;
}


void drv_ctrl_run(void)
{
    // 1. 节点超时检测
    TickType_t now = xTaskGetTickCount();
    for (int i = 0; i < MAX_ZB_NODE; i++)
    {
        if (zb_nodes[i].valid)
        {
            if ((now - zb_nodes[i].last_recv_tick) > DATA_TIMEOUT_TICK)
            {
                zb_nodes[i].valid = false;
                ESP_LOGW(TAG, "节点离线 %s", zb_nodes[i].dev_id);
            }
        }
    }

    // 2. 遍历在线节点，取最低温、最高湿
    int online_cnt = 0;
    float min_temp = 999.0f;
    float max_humi = 0.0f;
    for (int i = 0; i < MAX_ZB_NODE; i++)
    {
        if (zb_nodes[i].valid)
        {
            online_cnt++;
            if (zb_nodes[i].temp < min_temp)
                min_temp = zb_nodes[i].temp;
            if (zb_nodes[i].humi > max_humi)
                max_humi = zb_nodes[i].humi;
        }
    }
    if (online_cnt == 0)
    {
        ESP_LOGW(TAG, "无有效在线节点，跳过控制");
        return;
    }
    float t = min_temp;
    float h = max_humi;
    float c = g_co2;
    ESP_LOGI(TAG, "控制采样 minT=%.1f, maxH=%.1f, CO2=%.1f", t, h, c);

    // ========== 滞回控制逻辑 ==========
    if (t < temp_low)
    {
        drv_ssr_fan_set(SSR_FAN_OFF);
        ESP_LOGI(TAG, "温度低于下限 %.1f，开启PTC加热", temp_low);
        beep_flag = 1;
    }
    else if (t > temp_high)
    {
        drv_ssr_fan_set(SSR_FAN_ON);
        ESP_LOGI(TAG, "温度高于上限 %.1f，开启风扇降温", temp_high);
        beep_flag = 1;
    }
    else
    {
        if (drv_ssr_ptc_get_state() && t >= temp_low + HYSTERESIS)
        {
        }
        if (drv_ssr_fan_get_state() && drv_ssr_ptc_get_state())
        {
            drv_ssr_fan_set(SSR_FAN_OFF);
            ESP_LOGI(TAG, "温度回落，关闭风扇");
        }
    }

    if (h > humi_high)
    {
        drv_ssr_fan_set(SSR_FAN_ON);
        drv_ssr_ptc_set(SSR_PTC_OFF);
        ESP_LOGI(TAG, "湿度过高 %.1f，开启风扇通风", humi_high);
        beep_flag = 1;
    }
    else if (h < humi_low)
    {
        drv_ssr_ptc_set(SSR_PTC_ON);
        ESP_LOGI(TAG, "湿度低于下限 %.1f，开启加湿", humi_low);
        beep_flag = 1;
    }
    else
    {
        if (h <= humi_high - HYSTERESIS && drv_ssr_fan_get_state())
        {
            drv_ssr_fan_set(SSR_FAN_OFF);
            ESP_LOGI(TAG, "湿度回落，关闭风扇");
        }
    }

    if (c > co2_high)
    {
        drv_ssr_fan_set(SSR_FAN_ON);
        ESP_LOGI(TAG, "CO2浓度超标 %.1f，开启风扇换气", co2_high);
        beep_flag = 1;
    }
    else
    {
        if (c <= co2_high - HYSTERESIS && drv_ssr_fan_get_state())
        {
            drv_ssr_fan_set(SSR_FAN_OFF);
            ESP_LOGI(TAG, "CO2浓度回落，关闭风扇换气");
        }
    }

    if (t > temp_low && t < temp_high && h > humi_low && h < humi_high && c < co2_high)
    {
        beep_flag = 0;
    }

    if(beep_flag == 0)
    {
        drv_beep_off();
    }
    else
    {
        drv_beep_on();
    }
}

