#include "buttons.h"
#include "menu.h"
#include "sh1106.h"

#include "esp_log.h"

static void act_brightness(void *ctx) { (void)ctx; menu_toast("Brightness +", 600); }
static void act_contrast  (void *ctx) { (void)ctx; menu_toast("Contrast +",   600); }
static void act_reset     (void *ctx) { (void)ctx; menu_toast("Reset!",       600); }
static void act_about     (void *ctx) { (void)ctx; menu_toast("v0.1",         600); }
static void act_log       (void *ctx) { (void)ctx; menu_toast("OK",           400); }

static const menu_item_t long_items[] = {
    { .kind = MENU_ITEM_ACTION, .label = "Item 1",  .u.action = act_log },
    { .kind = MENU_ITEM_ACTION, .label = "Item 2",  .u.action = act_log },
    { .kind = MENU_ITEM_ACTION, .label = "Item 3",  .u.action = act_log },
    { .kind = MENU_ITEM_ACTION, .label = "Item 4",  .u.action = act_log },
    { .kind = MENU_ITEM_ACTION, .label = "Item 5",  .u.action = act_log },
    { .kind = MENU_ITEM_ACTION, .label = "Item 6",  .u.action = act_log },
    { .kind = MENU_ITEM_ACTION, .label = "Item 7",  .u.action = act_log },
    { .kind = MENU_ITEM_ACTION, .label = "Item 8",  .u.action = act_log },
    { .kind = MENU_ITEM_ACTION, .label = "Item 9",  .u.action = act_log },
    { .kind = MENU_ITEM_ACTION, .label = "Item 10", .u.action = act_log },
    MENU_END,
};
static const menu_t long_menu = {
    .title = "Long List", .layout = MENU_LAYOUT_LIST, .items = long_items, .style = NULL,
};

static const menu_item_t settings_items[] = {
    { .kind = MENU_ITEM_ACTION, .label = "Brightness", .u.action = act_brightness },
    { .kind = MENU_ITEM_ACTION, .label = "Contrast",   .u.action = act_contrast },
    { .kind = MENU_ITEM_ACTION, .label = "Reset",      .u.action = act_reset },
    MENU_END,
};
static const menu_t settings_menu = {
    .title = "Settings", .layout = MENU_LAYOUT_LIST, .items = settings_items, .style = NULL,
};

static const menu_item_t home_items[] = {
    { .kind = MENU_ITEM_SUBMENU, .label = "Settings",  .u.submenu = &settings_menu },
    { .kind = MENU_ITEM_SUBMENU, .label = "Long List", .u.submenu = &long_menu },
    { .kind = MENU_ITEM_ACTION,  .label = "About",     .u.action = act_about },
    MENU_END,
};
static const menu_t home = {
    .title = "Home", .layout = MENU_LAYOUT_LIST, .items = home_items, .style = NULL,
};

void app_main(void)
{
    sh1106_init();
    sh1106_clear();
    sh1106_flush();

    buttons_config_t cfg = {
        .pins              = { 25, 33, 32 },
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
