#include "tcp_server.h"
#include "drv_sdcard.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "TCP_SERVER";
static bool wifi_connected = false;

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
        ESP_LOGI(TAG, "Server got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
    }
}

esp_err_t tcp_server_wifi_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "TCP Server WiFi started");
    return ESP_OK;
}

static void tcp_server_task(void *arg)
{
    ESP_LOGI(TAG, "TCP server task wait for wifi...");
    // 等待WiFi获取IP
    while (!wifi_connected)
    {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    ESP_LOGI(TAG, "WiFi ready, start create socket");

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        ESP_LOGE(TAG, "create socket fail, errno:%d", errno);
        vTaskDelete(NULL);
        return;
    }

    // 可选：端口复用，防止重启后端口处于TIME_WAIT无法bind
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(TCP_SERVER_LISTEN_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    int ret = bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret != 0)
    {
        ESP_LOGE(TAG, "bind fail, errno:%d", errno);
        close(server_fd);
        vTaskDelete(NULL);
        return;
    }

    ret = listen(server_fd, 1);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "listen fail, errno:%d", errno);
        close(server_fd);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "TCP Server listen port %d", TCP_SERVER_LISTEN_PORT);

    while (1)
    {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0)
        {
            ESP_LOGW(TAG, "accept fail, errno:%d", errno);
            continue;
        }
        ESP_LOGI(TAG, "new client connected, fd=%d", client_fd);

        char buf[256];
        while (1)
        {
            int r = recv(client_fd, buf, sizeof(buf) - 1, 0);
            if (r < 0)
            {
                ESP_LOGW(TAG, "recv error, errno:%d", errno);
                break;
            }
            else if (r == 0)
            {
                ESP_LOGI(TAG, "client closed connection");
                break;
            }
            buf[r] = 0;
            ESP_LOGI(TAG, "RECV DATA: %s", buf);
            drv_sdcard_append_csv(buf);
        }
        // 先shutdown，再close，避免RST发给客户端
        shutdown(client_fd, SHUT_RDWR);
        close(client_fd);
        ESP_LOGI(TAG, "client fd closed");
    }
    close(server_fd);
    vTaskDelete(NULL);
}

void tcp_server_start_task(void)
{
    xTaskCreate(tcp_server_task, "tcp_server_task", 4096, NULL, 5, NULL);
}
