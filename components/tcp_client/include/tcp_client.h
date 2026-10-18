#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__
#include "esp_err.h"
#include <stdbool.h>

#define WIFI_SSID "lll"
#define WIFI_PASSWORD "Aa13579@"
#define TCP_SERVER_IP "10.104.43.223"
#define TCP_SERVER_PORT 8080

void tcp_client_sntp_init(void);
esp_err_t tcp_client_wifi_init(void);
bool tcp_client_wifi_is_connected(void);
esp_err_t tcp_client_send_data(const char *data);
bool tcp_client_get_time_str(char *time_buf, int buf_len);

#endif
