#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__
#include "esp_err.h"



#define WIFI_SSID "lll"
#define WIFI_PASSWORD "Aa13579@"
#define TCP_SERVER_LISTEN_PORT 8080

esp_err_t tcp_server_wifi_init(void);
void tcp_server_start_task(void);

#endif
