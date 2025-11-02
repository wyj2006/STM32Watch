#include <string.h>

#include "font.h"
#include "i2c.h"
#include "oled.h"

uint8_t gram[PAGE_NUM][COL_NUM];

void oled_sendcmd(uint8_t cmd)
{
    uint8_t buffer[2];
    buffer[0] = 0x00;
    buffer[1] = cmd;
    HAL_I2C_Master_Transmit(&OLED_I2C, OLED_ADDR, buffer, 2, HAL_MAX_DELAY);
}

void oled_init()
{
    HAL_Delay(100);
    uint8_t init_cmd[] = {
        0xae,       // 显示关闭
        0xd5, 0x80, // 设置显示时钟分频/震荡频率
        0xa8, 0x3f, // 设置多路复用率
        0xd3, 0x00, // 设置显示偏移
        0x40,       // 设置显示开始行
        0xa1,       // 设置左右方向，0xA1正常0xA0左右反置
        0xc8,       ////设置上下方向，0xC8正常0xC0上下反置
        0xda, 0x12, // 设置COM引I脚硬件配置
        0x81, 0xcf, // 设置对比度控制
        0xd9, 0xf1, // 设置预充电周期
        0xdb, 0x30, // 设置VCOMH取消选择级别
        0xa4,       // 设置整个显示打开/关闭
        0xa6,       // 设置正常/倒转显示
        0x8d, 0x14, // 设置充电泵
        0xaf,       // 开启显示
    };
    for (int i = 0; i < sizeof(init_cmd); i++) oled_sendcmd(init_cmd[i]);
}

void oled_flush()
{
    uint8_t buffer[COL_NUM + 1];
    buffer[0] = 0x40;
    for (uint8_t i = 0; i < PAGE_NUM; i++)
    {
        memcpy(buffer + 1, gram[i], sizeof(gram[i]));
        oled_sendcmd(0xb0 + i);
        oled_sendcmd(0x00);
        oled_sendcmd(0x10);
        HAL_I2C_Master_Transmit(&OLED_I2C, OLED_ADDR, buffer, sizeof(buffer),
                                HAL_MAX_DELAY);
    }
}

void oled_setpixel(uint8_t x, uint8_t y, uint8_t state)
{
    if (x >= COL_NUM || y >= ROW_NUM) return;
    if (state == 1)
        gram[y / 8][x] |= 1 << (y % 8);
    else
        gram[y / 8][x] &= ~(1 << (y % 8));
}

void oled_display_on() { oled_sendcmd(0xaf); }

void oled_display_off() { oled_sendcmd(0xae); }

void oled_show_number(int number, uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                      uint8_t line_w)
{
    if (number > 9) return;
    for (int i = x; i < x + w; i++)
    {
        for (int j = y; j < y + h; j++)
        {
            uint8_t state = 0;
            switch (number)
            {
#define TOP (j >= y && j < y + line_w)
#define BOTTOM (j >= y + h - line_w && j < y + h)
#define CENTER                                                                 \
    (line_w > 1 ?                                                              \
         (j >= y + h / 2 - line_w / 2 && j < y + h / 2 + line_w / 2) :         \
         (j == y + h / 2))
#define LEFT_TOP (i >= x && i < x + line_w && j >= y && j < y + h / 2)
#define LEFT_BOTTOM (i >= x && i < x + line_w && j >= y + h / 2 && j < y + h)
#define LEFT (LEFT_TOP || LEFT_BOTTOM)
#define RIGHT_TOP (i >= x + w - line_w && i < x + w && j >= y && j < y + h / 2)
#define RIGHT_BOTTOM                                                           \
    (i >= x + w - line_w && i < x + w && j >= y + h / 2 && j < y + h)
#define RIGHT (RIGHT_TOP || RIGHT_BOTTOM)
            case 0: state = TOP || BOTTOM || LEFT || RIGHT; break;
            case 1: state = RIGHT; break;
            case 2:
                state = TOP || RIGHT_TOP || CENTER || LEFT_BOTTOM || BOTTOM;
                break;
            case 3: state = TOP || CENTER || BOTTOM || RIGHT; break;
            case 4: state = LEFT_TOP || CENTER || RIGHT; break;
            case 5:
                state = TOP || LEFT_TOP || CENTER || RIGHT_BOTTOM || BOTTOM;
                break;
            case 6:
                state = TOP || LEFT || CENTER || BOTTOM || RIGHT_BOTTOM;
                break;
            case 7: state = TOP || RIGHT; break;
            case 8: state = TOP || BOTTOM || CENTER || LEFT || RIGHT; break;
            case 9: state = TOP || CENTER || BOTTOM || RIGHT || LEFT_TOP; break;
#undef TOP
#undef BOTTOM
#undef CENTER
#undef LEFT_TOP
#undef LEFT_BOTTOM
#undef LEFT
#undef RIGHT_TOP
#undef RIGHT_BOTTOM
#undef RIGHT
            }
            oled_setpixel(i, j, state);
        }
    }
}

void oled_show_numbers(int number, uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                       uint8_t line_w, uint8_t spacing, uint8_t length)
{
    uint8_t digits[10], digit_count = 0;
    while (number != 0)
    {
        digits[digit_count++] = number % 10;
        number /= 10;
    }
    while (digit_count < length) digits[digit_count++] = 0;
    for (uint8_t i = digit_count - 1; i < digit_count; i--)
        oled_show_number(digits[i], x + (digit_count - 1 - i) * (w + spacing),
                         y, w, h, line_w);
}

void oled_show_ascii(char c, uint8_t x, uint8_t y)
{
    uint8_t *data = FONT + (c - ' ') * (((FONT_H + 7) / 8) * FONT_W);
    for (uint8_t i = 0; i < FONT_W; i++)
    {
        for (uint8_t j = 0; j < FONT_H; j++)
        {
            for (uint8_t k = 0; k < 8; k++)
            {
                if (j * 8 + k >= FONT_H) break; // 防止越界(不完整的字节
                oled_setpixel(x + i, y + j * 8 + k,
                              (data[i + j * FONT_W] >> k) & 0x1);
            }
        }
    }
}

void oled_show_string(char *str, uint8_t x, uint8_t y, uint8_t spacing)
{
    while (*str != '\0')
    {
        oled_show_ascii(*str, x, y);
        str++;
        x += FONT_W + spacing;
    }
}