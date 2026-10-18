#ifndef __DRV_UART_H
#define __DRV_UART_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// 上报消息结构体：UART解析完成后打包发给worker
typedef struct
{
    char dev_id[16];
    char addr[16];
    float temp;
    float humi;
} ZigbeeReportMsg_t;

extern QueueHandle_t zb_report_queue;
extern QueueHandle_t mqtt_pub_queue;
extern bool uart_debug_enable;

esp_err_t drv_uart_init(void);

#endif
