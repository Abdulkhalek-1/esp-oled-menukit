#include "buttons.h"
#include "icons.h"
#include "menu.h"
#include "sh1106.h"

#include "esp_log.h"

// ----- Action callbacks --------------------------------------------------
static void act_brightness(void *ctx) { (void)ctx; menu_toast("Brightness +", 600); }
static void act_contrast  (void *ctx) { (void)ctx; menu_toast("Contrast +",   600); }
static void act_reset     (void *ctx) { (void)ctx; menu_toast("Reset!",       600); }
static void act_version   (void *ctx) { (void)ctx; menu_toast("v0.1",         600); }
static void act_uptime    (void *ctx) { (void)ctx; menu_toast("Up 0m12s",     600); }
static void act_scan      (void *ctx) { (void)ctx; menu_toast("Scanning",     600); }
static void act_connect   (void *ctx) { (void)ctx; menu_toast("Connect",      600); }
static void act_status    (void *ctx) { (void)ctx; menu_toast("Online",       600); }
static void act_pair      (void *ctx) { (void)ctx; menu_toast("Pairing",      600); }
static void act_devices   (void *ctx) { (void)ctx; menu_toast("Devices",      600); }

// ----- Styles ------------------------------------------------------------
static const menu_style_t arrow_style = {
    .icon_w = 32, .icon_h = 32, .row_height = 10, .title_height = 10,
    .selection = MENU_SEL_ARROW,
};
static const menu_style_t border_style = {
    .icon_w = 32, .icon_h = 32, .row_height = 10, .title_height = 10,
    .selection = MENU_SEL_BORDER,
};
static const menu_style_t home_style = {
    .icon_w = 32, .icon_h = 32, .row_height = 10, .title_height = 10,
    .selection = MENU_SEL_BORDER,
};

// ----- About submenu (3rd level) ----------------------------------------
static const menu_item_t about_items[] = {
    { .kind = MENU_ITEM_ACTION, .label = "Version", .u.action = act_version },
    { .kind = MENU_ITEM_ACTION, .label = "Uptime",  .u.action = act_uptime },
    MENU_END,
};
static const menu_t about_menu = {
    .title = "About", .layout = MENU_LAYOUT_LIST, .items = about_items, .style = &arrow_style,
};

// ----- Settings submenu --------------------------------------------------
static const menu_item_t settings_items[] = {
    { .kind = MENU_ITEM_ACTION,  .label = "Brightness", .u.action  = act_brightness },
    { .kind = MENU_ITEM_ACTION,  .label = "Contrast",   .u.action  = act_contrast },
    { .kind = MENU_ITEM_SUBMENU, .label = "About",      .u.submenu = &about_menu },
    { .kind = MENU_ITEM_ACTION,  .label = "Reset",      .u.action  = act_reset },
    MENU_END,
};
static const menu_t settings_menu = {
    .title = "Settings", .layout = MENU_LAYOUT_LIST, .items = settings_items, .style = &border_style,
};

// ----- WiFi submenu ------------------------------------------------------
static const menu_item_t wifi_items[] = {
    { .kind = MENU_ITEM_ACTION, .label = "Scan",    .u.action = act_scan },
    { .kind = MENU_ITEM_ACTION, .label = "Connect", .u.action = act_connect },
    { .kind = MENU_ITEM_ACTION, .label = "Status",  .u.action = act_status },
    MENU_END,
};
static const menu_t wifi_menu = {
    .title = "WiFi", .layout = MENU_LAYOUT_LIST, .items = wifi_items, .style = NULL,
};

// ----- Bluetooth submenu -------------------------------------------------
static const menu_item_t bt_items[] = {
    { .kind = MENU_ITEM_ACTION, .label = "Pair",    .u.action = act_pair },
    { .kind = MENU_ITEM_ACTION, .label = "Devices", .u.action = act_devices },
    MENU_END,
};
static const menu_t bt_menu = {
    .title = "Bluetooth", .layout = MENU_LAYOUT_LIST, .items = bt_items, .style = NULL,
};

// ----- Home (icons) ------------------------------------------------------
static const menu_item_t home_items[] = {
    { .kind = MENU_ITEM_SUBMENU, .label = "Settings",  .icon = icon_settings,
      .u.submenu = &settings_menu },
    { .kind = MENU_ITEM_SUBMENU, .label = "WiFi",      .icon = icon_wifi,
      .u.submenu = &wifi_menu },
    { .kind = MENU_ITEM_SUBMENU, .label = "Bluetooth", .icon = icon_bluetooth,
      .u.submenu = &bt_menu },
    MENU_END,
};
static const menu_t home = {
    .title = "Home", .layout = MENU_LAYOUT_ICONS, .items = home_items, .style = &home_style,
};

// ----- Entry point -------------------------------------------------------
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
