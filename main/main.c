#include "buttons.h"
#include "sh1106.h"

void app_main(void)
{
    sh1106_init();
    sh1106_clear();
    sh1106_draw_string(0, 0, "Buttons demo");
    sh1106_flush();

    buttons_config_t cfg = {
        .pins              = { 32, 33, 25 },  // BACK, ENTER, FORWARD
        .debounce_ms       = 20,
        .long_press_ms     = 500,
        .repeat_interval_ms = 150,
    };
    buttons_init(&cfg);
    // Queue is discarded for now — Task 2 will add the consumer.
}
