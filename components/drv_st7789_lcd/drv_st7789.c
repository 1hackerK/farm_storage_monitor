#include "drv_st7789.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static spi_device_handle_t st7789_spi_dev;

static inline void st7789_rst_clr(void) { gpio_set_level(ST7789_PIN_RST, 0); }
static inline void st7789_rst_set(void) { gpio_set_level(ST7789_PIN_RST, 1); }
static inline void st7789_dc_clr(void) { gpio_set_level(ST7789_PIN_DC, 0); }
static inline void st7789_dc_set(void) { gpio_set_level(ST7789_PIN_DC, 1); }

static void st7789_write_cmd(uint8_t cmd)
{
    st7789_dc_clr();
    spi_transaction_t t = {0};
    t.length = 8;
    t.tx_buffer = &cmd;
    ESP_ERROR_CHECK(spi_device_transmit(st7789_spi_dev, &t));
}

static void st7789_write_data(const uint8_t *buff, size_t buff_size)
{
    st7789_dc_set();
    size_t remain = buff_size;
    const uint8_t *p = buff;
    while (remain > 0)
    {
        size_t chunk = (remain > 4096) ? 4096 : remain;
        spi_transaction_t t = {0}; 
        t.length = chunk * 8;
        t.tx_buffer = p;
        ESP_ERROR_CHECK(spi_device_transmit(st7789_spi_dev, &t));
        p += chunk;
        remain -= chunk;
    }
}

static void st7789_write_small_data(uint8_t data)
{
    st7789_dc_set();
    spi_transaction_t t = {0};
    t.length = 8;
    t.tx_buffer = &data;
    ESP_ERROR_CHECK(spi_device_transmit(st7789_spi_dev, &t));
}

static void st7789_set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint16_t x_start = x0 + X_SHIFT;
    uint16_t x_end = x1 + X_SHIFT;
    uint16_t y_start = y0 + Y_SHIFT;
    uint16_t y_end = y1 + Y_SHIFT;

    st7789_write_cmd(ST7789_CASET);
    uint8_t ca_buf[] = {x_start >> 8, x_start & 0xFF, x_end >> 8, x_end & 0xFF};
    st7789_write_data(ca_buf, sizeof(ca_buf));

    st7789_write_cmd(ST7789_RASET);
    uint8_t ra_buf[] = {y_start >> 8, y_start & 0xFF, y_end >> 8, y_end & 0xFF};
    st7789_write_data(ra_buf, sizeof(ra_buf));

    st7789_write_cmd(ST7789_RAMWR);
}

