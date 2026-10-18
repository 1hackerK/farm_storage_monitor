#include "drv_uart.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "drv_sdcard.h"
#include "tcp_client.h"
#include "drv_mqtt.h"
#include "drv_scd30.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "freertos/task.h"
#include "drv_ctrl.h"
#include "esp_netif.h"
#include "lwip/apps/sntp.h" // sntp头文件

// 引用app_main全局变量
extern char g_dev_id[16];
extern char g_addr[16];
extern float g_temp;
extern float g_humi;
extern float g_co2;
extern bool g_has_new_data;
extern ZigbeeNode_t zb_nodes[MAX_ZB_NODE];

static const char *TAG = "DRV_UART";
#define ZIGBEE_UART_NUM UART_NUM_2
#define ZIGBEE_UART_TX_PIN GPIO_NUM_46
#define ZIGBEE_UART_RX_PIN GPIO_NUM_45
#define UART_BUF_SIZE 1024

static uint8_t uart_rx_buf[UART_BUF_SIZE];
static char line_buf[256];
static int line_idx = 0;
bool uart_debug_enable = true;

QueueHandle_t zb_report_queue = NULL;
QueueHandle_t mqtt_pub_queue = NULL;


/****************************
 * MQTT独立发布任务，只负责publish，和SD/TCP完全隔离
 ****************************/
static void mqtt_publish_task(void *arg)
{
    char payload_buf[200];
    while (1)
    {
        if (xQueueReceive(mqtt_pub_queue, payload_buf, portMAX_DELAY) == pdTRUE)
        {
            esp_err_t mqtt_ret = drv_mqtt_publish(MQTT_PUB_TOPIC_DEFAULT, payload_buf, 0, false);
            if (mqtt_ret == ESP_OK)
            {
                ESP_LOGI(TAG, "[MQTT Publish OK] %s", payload_buf);
            }
            else
            {
                ESP_LOGW(TAG, "[MQTT Publish Fail]");
            }
        }
    }
    vTaskDelete(NULL);
}

/****************************
 * 上报Worker任务：SD写入 + TCP上传，打包MQTT报文丢MQTT队列
 * 不再直接调用MQTT publish
 ****************************/
static void zb_report_worker(void *arg)
{
    ZigbeeReportMsg_t msg;
    while (1)
    {
        if (xQueueReceive(zb_report_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            time_t now = time(NULL);
            struct tm t;
            localtime_r(&now, &t); // 线程安全版本，替换原来 localtime(&now)

            // 判断时间是否已经同步：年份大于2024代表NTP同步成功
            if (t.tm_year + 1900 < 2024)
            {
                ESP_LOGW(TAG, "Time not sync, skip upload & save");
                continue;
            }

            char time_str[32];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &t);
            char csv_line[160];
            int ret = snprintf(csv_line, sizeof(csv_line),
                               "%s,%s,%s,%.2f,%.2f,%.2f\n",
                               time_str, msg.dev_id, msg.addr, msg.temp, msg.humi, g_co2);
            if (ret < 0 || ret >= sizeof(csv_line))
            {
                ESP_LOGE(TAG, "csv line buffer overflow");
                continue;
            }

            // ==========【1. TCP上传】==========
            // if (tcp_client_wifi_is_connected())
            // {
            //     esp_err_t tcp_ret = tcp_client_send_data(csv_line);
            //     if (tcp_ret == ESP_OK)
            //     {
            //         ESP_LOGI(TAG, "[TCP Upload CSV OK] %s", csv_line);
            //     }
            //     else
            //     {
            //         ESP_LOGW(TAG, "[TCP Upload CSV Fail]");
            //     }
            // }
            // ==========【2. SD卡写入CSV】==========
            drv_sdcard_append_csv(csv_line);
            ESP_LOGI(TAG, "[SD Saved]");

            // ==========【3. 打包MQTT JSON，丢MQTT队列，不阻塞当前worker】==========
            char mqtt_payload[200];
            snprintf(mqtt_payload, sizeof(mqtt_payload),
                     "{\"time\":\"%s\",\"dev_id\":\"%s\",\"addr\":\"%s\",\"temp\":%.2f,\"humi\":%.2f,\"co2\":%.2f}",
                     time_str, msg.dev_id, msg.addr, msg.temp, msg.humi, g_co2);

            // 队列满直接丢弃，等待仅10ms，不阻塞SD/TCP流程
            if (xQueueSend(mqtt_pub_queue, mqtt_payload, pdMS_TO_TICKS(10)) != pdTRUE)
            {
                ESP_LOGW(TAG, "mqtt_pub_queue full, drop mqtt payload");
            }
        }
    }
    vTaskDelete(NULL);
}

/****************************
 * parse_zigbee_line：只做解析、更新节点、入zb_report_queue，无IO阻塞！
 ****************************/
