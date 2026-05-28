#include "buttons.h"
#include "menu.h"
#include "sh1106.h"

#include "esp_log.h"

static const menu_item_t home_items[] = {
    { .kind = MENU_ITEM_ACTION, .label = "Option A" },
    { .kind = MENU_ITEM_ACTION, .label = "Option B" },
    { .kind = MENU_ITEM_ACTION, .label = "Option C" },
    MENU_END,
};

static const menu_t home = {
    .title  = NULL,
    .layout = MENU_LAYOUT_LIST,
    .items  = home_items,
    .style  = NULL,
};

void app_main(void)
{
    sh1106_init();
    sh1106_clear();
    sh1106_flush();

    buttons_config_t cfg = {
        .pins              = { 32, 33, 25 },
        .debounce_ms       = 20,
        .long_press_ms     = 500,
        .repeat_interval_ms = 150,
    };
    QueueHandle_t q = buttons_init(&cfg);
    if (q == NULL) {
        ESP_LOGE("main", "buttons_init failed");
        return;
    }

    menu_init(&home, NULL);
    menu_run_task(q, tskIDLE_PRIORITY + 1);
}
