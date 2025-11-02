#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>

#include "font.h"

#define OLED_ADDR 0x78
#define OLED_I2C hi2c1

#define ROW_NUM 64
#define COL_NUM 128
#define PAGE_NUM (ROW_NUM / 8)

#define FONT ascii_12x6[0]
#define FONT_W 6
#define FONT_H 12

extern uint8_t gram[PAGE_NUM][COL_NUM];

void oled_init();
void oled_flush();
void oled_setpixel(uint8_t x, uint8_t y, uint8_t state);
void oled_display_on();
void oled_display_off();
void oled_show_number(int number, uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                      uint8_t line_w);
void oled_show_numbers(int number, uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                       uint8_t line_w, uint8_t spacing, uint8_t length);
void oled_show_ascii(char c, uint8_t x, uint8_t y);
void oled_show_string(char *str, uint8_t x, uint8_t y, uint8_t spacing);

#endif