static void parse_zigbee_line(char *line)
{
    char line_copy[256];
    char *p_data, *p_dev_id, *p_addr, *p_temp, *p_hum;

    if (strstr(line, "$DATA") == NULL)
    {
        return;
    }
    if (uart_debug_enable)
    {
        ESP_LOGI(TAG, "【Zigbee raw】: %s", line);
    }
    strncpy(line_copy, line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';
    p_data = strtok(line_copy, ",");
    if (!p_data)
        return;
    p_dev_id = strtok(NULL, ",");
    if (!p_dev_id)
        return;
    p_addr = strtok(NULL, ",");
    if (!p_addr)
        return;
    p_temp = strtok(NULL, ",");
    if (!p_temp)
        return;
    p_hum = strtok(NULL, "*");
    if (!p_hum)
        return;

    // 存入全局变量，给主循环控制逻辑使用
    strncpy(g_dev_id, p_dev_id, sizeof(g_dev_id) - 1);
    strncpy(g_addr, p_addr, sizeof(g_addr) - 1);
    g_temp = atof(p_temp);
    g_humi = atof(p_hum);
    g_has_new_data = true;

    // 更新 zb_nodes 节点数组
    float temp_val = atof(p_temp);
    float hum_val = atof(p_hum);
    TickType_t tick_now = xTaskGetTickCount();
    int find_idx = -1;
    for (int i = 0; i < MAX_ZB_NODE; i++)
    {
        if (zb_nodes[i].valid && strcmp(zb_nodes[i].dev_id, p_dev_id) == 0)
        {
            find_idx = i;
            break;
        }
    }
    if (find_idx >= 0)
    {
        zb_nodes[find_idx].temp = temp_val;
        zb_nodes[find_idx].humi = hum_val;
        zb_nodes[find_idx].last_recv_tick = tick_now;
        zb_nodes[find_idx].valid = true;
        ESP_LOGI(TAG, "Update node ok, temp=%.2f hum=%.2f tick=%lu", temp_val, hum_val, (unsigned long)tick_now);
    }
    else
    {
        for (int i = 0; i < MAX_ZB_NODE; i++)
        {
            if (!zb_nodes[i].valid)
            {
                find_idx = i;
                strncpy(zb_nodes[i].dev_id, p_dev_id, sizeof(zb_nodes[i].dev_id) - 1);
                zb_nodes[i].temp = temp_val;
                zb_nodes[i].humi = hum_val;
                zb_nodes[i].last_recv_tick = tick_now;
                zb_nodes[i].valid = true;
                ESP_LOGI(TAG, "New zigbee node added, idx=%d dev_id=%s", find_idx, p_dev_id);
                break;
            }
        }
        if (find_idx == -1)
        {
            ESP_LOGW(TAG, "No empty slot for new zigbee node, MAX_ZB_NODE full");
            return;
        }
    }

    // 打包消息丢给上报队列，立刻返回，不阻塞UART任务
    ZigbeeReportMsg_t msg;
    strncpy(msg.dev_id, p_dev_id, sizeof(msg.dev_id) - 1);
    strncpy(msg.addr, p_addr, sizeof(msg.addr) - 1);
    msg.temp = temp_val;
    msg.humi = hum_val;
    if (xQueueSend(zb_report_queue, &msg, pdMS_TO_TICKS(10)) != pdTRUE)
    {
        ESP_LOGW(TAG, "zb_report_queue full, drop report msg");
    }
}

/****************************
 * UART接收任务：保持轻量，只收字节拼行
 ****************************/
static void uart_recv_task(void *arg)
{
    int len;
    uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(ZIGBEE_UART_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(ZIGBEE_UART_NUM, ZIGBEE_UART_TX_PIN, ZIGBEE_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(ZIGBEE_UART_NUM, 4096, 0, 0, NULL, 0));
    ESP_LOGI(TAG, "UART2 Init OK, TX:46 RX:45, waiting zigbee data...");

    while (1)
    {
        len = uart_read_bytes(ZIGBEE_UART_NUM, uart_rx_buf, UART_BUF_SIZE, pdMS_TO_TICKS(100));
        if (len > 0)
        {
            for (int i = 0; i < len; i++)
            {
                char ch = uart_rx_buf[i];
                if (ch == '\n' || ch == '\r')
                {
                    if (line_idx > 0)
                    {
                        line_buf[line_idx] = '\0';
                        parse_zigbee_line(line_buf);
                        line_idx = 0;
                        memset(line_buf, 0, sizeof(line_buf));
                    }
                }
                else
                {
                    if (line_idx < (int)sizeof(line_buf) - 1)
                    {
                        line_buf[line_idx++] = ch;
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    vTaskDelete(NULL);
}

esp_err_t drv_uart_init(void)
{
    // 第一层队列：zigbee原始数据上报队列，深度16
    zb_report_queue = xQueueCreate(16, sizeof(ZigbeeReportMsg_t));
    if (zb_report_queue == NULL)
    {
        ESP_LOGE(TAG, "create zb_report_queue failed");
        return ESP_FAIL;
    }

    // 第二层队列：MQTT发布队列，深度16
    mqtt_pub_queue = xQueueCreate(16, sizeof(char[200]));
    if (mqtt_pub_queue == NULL)
    {
        ESP_LOGE(TAG, "create mqtt_pub_queue failed");
        vQueueDelete(zb_report_queue);
        return ESP_FAIL;
    }

    // UART接收任务：优先级5，栈4096
    xTaskCreate(uart_recv_task, "uart_recv_task", 4096, NULL, 5, NULL);
    // SD+TCP worker：优先级3，栈8192（SD写卡栈占用大）
    xTaskCreate(zb_report_worker, "zb_report_worker", 8192, NULL, 3, NULL);
    // MQTT发布独立任务：优先级5，栈4096
    xTaskCreate(mqtt_publish_task, "mqtt_pub_task", 4096, NULL, 5, NULL);

    return ESP_OK;
}
