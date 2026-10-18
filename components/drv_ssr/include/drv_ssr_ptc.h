#ifndef __DRV_SSR_PTC_H
#define __DRV_SSR_PTC_H

#include "driver/gpio.h"

#define SSR_PTC_GPIO GPIO_NUM_9
#define SSR_PTC_OFF 0
#define SSR_PTC_ON 1

// 初始化PTC SSR引脚
void drv_ssr_ptc_init(void);
// 设置PTC SSR通断
void drv_ssr_ptc_set(uint8_t state);
// PTC单独循环测试任务
void drv_ssr_ptc_loop_test(void *arg);

bool drv_ssr_ptc_get_state(void);

#endif
