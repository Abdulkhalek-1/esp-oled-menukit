#include "sh1106.h"

static void draw_border(void)
{
    for (int x = 0; x < SH1106_WIDTH; x++) {
        sh1106_set_pixel(x, 0,                  true);
        sh1106_set_pixel(x, SH1106_HEIGHT - 1,  true);
    }
    for (int y = 0; y < SH1106_HEIGHT; y++) {
        sh1106_set_pixel(0,                 y, true);
        sh1106_set_pixel(SH1106_WIDTH - 1,  y, true);
    }
}

static void draw_filled_rect(int x0, int y0, int w, int h)
{
    for (int y = y0; y < y0 + h; y++) {
        for (int x = x0; x < x0 + w; x++) {
            sh1106_set_pixel(x, y, true);
        }
    }
}

void app_main(void)
{
    sh1106_init();
    sh1106_clear();
    draw_border();
    draw_filled_rect((SH1106_WIDTH - 40) / 2, (SH1106_HEIGHT - 20) / 2, 40, 20);
    sh1106_flush();
}
