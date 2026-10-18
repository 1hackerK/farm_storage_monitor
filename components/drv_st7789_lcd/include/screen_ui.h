#ifndef __SCREEN_UI_H
#define __SCREEN_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void screen_show_sensor(uint8_t dev_id, uint8_t temp, uint8_t humi, uint8_t co2);

#ifdef __cplusplus
}
#endif

#endif
