#include "screen_ui.h"
#include "drv_st7789.h"
#include "drv_ctrl.h"

static int curr_show_node = 0;
static TickType_t last_switch_tick = 0;
#define NODE_SWITCH_INTERVAL pdMS_TO_TICKS(2000)

void screen_show_sensor(uint8_t dev_id, uint8_t temp, uint8_t humi, uint8_t co2)
{
    TickType_t now = xTaskGetTickCount();
    int valid_node_list[MAX_ZB_NODE] = {0};
    int valid_cnt = 0;
    int16_t disp_co2 = (int16_t)g_co2;
    char buf_co2[32];

    for (int i = 0; i < MAX_ZB_NODE; i++)
    {
        if (zb_nodes[i].valid)
        {
            valid_node_list[valid_cnt++] = i;
        }
    }
    if (valid_cnt > 0)
    {
        if (curr_show_node >= valid_cnt)
        {
            curr_show_node = 0;
            last_switch_tick = now;
        }
    }
    else
    {
        curr_show_node = 0; // 无节点时重置索引
    }
    st7789_fill(0, 0, 239, 319, BLACK);

    if (valid_cnt == 0)
    {
        st7789_fill(0, 0, 239, 319, BLACK);
        st7789_write_string(10, 10, "FARM STORAGE MONITOR", Font_11x18, WHITE, BLACK);
        st7789_write_string(10, 60, "No Online Node", Font_11x18, RED, BLACK);
        sprintf(buf_co2, "CO2: %d ppm", disp_co2);
        st7789_write_string(10, 130, buf_co2, Font_11x18, MAGENTA, BLACK);
        return;
    }

    // 2秒切换到下一个在线节点
    if (now - last_switch_tick >= NODE_SWITCH_INTERVAL)
    {
        last_switch_tick = now;
        curr_show_node++;
        if (curr_show_node >= valid_cnt)
        {
            curr_show_node = 0;
        }
    }

    int node_idx = valid_node_list[curr_show_node];
    ZigbeeNode_t *node = &zb_nodes[node_idx];

    char buf[64];

    st7789_write_string(10, 10, "Farm Storage Monitor", Font_11x18, WHITE, BLACK);

    snprintf(buf, sizeof(buf), "DEV ID: %s", node->dev_id);
    st7789_write_string(10, 40, buf, Font_11x18, CYAN, BLACK);

    snprintf(buf, sizeof(buf), "TEMP: %.1f C", node->temp);
    st7789_write_string(10, 70, buf, Font_11x18, YELLOW, BLACK);

    snprintf(buf, sizeof(buf), "HUMI: %.1f %%RH", node->humi);
    st7789_write_string(10, 100, buf, Font_11x18, GREEN, BLACK);

    snprintf(buf, sizeof(buf), "CO2: %.1f ppm", g_co2);
    st7789_write_string(10, 130, buf, Font_11x18, MAGENTA, BLACK);

    snprintf(buf, sizeof(buf), "Node %d / Total:%d", curr_show_node + 1, valid_cnt);
    st7789_write_string(10, 160, buf, Font_11x18, WHITE, BLACK);

    return;
}
