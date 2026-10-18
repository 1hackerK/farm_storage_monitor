#include "tcp_client.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/apps/sntp.h" // sntp头文件
#include <string.h>
#include <stdbool.h>
#include <time.h>

static const char *TAG = "TCP_CLIENT";
static bool wifi_connected = false;

void tcp_client_sntp_init(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    // 国内NTP服务器，速度快
    sntp_setservername(0, "ntp.aliyun.com");
    sntp_setservername(1, "ntp1.aliyun.com");
    sntp_init();

    // 设置时区：东八区北京时间 CST-8
    setenv("TZ", "CST-8", 1);
    tzset();
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "WiFi disconnected, reconnect");
        wifi_connected = false;
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
    }
}

esp_err_t tcp_client_wifi_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_cfg = {.sta = {.ssid = WIFI_SSID, .password = WIFI_PASSWORD}};
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "TCP Client WiFi started");
    return ESP_OK;
}

bool tcp_client_wifi_is_connected(void)
{
    return wifi_connected;
}

esp_err_t tcp_client_send_data(const char *data)
{
    if (!wifi_connected)
    {
        ESP_LOGW(TAG, "WiFi offline");
        return ESP_ERR_NOT_SUPPORTED;
    }
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "socket create fail");
        return ESP_FAIL;
    }
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(TCP_SERVER_PORT);
    dest.sin_addr.s_addr = inet_addr(TCP_SERVER_IP);
    int ret = connect(sock, (struct sockaddr *)&dest, sizeof(dest));
    if (ret != 0)
    {
        ESP_LOGE(TAG, "connect server failed, errno: %d", errno);
        close(sock);
        return ESP_FAIL;
    }
    ret = send(sock, data, strlen(data), 0);
    if (ret < 0)
    {
        ESP_LOGE(TAG, "send failed, errno: %d", errno);
        close(sock);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "send ok: %s", data);
    close(sock);
    return ESP_OK;
}

// 获取格式化时间字符串 YYYY-MM-DD HH:MM:SS
bool tcp_client_get_time_str(char *time_buf, int buf_len)
{
    if (time_buf == NULL || buf_len < 20)
    {
        return false;
    }
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    int len = strftime(time_buf, buf_len, "%Y-%m-%d %H:%M:%S", &tm_now);
    return (len > 0);
}