#include "scenes.h"

#include "buttons.h"
#include "icons.h"
#include "menu.h"
#include "sh1106_host.h"

#include <stddef.h>

// ----- Styles (mirror main/main.c) ---------------------------------------
static const menu_style_t arrow_style = {
    .icon_w       = 32,
    .icon_h       = 32,
    .row_height   = 10,
    .title_height = 10,
    .selection    = MENU_SEL_ARROW,
};
static const menu_style_t border_style = {
    .icon_w       = 32,
    .icon_h       = 32,
    .row_height   = 10,
    .title_height = 10,
    .selection    = MENU_SEL_BORDER,
};
static const menu_style_t home_style = {
    .icon_w       = 32,
    .icon_h       = 32,
    .row_height   = 10,
    .title_height = 10,
    .selection    = MENU_SEL_BORDER,
};

// ----- Menus -------------------------------------------------------------
static const menu_item_t about_items[] = {
    {.kind = MENU_ITEM_ACTION, .label = "Version", .u.action = NULL},
    {.kind = MENU_ITEM_ACTION, .label = "Uptime", .u.action = NULL},
    MENU_END,
};
static const menu_t about_menu = {
    .title  = "About",
    .layout = MENU_LAYOUT_LIST,
    .items  = about_items,
    .style  = &arrow_style,
};

static const menu_item_t settings_items[] = {
    {.kind = MENU_ITEM_ACTION, .label = "Brightness", .u.action = NULL},
    {.kind = MENU_ITEM_ACTION, .label = "Contrast", .u.action = NULL},
    {.kind = MENU_ITEM_SUBMENU, .label = "About", .u.submenu = &about_menu},
    {.kind = MENU_ITEM_ACTION, .label = "Reset", .u.action = NULL},
    MENU_END,
};
static const menu_t settings_menu = {
    .title  = "Settings",
    .layout = MENU_LAYOUT_LIST,
    .items  = settings_items,
    .style  = &border_style,
};

static const menu_item_t wifi_items[] = {
    {.kind = MENU_ITEM_ACTION, .label = "Scan", .u.action = NULL},
    {.kind = MENU_ITEM_ACTION, .label = "Connect", .u.action = NULL},
    {.kind = MENU_ITEM_ACTION, .label = "Status", .u.action = NULL},
    MENU_END,
};
static const menu_t wifi_menu = {
    .title  = "WiFi",
    .layout = MENU_LAYOUT_LIST,
    .items  = wifi_items,
    .style  = NULL, // default style → MENU_SEL_INVERT
};

static const menu_item_t bt_items[] = {
    {.kind = MENU_ITEM_ACTION, .label = "Pair", .u.action = NULL},
    {.kind = MENU_ITEM_ACTION, .label = "Devices", .u.action = NULL},
    MENU_END,
};
static const menu_t bt_menu = {
    .title  = "Bluetooth",
    .layout = MENU_LAYOUT_LIST,
    .items  = bt_items,
    .style  = NULL,
};

static const menu_item_t home_items[] = {
    {.kind      = MENU_ITEM_SUBMENU,
     .label     = "Settings",
     .icon      = icon_settings,
     .u.submenu = &settings_menu},
    {.kind = MENU_ITEM_SUBMENU, .label = "WiFi", .icon = icon_wifi, .u.submenu = &wifi_menu},
    {.kind      = MENU_ITEM_SUBMENU,
     .label     = "Bluetooth",
     .icon      = icon_bluetooth,
     .u.submenu = &bt_menu},
    MENU_END,
};
static const menu_t home = {
    .title  = "Home",
    .layout = MENU_LAYOUT_ICONS,
    .items  = home_items,
    .style  = &home_style,
};

// ----- Helpers -----------------------------------------------------------
static void press(button_id_t id)
{
    button_event_t evt = {.button = id, .event = BTN_EVT_PRESSED};
    menu_handle_event(evt);
}

// ----- Per-scene setup ---------------------------------------------------
static void setup_home_icons(void)
{
    menu_init(&home, NULL); // Home with Settings (index 0) selected — done.
}

static void setup_settings_list(void)
{
    menu_init(&home, NULL);
    press(BTN_ENTER);   // descend into settings_menu
    press(BTN_FORWARD); // index 0 (Brightness) → 1 (Contrast)
}

static void setup_wifi_invert(void)
{
    menu_init(&home, NULL);
    press(BTN_FORWARD); // home index 0 → 1 (WiFi)
    press(BTN_ENTER);   // descend into wifi_menu
    press(BTN_FORWARD); // index 0 (Scan) → 1 (Connect)
}

static void setup_about_arrow(void)
{
    menu_init(&home, NULL);
    press(BTN_ENTER);   // descend into settings_menu
    press(BTN_FORWARD); // 0 → 1
    press(BTN_FORWARD); // 1 → 2 (About)
    press(BTN_ENTER);   // descend into about_menu (index 0 = Version)
}

static void setup_toast(void)
{
    menu_init(&home, NULL);
    press(BTN_ENTER);                     // settings_menu, index 0 (Brightness)
    sh1106_host_capture_next_flush();     // freeze the upcoming toast frame
    menu_toast("Brightness +", 600);
}

// ----- Table -------------------------------------------------------------
const scene_t scenes[] = {
    {"home-icons", setup_home_icons},
    {"settings-list", setup_settings_list},
    {"wifi-invert", setup_wifi_invert},
    {"about-arrow", setup_about_arrow},
    {"toast", setup_toast},
};
const int scenes_count = (int)(sizeof(scenes) / sizeof(scenes[0]));
