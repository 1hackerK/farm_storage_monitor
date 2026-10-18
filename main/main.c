#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "drv_ctrl.h"
#include "esp_sntp.h"
#include "esp_netif.h"
// ===================== TCP 模式选择，二选一 =====================
#define USE_TCP_CLIENT 1
#define USE_TCP_SERVER 0
// ===============================================================
#if USE_TCP_CLIENT
#include "drv_st7789.h"
#include "screen_ui.h"

#include "drv_sdcard.h"
#include "drv_ssr_ptc.h"
#include "drv_ssr_fan.h"
#include "drv_scd30.h"
#include "drv_uart.h"
#include "tcp_client.h"
#include "drv_mqtt.h"
#endif
#if USE_TCP_SERVER
#include "tcp_server.h"
#endif

char g_dev_id[16];
char g_addr[16];
float g_temp;
float g_humi;

bool g_has_new_data = false;
extern float g_co2;
extern ZigbeeNode_t zb_nodes[MAX_ZB_NODE];



void app_main(void)
{
#if USE_TCP_SERVER
    ESP_LOGI("MAIN", "===== 运行 TCP 服务端（上位接收） =====");
    tcp_server_wifi_init();
    tcp_server_start_task();
    return;

#endif
#if USE_TCP_CLIENT
    ESP_LOGI("MAIN", "===== 运行 TCP 客户端（下位采集上传） =====");
    drv_ctrl_init(); // 初始化控制器组件

    drv_ssr_ptc_init();
    drv_ssr_fan_init();
    drv_beep_init();

    printf("===== ESP32S3 SDMMC 1bit 启动 =====\n");
    esp_err_t ret = drv_sdcard_init();
    if (ret == ESP_OK)
    {
        drv_test_write_csv();
    }
    else
    {
        printf("SD卡初始化失败，不再执行读写测试\n");
    }

    drv_uart_init();
    tcp_client_wifi_init();

    ESP_LOGI("MAIN", "等待WiFi连接...");
    uint32_t wait_ms = 0;
    const uint32_t MAX_WAIT_MS = 10000;
    while (!tcp_client_wifi_is_connected() && wait_ms < MAX_WAIT_MS)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_ms += 100;
    }
    if (tcp_client_wifi_is_connected())
    {
        ESP_LOGI("MAIN", "✅ WiFi已获取IP，启动SNTP和MQTT");
        tcp_client_sntp_init();
        esp_sntp_init();
        drv_mqtt_init();
    }
    else
    {
        ESP_LOGE("MAIN", "❌ WiFi连接超时，不启动SNTP和MQTT");
    }

    uart_debug_enable = true;
    printf("UART调试已开启，等待Zigbee真实数据...\n");

    esp_err_t ret_st7789 = st7789_init();
    if (ret_st7789 != ESP_OK)
    {
        printf("ST7789 init failed! err:%d\n", ret_st7789);
        return;
    }
    printf("ST7789 init OK\n");

    esp_err_t ret_scd30 = scd30_init();
    if (ret_scd30 == ESP_OK)
    {
        printf("SCD30 init OK, IO13=SCL IO14=SDA\n");
    }
    else
    {
        printf("SCD30 init failed! err:%d\n", ret_scd30);
    }

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));

        // 读取SCD30 CO2，直接赋值给组件全局变量g_co2
        if (scd30_data_ready())
        {
            if (scd30_read_measurement() == ESP_OK)
            {
                g_co2 = scd30_co2_val;           
            }
        }
        drv_ctrl_run();

        // ========== 屏幕刷新：遍历zb_nodes找第一个有效节点显示 ==========

        int show_idx = -1;
        int16_t disp_temp = 0;
        int16_t disp_humi = 0;
        int16_t disp_co2 = (int16_t)g_co2;

        st7789_fill(0, 0, 239, 319, BLACK);
        for (int i = 0; i < MAX_ZB_NODE; i++)
        {
            if (zb_nodes[i].valid)
            {
                show_idx = i;
                disp_temp = (int16_t)zb_nodes[show_idx].temp;
                disp_humi = (int16_t)zb_nodes[show_idx].humi;
                break;
            }
        }
        screen_show_sensor(0, disp_temp, disp_humi, disp_co2);
    }
#endif
}
