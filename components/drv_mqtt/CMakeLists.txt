idf_component_register(SRCS "drv_mqtt.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_event log mqtt drv_ctrl cjson
                    )

include_directories(${PROJECT_DIR}/managed_components/espressif__cjson/cJSON)