#pragma once
#include <stdint.h>
void st7735_init(void);
void st7735_fill_screen(uint16_t color);
void st7735_draw_text(int x, int y, const char *text, uint16_t color, uint16_t bg, int scale);
void st7735_draw_pixel(int x, int y, uint16_t color);
void st7735_fill_rect(int x, int y, int w, int h, uint16_t color);
