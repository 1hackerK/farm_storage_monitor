#include "drv_mqtt.h"
#include "mqtt_client.h"
#include "drv_ctrl.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "DRV_MQTT";
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

static int parse_threshold_json(const char *json_str, CtrlThreshold_t *thr)
{
    if (json_str == NULL || thr == NULL)
    {
        return -1;
    }
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL)
    {
        const char *err_ptr = cJSON_GetErrorPtr();
        if (err_ptr != NULL)
        {
            ESP_LOGE(TAG, "JSON parse error before: %s", err_ptr);
        }
        return -1;
    }

    cJSON *item;
    item = cJSON_GetObjectItemCaseSensitive(root, "temp_low");
    if (item != NULL && cJSON_IsNumber(item))
        thr->temp_low = (float)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "temp_high");
    if (item != NULL && cJSON_IsNumber(item))
        thr->temp_high = (float)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "humi_low");
    if (item != NULL && cJSON_IsNumber(item))
        thr->humi_low = (float)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "humi_high");
    if (item != NULL && cJSON_IsNumber(item))
        thr->humi_high = (float)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "co2_high");
    if (item != NULL && cJSON_IsNumber(item))
        thr->co2_high = (float)item->valuedouble;

    cJSON_Delete(root);
    return 0;
}

// MQTT事件回调函数
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected to broker");
        mqtt_connected = true;
        // 连接成功，自动订阅主题
        ESP_LOGI(TAG, "Subscribe topic: %s", MQTT_SUB_TOPIC);
        esp_mqtt_client_subscribe(client, MQTT_SUB_TOPIC, MQTT_QOS_0);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        mqtt_connected = false;
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT message published ok");
        break;

    // 收到订阅的消息！
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "===== Recv MQTT Data =====");
        ESP_LOGI(TAG, "Topic: %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "Payload: %.*s", event->data_len, event->data);

        // 拷贝payload并补'\0'，cJSON需要字符串结束符
        char payload_buf[256] = {0};
        if (event->data_len < (sizeof(payload_buf) - 1))
        {
            memcpy(payload_buf, event->data, event->data_len);
            payload_buf[event->data_len] = '\0';

            CtrlThreshold_t new_thr;
            if (parse_threshold_json(payload_buf, &new_thr) == 0)
            {
                ESP_LOGI(TAG, "阈值解析成功！");
                ESP_LOGI(TAG,
                         "temp_low:%.2f temp_high:%.2f humi_low:%.2f humi_high:%.2f co2_high:%.2f",
                         new_thr.temp_low,
                         new_thr.temp_high,
                         new_thr.humi_low,
                         new_thr.humi_high,
                         new_thr.co2_high);

                // 调用drv_ctrl里的函数，把新阈值更新到控制模块
                drv_ctrl_set_threshold(&new_thr);
            }
            else
            {
                ESP_LOGE(TAG, "阈值JSON解析失败");
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT event error");
        break;

    default:
        break;
    }
}

esp_err_t drv_mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = MQTT_CLIENT_ID,
        .credentials.username = MQTT_USER_NAME,
        .credentials.authentication.password = MQTT_USER_PASSWD,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL)
    {
        ESP_LOGE(TAG, "mqtt client init fail");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    ESP_LOGI(TAG, "MQTT client started");
    return ESP_OK;
}

esp_err_t drv_mqtt_publish(const char *topic, const char *payload, int qos, bool retain)
{
    if (!mqtt_connected || mqtt_client == NULL)
    {
        ESP_LOGW(TAG, "MQTT not connected, skip publish");
        return ESP_ERR_INVALID_STATE;
    }
    // payload传0，内部自动strlen
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, qos, retain);
    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "MQTT publish failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

bool drv_mqtt_is_connected(void)
{
    return mqtt_connected;
}
