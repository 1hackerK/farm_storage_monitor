idf_component_register(SRCS "tcp_client.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash esp_wifi esp_netif lwip log)
