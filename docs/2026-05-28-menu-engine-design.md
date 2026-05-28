# Menu Engine + Icons — Design

**Date:** 2026-05-28
**Goal:** Build a reusable, data-driven menu engine for the SH1106 OLED with icon-home / text-submenu layouts, three selection styles, arbitrary nesting depth, and a working demo (Settings / WiFi / Bluetooth with one 3-level branch).

## Context

This is sub-project 2b of the navigation feature. It builds on:

- [`sh1106`](../main/sh1106.c) — pixel-level driver with `sh1106_draw_string`.
- [`font8x8`](../main/font8x8.c) — 8×8 ASCII bitmap font.
- [`buttons`](../main/buttons.c) — debounced 3-button event queue (PRESSED / RELEASED / LONG_PRESS / REPEAT).

The menu engine is designed to drop into other projects unchanged. The application defines a static tree of menus and items; the engine renders them and dispatches actions.

## Architecture

```text
buttons queue ──► menu_handle_event() ──► nav stack + selection state
                                          │
                                          ├──► action callback (caller code)
                                          │
                                          └──► menu_render() ──► framebuffer ──► sh1106_flush()
```

Single in-process, event-driven. No continuous redraw loop — we re-render only when state changes (selection move, descend, ascend, action returns).

New files:

```text
main/
  menu.h        (public API + data types)
  menu.c        (engine: nav stack, event dispatch, layout, selection rendering)
  icons.h       (extern declarations for demo icons)
  icons.c       (bitmap data for icon_settings / icon_wifi / icon_bluetooth, 32×32 each)
  CMakeLists.txt   (modified — add menu.c, icons.c)
  main.c        (modified — defines demo menu tree, wires it up)
```

`menu.h` / `menu.c` is the reusable library. `icons.{h,c}` is demo-specific (callers swap in their own icons in their own projects).

## Data model (`menu.h`)

```c
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "buttons.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef enum {
    MENU_ITEM_END = 0,         // sentinel; arrays end with MENU_END
    MENU_ITEM_ACTION,
    MENU_ITEM_SUBMENU,
} menu_item_kind_t;

typedef struct menu_s menu_t;

typedef struct {
    menu_item_kind_t   kind;
    const char        *label;
    const uint8_t     *icon;            // NULL for text items
    union {
        void (*action)(void *user_ctx);
        const menu_t *submenu;
    } u;
} menu_item_t;

typedef enum { MENU_LAYOUT_LIST, MENU_LAYOUT_ICONS } menu_layout_t;
typedef enum { MENU_SEL_INVERT, MENU_SEL_ARROW, MENU_SEL_BORDER } menu_selection_t;

typedef struct {
    int                icon_w;          // default 32
    int                icon_h;          // default 32
    int                row_height;      // text rows: default 10
    int                title_height;    // default 10
    menu_selection_t   selection;       // default MENU_SEL_INVERT
} menu_style_t;

struct menu_s {
    const char         *title;          // NULL = no title bar
    menu_layout_t       layout;
    const menu_item_t  *items;          // NULL-terminated with MENU_END
    const menu_style_t *style;          // NULL = built-in defaults
};

#define MENU_END { .kind = MENU_ITEM_END }
```

Forward declarations are used so a child can be declared before its parent:

```c
extern const menu_t settings_menu;
static const menu_item_t home_items[] = {
    { .kind = MENU_ITEM_SUBMENU, .label = "Settings",
      .icon = icon_settings, .u.submenu = &settings_menu },
    ...
};
```

## Public API

```c
// Set the root menu and the user context passed to action callbacks.
void menu_init(const menu_t *root, void *user_ctx);

// Drive the engine with one button event. Re-renders if state changed.
// Safe to call from any task; not thread-safe to call concurrently.
void menu_handle_event(button_event_t evt);

// Convenience: spawn a task that xQueueReceives from q and calls menu_handle_event.
void menu_run_task(QueueHandle_t q, UBaseType_t priority);

// Force an immediate redraw (useful from action callbacks that updated state externally).
void menu_redraw(void);
```

