#include "sh1106.h"

void app_main(void)
{
    sh1106_init();
    sh1106_clear();
    sh1106_flush();
}
