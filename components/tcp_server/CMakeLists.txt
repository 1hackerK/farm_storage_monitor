idf_component_register(SRCS "tcp_server.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash esp_wifi esp_netif lwip log freertos drv_sdcard)
