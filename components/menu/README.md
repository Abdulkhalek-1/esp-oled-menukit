# menu

Recursive data-driven menu engine for the SH1106 OLED + 3-button input. Supports two layouts (vertical text list, horizontal icon row), three selection styles (invert / arrow / border), title bars, top-half scrolling, action callbacks, and toast notifications.

## Public API

| Symbol                                              | Description                                                                  |
|-----------------------------------------------------|------------------------------------------------------------------------------|
| `enum menu_item_kind_t { MENU_ITEM_END, MENU_ITEM_ACTION, MENU_ITEM_SUBMENU }` | Item discriminator; END is the array sentinel.            |
| `enum menu_layout_t { MENU_LAYOUT_LIST, MENU_LAYOUT_ICONS }` | Per-menu layout selector.                                          |
| `enum menu_selection_t { MENU_SEL_INVERT, MENU_SEL_ARROW, MENU_SEL_BORDER }` | Per-menu selection rendering style.                          |
| `struct menu_style_t`                               | Per-menu style overrides (icon size, row pitch, title height, selection).    |
| `struct menu_item_t`                                | One menu item: label + optional icon + (action callback OR submenu pointer). |
| `struct menu_t`                                     | One menu screen: optional title + layout + items + optional style.           |
| `#define MENU_END`                                  | Sentinel value to terminate `menu_item_t` arrays.                            |
| `void menu_init(const menu_t *root, void *user_ctx)`| Set root menu and the context passed to action callbacks.                    |
| `void menu_handle_event(button_event_t evt)`        | Drive the engine with one button event.                                      |
| `void menu_run_task(QueueHandle_t q, UBaseType_t priority)` | Spawn a task that drains q and calls `menu_handle_event`.            |
| `void menu_redraw(void)`                            | Force a redraw (useful after async state changes).                           |
| `void menu_toast(const char *msg, int duration_ms)` | Blocking inverted-bar message at the bottom of the screen.                   |

## Dependencies

- `buttons` — public (events).
- `freertos` — public (queue + task types).
- `sh1106` — private (rendering).
- `font8x8` — private (text rendering).

## Usage

```c
#include "menu.h"

static void act_hello(void *ctx) { menu_toast("Hello!", 500); }

static const menu_item_t home_items[] = {
    { .kind = MENU_ITEM_ACTION, .label = "Say hi", .u.action = act_hello },
    MENU_END,
};
static const menu_t home = {
    .title = "Demo", .layout = MENU_LAYOUT_LIST, .items = home_items, .style = NULL,
};

// In app_main, after sh1106_init() and buttons_init():
menu_init(&home, NULL);
menu_run_task(buttons_queue, tskIDLE_PRIORITY + 1);
```

## Notes

- Menus are defined as static C arrays with `MENU_END` sentinel — no runtime construction. For recursion (a child referencing a parent or sibling) use `extern const menu_t foo_menu;` forward declarations.
- The nav stack has a fixed depth of 8 (`MENU_MAX_DEPTH`). Deeper trees will refuse to descend further.
- `menu_toast` blocks the menu task for the specified duration. Button events received during the toast are buffered in the queue and processed afterward.
- Icons must be 32×32 by default; change via `menu_style_t.icon_w` / `icon_h` and supply correctly-sized bitmaps. Layout: page-major, 4 pages × 32 bytes per icon = 128 bytes.
- Selection style applies equivalently to LIST and ICONS layouts. INVERT on icons uses a 1-px box instead of true pixel inversion (icon inversion would require XOR pixel ops).