## Navigation behavior

- **Nav stack** of `(const menu_t *menu, int index)` frames. Max depth 8 (`#define MENU_MAX_DEPTH 8`). Static array, no allocations.
- **FORWARD** (press or repeat): `index = (index + 1) % count(items)`. Wrap.
- **BACK** (press or repeat): if depth > 1, pop. At root, no-op.
- **ENTER** (press only — not repeat): act on current item.
  - `MENU_ITEM_SUBMENU` → push current frame, set top = `item->u.submenu`, reset index to 0.
  - `MENU_ITEM_ACTION` → call `item->u.action(user_ctx)`. Engine re-renders after the callback returns.
- **RELEASED / LONG_PRESS**: ignored this iteration.
- **REPEAT** events for FORWARD/BACK trigger fast scroll when held. REPEAT events for ENTER are ignored (don't want to retrigger actions accidentally).

`menu_handle_event` is a pure function of the event — render happens at the end if anything changed.

## Rendering

`menu_render()` does the full sequence:

```text
sh1106_clear();
draw_title_bar();             // if menu->title && style.title_height > 0
draw_items();                 // dispatches to layout-specific renderer
sh1106_flush();
```

### `MENU_LAYOUT_LIST` (text submenus)

- Title (8×8) at y = 1, with a horizontal separator line at y = title_height - 1 if title present.
- Items drawn at y = title_height, then `+row_height`, etc.
- **Top-half scroll:** maintain `int scroll_offset` (index of first visible item). If `selected_index < scroll_offset`, set `scroll_offset = selected_index`. If `selected_index >= scroll_offset + visible_rows`, set `scroll_offset = selected_index - visible_rows + 1`. `visible_rows = (SH1106_HEIGHT - title_height) / row_height`.

### `MENU_LAYOUT_ICONS` (icon home screen)

- Title (optional) at top.
- Icons drawn in a horizontal row, vertically centered in the remaining area.
- `n` icons of width `icon_w` get padded: `gap = (SH1106_WIDTH - n*icon_w) / (n + 1)`. First icon's x = gap; second icon's x = gap*2 + icon_w; etc.
- Label of the **selected** icon is drawn below the row in 8×8 text, centered.

### Selection styles

All three are implemented; each menu's `style->selection` picks one. They apply equivalently to LIST and ICONS layouts.

- `MENU_SEL_INVERT`: fill the item's bounding box (text row OR icon cell + padding) with `true` pixels, then XOR the foreground bits when drawing.
- `MENU_SEL_ARROW`: for LIST, draw `>` at x=0 before the label. For ICONS, draw a small downward-pointing 3-pixel triangle above the selected icon.
- `MENU_SEL_BORDER`: 1-pixel rectangle around the bounding box.

Implementation: a helper `void draw_item_bg(int x, int y, int w, int h, menu_selection_t s)` is called before each item is drawn; the item's foreground (text/icon) is then drawn over it, XORed for INVERT.

## Icons

Format: page-major, LSB = top pixel of each page. A 32×32 icon = 4 pages × 32 bytes = 128 bytes.

To render an icon at `(x, y)` (where y is page-aligned, i.e. a multiple of 8):
```c
for (int page = 0; page < 4; page++) {
    for (int col = 0; col < 32; col++) {
        uint8_t bits = icon[page * 32 + col];
        for (int row = 0; row < 8; row++) {
            if (bits & (1 << row)) {
                sh1106_set_pixel(x + col, y + page * 8 + row, true);
            }
        }
    }
}
```

If `y` is not page-aligned, the rendering still works — `sh1106_set_pixel` handles arbitrary y. The page-aligned case is faster but not required.

Demo icons (`icons.c`): `icon_settings` (gear), `icon_wifi` (antenna + arc waves), `icon_bluetooth` (stylized "B" with two diamond bumps). Each defined as a 128-byte array. License: original work, no upstream.

## Demo tree

```text
home (MENU_LAYOUT_ICONS, MENU_SEL_BORDER, title "Home")
├── Settings (icon)
│   ↳ settings_menu (LIST, INVERT, title "Settings")
│       ├── Brightness   action: log "brightness", show toast
│       ├── Contrast     action: log "contrast",   show toast
│       ├── About
│       │   ↳ about_menu (LIST, ARROW, title "About")
│       │       ├── Version   action: log + toast "v0.1"
│       │       └── Uptime    action: log + toast "Up 1m23s"
│       └── Reset        action: log "reset"
├── WiFi (icon)
│   ↳ wifi_menu (LIST, INVERT, title "WiFi")
│       ├── Scan         action
│       ├── Connect      action
│       └── Status       action
└── Bluetooth (icon)
    ↳ bt_menu (LIST, INVERT, title "Bluetooth")
        ├── Pair         action
        └── Devices      action
```

Three different selection styles are exercised across the tree so we can verify all three render correctly.

Action implementation pattern:

```c
static void act_brightness(void *user_ctx)
{
    ESP_LOGI("menu", "brightness");
    menu_toast("Brightness +", 800 /*ms*/);
}
```

`menu_toast(const char *msg, int duration_ms)` is a helper inside `menu.c`: blanks the bottom 8 pixels, draws the message, flushes, `vTaskDelay`s the duration, then calls `menu_redraw()` to restore the menu. Simple, blocking, fine for demo. (`menu_toast` is exposed in `menu.h` because actions in caller code will want it.)

## Demo (definition of done)

Boot:

- OLED shows `Home` title at top, three 32×32 icons in a row below (Settings, WiFi, Bluetooth), with a 1-px border around the first icon (selected by default).
- Pressing FORWARD moves the border to the next icon (and the label below changes).
- Pressing BACK at the home screen does nothing (we're at root).
- Pressing ENTER on `Settings` descends to the settings list with INVERT selection.
- Pressing FORWARD scrolls down the list. Holding FORWARD scrolls fast (via REPEAT events).
- Pressing ENTER on `About` descends another level (3rd level total).
- Pressing BACK pops back up.
- Pressing ENTER on `Brightness` logs and shows a brief toast, then returns to the settings list with selection preserved.

## Non-goals

- No "toggle" / "slider" / "value editor" item types — actions only.
- No animations or screen transitions.
- No multiple home pages.
- No persistence of selection / nav state across reboots.
- No async actions (callbacks block the menu task during their `menu_toast` delay).
- No localization / i18n.
- No icon loading from filesystem — pure compiled-in data.

## Risks

- **Hand-drawn 32×32 icons.** Drawing pixel art is time-consuming. We accept some ugliness for the demo; user can replace them later. Production projects will define their own icons via the same `extern const uint8_t my_icon[128];` pattern.
- **Three selection styles = three code paths.** Each is small (~30 lines), but it's roughly 3× the rendering work of choosing one. Spec'd because the user explicitly wanted all three configurable.
- **Forward declarations for the recursive tree.** Standard C pattern: `extern const menu_t settings_menu;` declared before `home_items[]`, with the actual definition further down. The demo will show this pattern clearly.
- **No re-entrancy.** `menu_handle_event` is not safe to call concurrently from multiple tasks. The intended caller is a single consumer task draining the button queue. Action callbacks that need to update other tasks should post to their own queues.
- **`menu_toast` blocks the menu task.** Long toasts will queue up button events. Default toast duration is short (800 ms); button events queued during a toast will replay against the post-toast menu state, which is reasonable.

## Future hooks (not for this iteration)

- Additional item kinds: `MENU_ITEM_TOGGLE`, `MENU_ITEM_VALUE` (numeric, with FORWARD/BACK adjusting the value in-place).
- Custom item render callback: `void (*render)(int x, int y, bool selected)` for items that want full control.
- Configurable scroll style per menu (top-half / center-fixed / paged).
- Soft animation (e.g. arrow slides, fade between screens).