void st7789_set_rotation(uint8_t m)
{
    st7789_write_cmd(ST7789_MADCTL);
    switch (m)
    {
    case 0:
        st7789_write_small_data(ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
        break;
    case 1:
        st7789_write_small_data(ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
        break;
    case 2:
        st7789_write_small_data(ST7789_MADCTL_RGB);
        break;
    case 3:
        st7789_write_small_data(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
        break;
    default:
        break;
    }
}

esp_err_t st7789_init(void)
{
    gpio_config_t io_conf = {0};
    io_conf.pin_bit_mask = (1ULL << ST7789_PIN_RST) | (1ULL << ST7789_PIN_DC) | (1ULL << ST7789_PIN_BL);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    spi_bus_config_t buscfg = {
        .mosi_io_num = ST7789_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = ST7789_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t ret = spi_bus_initialize(ST7789_SPI_HOST, &buscfg, ST7789_USE_DMA ? SPI_DMA_CH_AUTO : 0);
    if (ret != ESP_OK)
        return ret;

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = ST7789_PIN_CS,
        .queue_size = 7,
        .flags = SPI_DEVICE_NO_DUMMY,
    };
    ret = spi_bus_add_device(ST7789_SPI_HOST, &devcfg, &st7789_spi_dev);
    if (ret != ESP_OK)
        return ret;

    vTaskDelay(pdMS_TO_TICKS(10));
    st7789_rst_clr();
    vTaskDelay(pdMS_TO_TICKS(10));
    st7789_rst_set();
    vTaskDelay(pdMS_TO_TICKS(20));

    st7789_write_cmd(ST7789_COLMOD);
    st7789_write_small_data(ST7789_COLOR_MODE_16bit);

    st7789_write_cmd(0xB2);
    uint8_t b2dat[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    st7789_write_data(b2dat, sizeof(b2dat));

    st7789_set_rotation(ST7789_ROTATION);

    st7789_write_cmd(0xB7);
    st7789_write_small_data(0x35);
    st7789_write_cmd(0xBB);
    st7789_write_small_data(0x19);
    st7789_write_cmd(0xC0);
    st7789_write_small_data(0x2C);
    st7789_write_cmd(0xC2);
    st7789_write_small_data(0x01);
    st7789_write_cmd(0xC3);
    st7789_write_small_data(0x12);
    st7789_write_cmd(0xC4);
    st7789_write_small_data(0x20);
    st7789_write_cmd(0xC6);
    st7789_write_small_data(0x0F);
    st7789_write_cmd(0xD0);
    st7789_write_small_data(0xA4);
    st7789_write_small_data(0xA1);

    st7789_write_cmd(0xE0);
    uint8_t e0dat[] = {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23};
    st7789_write_data(e0dat, sizeof(e0dat));

    st7789_write_cmd(0xE1);
    uint8_t e1dat[] = {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23};
    st7789_write_data(e1dat, sizeof(e1dat));

    st7789_write_cmd(ST7789_INVON);
    st7789_write_cmd(ST7789_SLPOUT);
    st7789_write_cmd(ST7789_NORON);
    st7789_write_cmd(ST7789_DISPON);

    vTaskDelay(pdMS_TO_TICKS(50));
    st7789_fill_color(BLACK);
    st7789_set_backlight(true);
    return ESP_OK;
}

void st7789_fill_color(uint16_t color)
{
    st7789_set_address_window(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1);
    uint8_t line_buf[ST7789_WIDTH * 2];
    for (int i = 0; i < ST7789_WIDTH; i++)
    {
        line_buf[i * 2] = (color >> 8) & 0xFF;
        line_buf[i * 2 + 1] = color & 0xFF;
    }
    for (int y = 0; y < ST7789_HEIGHT; y++)
    {
        st7789_write_data(line_buf, sizeof(line_buf));
    }
}

void st7789_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT)
        return;
    st7789_set_address_window(x, y, x, y);
    uint8_t data[] = {color >> 8, color & 0xFF};
    st7789_write_data(data, sizeof(data));
}

void st7789_fill(uint16_t xSta, uint16_t ySta, uint16_t xEnd, uint16_t yEnd, uint16_t color)
{
    if (xEnd >= ST7789_WIDTH || yEnd >= ST7789_HEIGHT)
        return;
    st7789_set_address_window(xSta, ySta, xEnd, yEnd);
    uint16_t w = xEnd - xSta + 1;
    uint8_t *line_buf = malloc(w * 2);
    for (int i = 0; i < w; i++)
    {
        line_buf[i * 2] = color >> 8;
        line_buf[i * 2 + 1] = color & 0xFF;
    }
    for (int j = ySta; j <= yEnd; j++)
    {
        st7789_write_data(line_buf, w * 2);
    }
    free(line_buf);
}

void st7789_draw_pixel_4px(uint16_t x, uint16_t y, uint16_t color)
{
    if (x <= 0 || x > ST7789_WIDTH || y <= 0 || y > ST7789_HEIGHT)
        return;
    st7789_fill(x - 1, y - 1, x + 1, y + 1, color);
}

void st7789_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    uint16_t swap;
    uint16_t steep = ABS(y1 - y0) > ABS(x1 - x0);
    if (steep)
    {
        swap = x0;
        x0 = y0;
        y0 = swap;
        swap = x1;
        x1 = y1;
        y1 = swap;
    }
    if (x0 > x1)
    {
        swap = x0;
        x0 = x1;
        x1 = swap;
        swap = y0;
        y0 = y1;
        y1 = swap;
    }
    int16_t dx, dy;
    dx = x1 - x0;
    dy = ABS(y1 - y0);
    int16_t err = dx / 2;
    int16_t ystep = (y0 < y1) ? 1 : -1;
    for (; x0 <= x1; x0++)
    {
        if (steep)
        {
            st7789_draw_pixel(y0, x0, color);
        }
        else
        {
            st7789_draw_pixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0)
        {
            y0 += ystep;
            err += dx;
        }
    }
}

void st7789_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    st7789_draw_line(x1, y1, x2, y1, color);
    st7789_draw_line(x1, y1, x1, y2, color);
    st7789_draw_line(x1, y2, x2, y2, color);
    st7789_draw_line(x2, y1, x2, y2, color);
}

