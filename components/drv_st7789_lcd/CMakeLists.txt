idf_component_register(
    SRCS "drv_st7789.c" "fonts.c" "screen_ui.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES driver freertos drv_ctrl
)
