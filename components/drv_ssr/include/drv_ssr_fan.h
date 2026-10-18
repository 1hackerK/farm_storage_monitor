#ifndef __DRV_SSR_FAN_H
#define __DRV_SSR_FAN_H

#include "driver/gpio.h"

#define SSR_FAN_GPIO GPIO_NUM_11
#define SSR_FAN_OFF 0
#define SSR_FAN_ON 1

// 初始化FAN SSR引脚
void drv_ssr_fan_init(void);
// 设置FAN SSR通断
void drv_ssr_fan_set(uint8_t state);
// FAN单独循环测试任务
void drv_ssr_fan_loop_test(void *arg);

bool drv_ssr_fan_get_state(void);

#endif