void st7789_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    st7789_draw_pixel(x0, y0 + r, color);
    st7789_draw_pixel(x0, y0 - r, color);
    st7789_draw_pixel(x0 + r, y0, color);
    st7789_draw_pixel(x0 - r, y0, color);
    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        st7789_draw_pixel(x0 + x, y0 + y, color);
        st7789_draw_pixel(x0 - x, y0 + y, color);
        st7789_draw_pixel(x0 + x, y0 - y, color);
        st7789_draw_pixel(x0 - x, y0 - y, color);
        st7789_draw_pixel(x0 + y, y0 + x, color);
        st7789_draw_pixel(x0 - y, y0 + x, color);
        st7789_draw_pixel(x0 + y, y0 - x, color);
        st7789_draw_pixel(x0 - y, y0 - x, color);
    }
}

void st7789_draw_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data)
{
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT)
        return;
    if ((x + w - 1) >= ST7789_WIDTH || (y + h - 1) >= ST7789_HEIGHT)
        return;
    st7789_set_address_window(x, y, x + w - 1, y + h - 1);
    st7789_write_data((const uint8_t *)data, w * h * 2);
}

void st7789_invert_colors(uint8_t invert)
{
    st7789_write_cmd(invert ? ST7789_INVON : ST7789_INVOFF);
}

void st7789_write_char(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor)
{
    uint32_t i, b, j;
    st7789_set_address_window(x, y, x + font.width - 1, y + font.height - 1);
    for (i = 0; i < font.height; i++)
    {
        b = font.data[(ch - 32) * font.height + i];
        for (j = 0; j < font.width; j++)
        {
            if ((b << j) & 0x8000)
            {
                uint8_t d[] = {color >> 8, color & 0xFF};
                st7789_write_data(d, 2);
            }
            else
            {
                uint8_t d[] = {bgcolor >> 8, bgcolor & 0xFF};
                st7789_write_data(d, 2);
            }
        }
    }
}

void st7789_write_string(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor)
{
    while (*str)
    {
        if (x + font.width >= ST7789_WIDTH)
        {
            x = 0;
            y += font.height;
            if (y + font.height >= ST7789_HEIGHT)
                break;
            if (*str == ' ')
            {
                str++;
                continue;
            }
        }
        st7789_write_char(x, y, *str, font, color, bgcolor);
        x += font.width;
        str++;
    }
}

void st7789_draw_filled_rectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT)
        return;
    if ((x + w) >= ST7789_WIDTH)
        w = ST7789_WIDTH - x;
    if ((y + h) >= ST7789_HEIGHT)
        h = ST7789_HEIGHT - y;
    for (uint16_t i = 0; i <= h; i++)
    {
        st7789_draw_line(x, y + i, x + w, y + i, color);
    }
}

void st7789_draw_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color)
{
    st7789_draw_line(x1, y1, x2, y2, color);
    st7789_draw_line(x2, y2, x3, y3, color);
    st7789_draw_line(x3, y3, x1, y1, color);
}

void st7789_draw_filled_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color)
{
    int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0,
            yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0,
            curpixel = 0;
    deltax = ABS(x2 - x1);
    deltay = ABS(y2 - y1);
    x = x1;
    y = y1;
    if (x2 >= x1)
    {
        xinc1 = 1;
        xinc2 = 1;
    }
    else
    {
        xinc1 = -1;
        xinc2 = -1;
    }
    if (y2 >= y1)
    {
        yinc1 = 1;
        yinc2 = 1;
    }
    else
    {
        yinc1 = -1;
        yinc2 = -1;
    }
    if (deltax >= deltay)
    {
        xinc1 = 0;
        yinc2 = 0;
        den = deltax;
        num = deltax / 2;
        numadd = deltay;
        numpixels = deltax;
    }
    else
    {
        xinc2 = 0;
        yinc1 = 0;
        den = deltay;
        num = deltay / 2;
        numadd = deltax;
        numpixels = deltay;
    }
    for (curpixel = 0; curpixel <= numpixels; curpixel++)
    {
        st7789_draw_line(x, y, x3, y3, color);
        num += numadd;
        if (num >= den)
        {
            num -= den;
            x += xinc1;
            y += yinc1;
        }
        x += xinc2;
        y += yinc2;
    }
}

