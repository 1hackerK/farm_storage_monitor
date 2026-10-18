idf_component_register(SRCS "drv_uart.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver sdmmc fatfs vfs log freertos drv_sdcard tcp_client drv_mqtt drv_scd30 drv_ssr drv_ctrl esp_netif lwip)

