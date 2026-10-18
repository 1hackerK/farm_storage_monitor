idf_component_register(SRCS "drv_sdcard.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES sdmmc fatfs vfs driver)