void st7789_draw_filled_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    st7789_draw_pixel(x0, y0 + r, color);
    st7789_draw_pixel(x0, y0 - r, color);
    st7789_draw_pixel(x0 + r, y0, color);
    st7789_draw_pixel(x0 - r, y0, color);
    st7789_draw_line(x0 - r, y0, x0 + r, y0, color);
    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        st7789_draw_line(x0 - x, y0 + y, x0 + x, y0 + y, color);
        st7789_draw_line(x0 + x, y0 - y, x0 - x, y0 - y, color);
        st7789_draw_line(x0 + y, y0 + x, x0 - y, y0 + x, color);
        st7789_draw_line(x0 + y, y0 - x, x0 - y, y0 - x, color);
    }
}

void st7789_tear_effect(uint8_t tear)
{
    st7789_write_cmd(tear ? 0x35 : 0x34);
}

void st7789_set_backlight(bool en)
{
    gpio_set_level(ST7789_PIN_BL, en ? 1 : 0);
}

void st7789_test(void)
{
    st7789_fill_color(WHITE);
    vTaskDelay(pdMS_TO_TICKS(1000));
    st7789_write_string(10, 20, "Speed Test", Font_11x18, RED, WHITE);
    vTaskDelay(pdMS_TO_TICKS(1000));
    st7789_fill_color(CYAN);
    vTaskDelay(pdMS_TO_TICKS(500));
    st7789_fill_color(RED);
    vTaskDelay(pdMS_TO_TICKS(500));
    st7789_fill_color(BLUE);
    vTaskDelay(pdMS_TO_TICKS(500));
    st7789_fill_color(GREEN);
    vTaskDelay(pdMS_TO_TICKS(500));
    st7789_fill_color(YELLOW);
    vTaskDelay(pdMS_TO_TICKS(500));
    st7789_fill_color(BROWN);
    vTaskDelay(pdMS_TO_TICKS(500));
    st7789_fill_color(DARKBLUE);
    vTaskDelay(pdMS_TO_TICKS(500));
    st7789_fill_color(MAGENTA);
    vTaskDelay(pdMS_TO_TICKS(500));
    st7789_fill_color(LIGHTGREEN);
    vTaskDelay(pdMS_TO_TICKS(500));
    st7789_fill_color(LGRAY);
    vTaskDelay(pdMS_TO_TICKS(500));
    st7789_fill_color(LBBLUE);
    vTaskDelay(pdMS_TO_TICKS(500));
    st7789_fill_color(WHITE);
    vTaskDelay(pdMS_TO_TICKS(1000));

    st7789_write_string(10, 10, "Font test.", Font_16x26, GBLUE, WHITE);
    st7789_write_string(10, 50, "Hello ESP32‑S3!", Font_7x10, RED, WHITE);
    st7789_write_string(10, 75, "Hello ESP32‑S3!", Font_11x18, YELLOW, WHITE);
    st7789_write_string(10, 100, "Hello ESP32‑S3!", Font_16x26, MAGENTA, WHITE);
    vTaskDelay(pdMS_TO_TICKS(1000));

    st7789_fill_color(RED);
    st7789_write_string(10, 10, "Rect./Line.", Font_11x18, YELLOW, BLACK);
    st7789_draw_rectangle(30, 30, 100, 100, WHITE);
    vTaskDelay(pdMS_TO_TICKS(1000));

    st7789_fill_color(RED);
    st7789_write_string(10, 10, "Filled Rect.", Font_11x18, YELLOW, BLACK);
    st7789_draw_filled_rectangle(30, 30, 50, 50, WHITE);
    vTaskDelay(pdMS_TO_TICKS(1000));

    st7789_fill_color(RED);
    st7789_write_string(10, 10, "Circle.", Font_11x18, YELLOW, BLACK);
    st7789_draw_circle(60, 60, 25, WHITE);
    vTaskDelay(pdMS_TO_TICKS(1000));

    st7789_fill_color(RED);
    st7789_write_string(10, 10, "Filled Cir.", Font_11x18, YELLOW, BLACK);
    st7789_draw_filled_circle(60, 60, 25, WHITE);
    vTaskDelay(pdMS_TO_TICKS(1000));

    st7789_fill_color(RED);
    st7789_write_string(10, 10, "Triangle", Font_11x18, YELLOW, BLACK);
    st7789_draw_triangle(30, 30, 30, 70, 60, 40, WHITE);
    vTaskDelay(pdMS_TO_TICKS(1000));

    st7789_fill_color(RED);
    st7789_write_string(10, 10, "Filled Tri", Font_11x18, YELLOW, BLACK);
    st7789_draw_filled_triangle(30, 30, 30, 70, 60, 40, WHITE);
    vTaskDelay(pdMS_TO_TICKS(1000));
}
