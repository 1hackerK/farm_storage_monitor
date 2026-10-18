#ifndef __ST7789_H
#define __ST7789_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

// ===================== 硬件引脚配置 =====================
#define ST7789_SPI_HOST SPI2_HOST

#define ST7789_PIN_SCLK GPIO_NUM_41
#define ST7789_PIN_MOSI GPIO_NUM_40
#define ST7789_PIN_DC GPIO_NUM_39
#define ST7789_PIN_RST 1
#define ST7789_PIN_CS GPIO_NUM_38
#define ST7789_PIN_BL GPIO_NUM_42

#define ST7789_USE_DMA 1

// 屏幕 240*320
#define USING_240X320
#define ST7789_ROTATION 2

#ifdef USING_240X320
#define ST7789_WIDTH 240
#define ST7789_HEIGHT 320
#if ST7789_ROTATION == 0
#define X_SHIFT 0
#define Y_SHIFT 80
#elif ST7789_ROTATION == 1
#define X_SHIFT 80
#define Y_SHIFT 0
#elif ST7789_ROTATION == 2
#define X_SHIFT 0
#define Y_SHIFT 0
#elif ST7789_ROTATION == 3
#define X_SHIFT 0
#define Y_SHIFT 0
#endif
#endif

// ===================== RGB565颜色定义 =====================
#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define MAGENTA 0xF81F
#define GREEN 0x07E0
#define CYAN 0x7FFF
#define YELLOW 0xFFE0
#define GRAY 0X8430
#define BRED 0XF81F
#define GRED 0XFFE0
#define GBLUE 0X07FF
#define BROWN 0XBC40
#define BRRED 0XFC07
#define DARKBLUE 0X01CF
#define LIGHTBLUE 0X7D7C
#define GRAYBLUE 0X5458
#define LIGHTGREEN 0X841F
#define LGRAY 0XC618
#define LGRAYBLUE 0XA651
#define LBBLUE 0X2B12

// ===================== ST7789寄存器 =====================
#define ST7789_NOP 0x00
#define ST7789_SWRESET 0x01
#define ST7789_RDDID 0x04
#define ST7789_RDDST 0x09

#define ST7789_SLPIN 0x10
#define ST7789_SLPOUT 0x11
#define ST7789_PTLON 0x12
#define ST7789_NORON 0x13

#define ST7789_INVOFF 0x20
#define ST7789_INVON 0x21
#define ST7789_DISPOFF 0x28
#define ST7789_DISPON 0x29

#define ST7789_CASET 0x2A
#define ST7789_RASET 0x2B
#define ST7789_RAMWR 0x2C
#define ST7789_RAMRD 0x2E

#define ST7789_PTLAR 0x30
#define ST7789_COLMOD 0x3A
#define ST7789_MADCTL 0x36

#define ST7789_MADCTL_MY 0x80
#define ST7789_MADCTL_MX 0x40
#define ST7789_MADCTL_MV 0x20
#define ST7789_MADCTL_ML 0x10
#define ST7789_MADCTL_RGB 0x00

#define ST7789_COLOR_MODE_16bit 0x55

// 字体结构体
typedef struct
{
    uint8_t width;
    uint8_t height;
    const uint16_t *data;
} FontDef;

// 外部字体声明
extern FontDef Font_7x10;
extern FontDef Font_11x18;
extern FontDef Font_16x26;

// ===================== API函数声明 =====================
esp_err_t st7789_init(void);
void st7789_set_rotation(uint8_t m);
void st7789_fill_color(uint16_t color);
void st7789_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void st7789_fill(uint16_t xSta, uint16_t ySta, uint16_t xEnd, uint16_t yEnd, uint16_t color);
void st7789_draw_pixel_4px(uint16_t x, uint16_t y, uint16_t color);

void st7789_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void st7789_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void st7789_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);
void st7789_draw_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data);
void st7789_invert_colors(uint8_t invert);

void st7789_write_char(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor);
void st7789_write_string(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor);

void st7789_draw_filled_rectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void st7789_draw_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);
void st7789_draw_filled_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);
void st7789_draw_filled_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void st7789_tear_effect(uint8_t tear);
void st7789_test(void);

void st7789_set_backlight(bool en);

#define ABS(x) ((x) > 0 ? (x) : -(x))

#endif
