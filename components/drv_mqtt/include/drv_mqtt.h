#ifndef __DRV_MQTT_H
#define __DRV_MQTT_H

#include "esp_err.h"
#include <stdbool.h>

// ================= MQTT 参数宏定义，统一在这里修改 =================
#define MQTT_BROKER_URI "mqtt://120.55.84.46:1883"
#define MQTT_CLIENT_ID "111"
#define MQTT_USER_NAME ""   // 无用户名填空
#define MQTT_USER_PASSWD "" // 无密码填空

// 订阅主题：ESP32订阅这个主题，接收服务器下发指令
#define MQTT_SUB_TOPIC "threshold"
// 默认发布主题（测试用，调用publish时也可以自定义其他主题）
#define MQTT_PUB_TOPIC_DEFAULT "monitor"

#define MQTT_QOS_0 0
#define MQTT_QOS_1 1
#define MQTT_QOS_2 2
// ==================================================================

esp_err_t drv_mqtt_init(void);
esp_err_t drv_mqtt_publish(const char *topic, const char *payload, int qos, bool retain);
bool drv_mqtt_is_connected(void);

#endif
