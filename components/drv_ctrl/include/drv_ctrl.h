#ifndef __DRV_CTRL_H
#define __DRV_CTRL_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>

#define MAX_ZB_NODE 8
#define DATA_TIMEOUT_TICK (pdMS_TO_TICKS(5000)) // 节点5s无数据离线
#define HYSTERESIS 1.0f                          // 数值滞回
#define CTRL_DEBOUNCE_MS 2000                    // 消抖时间，条件持续2s才切换状态

// 控制输出状态
typedef struct
{
    bool beep;
    bool fan;
    bool ptc;
} CtrlOutputState_t;

// 报警状态
typedef struct
{
    bool temp_alarm;
    bool humi_alarm;
    bool co2_alarm;
} CtrlAlarmState_t;

// 阈值
typedef struct
{
    float temp_low;
    float temp_high;
    float humi_low;
    float humi_high;
    float co2_high;
} CtrlThreshold_t;

// zigbee节点结构体
typedef struct
{
    char dev_id[16];
    float temp;
    float humi;
    bool valid;
    TickType_t last_recv_tick;
} ZigbeeNode_t;

extern float temp_low;
extern float temp_high;
extern float humi_low;
extern float humi_high;
extern float co2_high;

extern ZigbeeNode_t zb_nodes[MAX_ZB_NODE];
extern float g_co2;
extern bool g_ctrl_new_data; // 有新传感器数据标记

// 状态获取接口，方便调试/屏幕显示
void drv_ctrl_get_output_state(CtrlOutputState_t *out);
void drv_ctrl_get_alarm_state(CtrlAlarmState_t *alarm);

void drv_ctrl_init(void);
void drv_beep_init(void);
void drv_beep_on(void);
void drv_beep_off(void);
void drv_beep_test(void);
void drv_ctrl_set_threshold(CtrlThreshold_t *thr);
void drv_ctrl_get_threshold(CtrlThreshold_t *thr);
int drv_ctrl_update_zb_node(const char *dev_id, float temp, float humi);
void drv_ctrl_run(void);
#endif
