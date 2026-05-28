#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SH1106_WIDTH 128
#define SH1106_HEIGHT 64

void sh1106_init(void);
void sh1106_clear(void);
void sh1106_set_pixel(int x, int y, bool on);
void sh1106_flush(void);
void sh1106_draw_string(int x, int y, const char *s);
