idf_component_register(SRCS "drv_ssr_ptc.c" "drv_ssr_fan.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver freertos esp_common)
