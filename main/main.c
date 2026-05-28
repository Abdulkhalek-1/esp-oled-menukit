#include "sh1106.h"

void app_main(void)
{
    sh1106_init();
    sh1106_clear();
    sh1106_draw_string(0, 0, "Hello, OLED!");
    sh1106_draw_string(0, 8, "SH1106-learn");
    sh1106_flush();
}
