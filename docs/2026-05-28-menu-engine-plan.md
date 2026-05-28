# Menu Engine + Icons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a reusable data-driven menu engine for the SH1106 OLED that supports recursive submenus, text-list and icon-row layouts, three selection styles, top-half scrolling, action callbacks, and a 3-level demo tree (Settings / WiFi / Bluetooth, with Settings → About → Version).

**Architecture:** New module `menu.{h,c}` is the reusable engine. Demo-specific `icons.{h,c}` ship three 32×32 bitmaps. `main.c` defines the static menu tree (forward-declared for recursion) and wires it to the existing `buttons` queue. Engine renders only on state change; no continuous redraw.

**Tech Stack:** ESP-IDF v5+, existing `sh1106` driver, `font8x8`, `buttons` module. No new managed components.

**Reference spec:** [2026-05-28-menu-engine-design.md](./2026-05-28-menu-engine-design.md)

**Note on testing:** Same as prior plans — each task verifies with `idf.py build` and, where applicable, `idf.py flash monitor` + physical button presses + visual OLED check.

---

## File Structure

After all tasks complete:

```text
main/
  CMakeLists.txt   (modified — adds menu.c, icons.c)
  menu.h           (new — engine API + data types)
  menu.c           (new — engine implementation)
  icons.h          (new — extern declarations for the 3 demo icons)
  icons.c          (new — 32×32 bitmap data)
  main.c           (modified — defines the demo menu tree)
  buttons.h, buttons.c, sh1106.h, sh1106.c, font8x8.h, font8x8.c   (unchanged)
```

**Responsibilities:**

- `menu.h` — exposes the menu data types (`menu_item_kind_t`, `menu_item_t`, `menu_layout_t`, `menu_selection_t`, `menu_style_t`, `menu_t`), the `MENU_END` sentinel macro, and four functions: `menu_init`, `menu_handle_event`, `menu_run_task`, `menu_toast`, `menu_redraw`.
- `menu.c` — nav stack (max depth 8, static array), event dispatch, both layout renderers, all three selection styles, scroll math, toast helper. Self-contained; only depends on `sh1106` and `buttons` headers.
- `icons.h / icons.c` — pure data: three `const uint8_t [128]` arrays. No logic.
- `main.c` — defines the menu tree (forward-declared recursive structure), demo action callbacks, wires up `buttons_init` + `menu_init` + `menu_run_task`.

---

## Task 1: Menu module skeleton (header, stubs, build green)

**Files:**
- Create: `main/menu.h`
- Create: `main/menu.c`
- Modify: `main/CMakeLists.txt`

Establishes the public API surface as compileable stubs. No behavior yet. Verifies the types and CMake registration before any logic.

- [ ] **Step 1: Create `main/menu.h`**

```c
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "buttons.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef enum {
    MENU_ITEM_END = 0,
    MENU_ITEM_ACTION,
    MENU_ITEM_SUBMENU,
} menu_item_kind_t;

typedef struct menu_s menu_t;

typedef struct {
    menu_item_kind_t   kind;
    const char        *label;
    const uint8_t     *icon;
    union {
        void (*action)(void *user_ctx);
        const menu_t *submenu;
    } u;
} menu_item_t;

typedef enum { MENU_LAYOUT_LIST, MENU_LAYOUT_ICONS } menu_layout_t;
typedef enum { MENU_SEL_INVERT, MENU_SEL_ARROW, MENU_SEL_BORDER } menu_selection_t;

typedef struct {
    int                icon_w;
    int                icon_h;
    int                row_height;
    int                title_height;
    menu_selection_t   selection;
} menu_style_t;

struct menu_s {
    const char         *title;
    menu_layout_t       layout;
    const menu_item_t  *items;
    const menu_style_t *style;
};

#define MENU_END { .kind = MENU_ITEM_END }

void menu_init(const menu_t *root, void *user_ctx);
void menu_handle_event(button_event_t evt);
void menu_run_task(QueueHandle_t q, UBaseType_t priority);
void menu_redraw(void);
void menu_toast(const char *msg, int duration_ms);
```

- [ ] **Step 2: Create `main/menu.c` with stubs**

```c
#include "menu.h"

#include "esp_log.h"

static const char *TAG = "menu";

void menu_init(const menu_t *root, void *user_ctx)
{
    (void)root; (void)user_ctx;
    ESP_LOGI(TAG, "menu_init (stub)");
}

void menu_handle_event(button_event_t evt) { (void)evt; }
void menu_run_task(QueueHandle_t q, UBaseType_t priority) { (void)q; (void)priority; }
void menu_redraw(void) {}
void menu_toast(const char *msg, int duration_ms) { (void)msg; (void)duration_ms; }
```

- [ ] **Step 3: Register menu.c in CMake**

Modify `main/CMakeLists.txt`:

```cmake
idf_component_register(SRCS "sh1106.c" "main.c" "font8x8.c" "buttons.c" "menu.c"
                    INCLUDE_DIRS ".")
```

- [ ] **Step 4: Verify build**

Run: `idf.py build`
Expected: succeeds. No new warnings.

- [ ] **Step 5: Commit**

```bash
git add main/menu.h main/menu.c main/CMakeLists.txt
git commit -m "feat: menu module skeleton with API stubs"
```

---

## Task 2: Core engine — LIST layout, FORWARD/BACK selection, INVERT style

**Files:**
- Modify: `main/menu.c`
- Modify: `main/main.c`

Brings the engine to life with the smallest visible feature: a flat single-screen LIST menu where FORWARD/BACK move a highlighted (inverted) row. No nesting, no ENTER action, no title, no icons yet.

- [ ] **Step 1: Replace `main/menu.c` with the core implementation**

```c
#include "menu.h"

#include <string.h>

#include "esp_log.h"
#include "sh1106.h"

static const char *TAG = "menu";

#define MENU_MAX_DEPTH 8

typedef struct {
    const menu_t *menu;
    int           index;
    int           scroll_offset;
} frame_t;

static frame_t  s_stack[MENU_MAX_DEPTH];
static int      s_depth;          // 0 if uninitialized; 1+ = frames in use
static void    *s_user_ctx;

// Defaults used when menu->style is NULL.
static const menu_style_t s_default_style = {
    .icon_w        = 32,
    .icon_h        = 32,
    .row_height    = 10,
    .title_height  = 10,
    .selection     = MENU_SEL_INVERT,
};

static const menu_style_t *style_of(const menu_t *m)
{
    return m->style ? m->style : &s_default_style;
}

static int item_count(const menu_item_t *items)
{
    int n = 0;
    while (items[n].kind != MENU_ITEM_END) n++;
    return n;
}

// Invert all pixels inside a rectangle of the framebuffer.
static void invert_rect(int x, int y, int w, int h)
{
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            // toggle: set_pixel doesn't expose XOR, so read-modify isn't possible without
            // touching the framebuffer directly. As a workaround we approximate INVERT by
            // pre-filling the rect white and drawing text via XOR-like overdraw. For text-only
            // this works because text pixels land on top of the filled rectangle and we
            // selectively clear them. Implementation: fill rect white, then in the text path
            // clear pixels where the glyph has them set.
            sh1106_set_pixel(xx, yy, true);
        }
    }
}

// Draw a string in "inverse" mode: assumes background was already filled white via invert_rect.
// Clears (sets to false) the pixels corresponding to glyph bits.
static void draw_string_inverse(int x, int y, const char *s)
{
    extern const uint8_t font8x8[96][8];  // from font8x8.h, re-declared here to avoid header churn
    while (*s) {
        unsigned char uc = (unsigned char)(*s++);
        if (uc < 32 || uc > 127) uc = '?';
        const uint8_t *glyph = font8x8[uc - 32];
        for (int col = 0; col < 8; col++) {
            uint8_t bits = glyph[col];
            for (int row = 0; row < 8; row++) {
                if (bits & (1 << row)) {
                    sh1106_set_pixel(x + col, y + row, false);  // clear, since bg is white
                }
            }
        }
        x += 8;
    }
}

static void render_list(const menu_t *m, const frame_t *f)
{
    const menu_style_t *st = style_of(m);
    int n = item_count(m->items);
    int y = 0;  // no title bar yet (Task 5 adds it)
    for (int i = 0; i < n; i++) {
        bool selected = (i == f->index);
        if (selected && st->selection == MENU_SEL_INVERT) {
            invert_rect(0, y, SH1106_WIDTH, st->row_height);
            draw_string_inverse(2, y + 1, m->items[i].label);
        } else {
            sh1106_draw_string(2, y + 1, m->items[i].label);
        }
        y += st->row_height;
    }
}

static void render(void)
{
    if (s_depth == 0) return;
    sh1106_clear();
    const frame_t *f = &s_stack[s_depth - 1];
    if (f->menu->layout == MENU_LAYOUT_LIST) {
        render_list(f->menu, f);
    }
    sh1106_flush();
}

void menu_init(const menu_t *root, void *user_ctx)
{
    s_user_ctx = user_ctx;
    s_depth = 1;
    s_stack[0].menu          = root;
    s_stack[0].index         = 0;
    s_stack[0].scroll_offset = 0;
    ESP_LOGI(TAG, "menu_init: root=%p", root);
    render();
}

void menu_handle_event(button_event_t evt)
{
    if (s_depth == 0) return;
    frame_t *f = &s_stack[s_depth - 1];
    int n = item_count(f->menu->items);
    if (n == 0) return;

    bool changed = false;
    bool is_press_or_repeat =
        (evt.event == BTN_EVT_PRESSED) || (evt.event == BTN_EVT_REPEAT);

    if (!is_press_or_repeat) return;  // ignore RELEASED, LONG_PRESS for now

    switch (evt.button) {
    case BTN_FORWARD:
        f->index = (f->index + 1) % n;
        changed = true;
        break;
    case BTN_BACK:
        f->index = (f->index - 1 + n) % n;  // Task 3 will change this to "pop stack"
        changed = true;
        break;
    case BTN_ENTER:
        // No-op for now — Task 3/4 add submenu + action handling.
        break;
    default:
        break;
    }

    if (changed) render();
}

void menu_run_task_fn(void *arg);  // forward decl

void menu_run_task(QueueHandle_t q, UBaseType_t priority)
{
    xTaskCreate(menu_run_task_fn, "menu", 4096, (void *)q, priority, NULL);
}

void menu_run_task_fn(void *arg)
{
    QueueHandle_t q = (QueueHandle_t)arg;
    button_event_t evt;
    while (1) {
        if (xQueueReceive(q, &evt, portMAX_DELAY) == pdTRUE) {
            menu_handle_event(evt);
        }
    }
}

void menu_redraw(void) { render(); }
void menu_toast(const char *msg, int duration_ms) { (void)msg; (void)duration_ms; }
```

Two implementation notes baked into the code above:

- The INVERT selection is implemented as a "fill white, then knock out glyph pixels". This avoids needing an XOR primitive in `sh1106`. `draw_string_inverse` re-declares the `font8x8` extern locally rather than adding a `font8x8.h` include to `menu.c` for a single helper (we'll clean this up if it becomes a pattern).
- The `BACK` button currently moves selection backward — temporary so we can verify wrap-around in both directions before nav-stack pop arrives in Task 3.

- [ ] **Step 2: Replace `main/main.c` with a tiny flat-menu demo**

```c
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
```

- [ ] **Step 3: Build, flash, verify**

Run: `idf.py build flash monitor`
Expected:

- OLED shows three rows: `Option A`, `Option B`, `Option C`, with row 0 inverted (white bar with black text).
- FORWARD button → next row inverted, wrapping back to row 0 after row 2.
- BACK button → previous row inverted, wrapping the other direction (this is temporary, will become "go back" in Task 3).
- Holding FORWARD or BACK → fast scroll via REPEAT events.
- ENTER button → no visible effect (Task 3 adds it).
- No flicker, no garbled glyphs.

- [ ] **Step 4: Commit**

```bash
git add main/menu.c main/main.c
git commit -m "feat: menu engine core with LIST layout and INVERT selection"
```

---

## Task 3: ENTER descends into submenu; BACK pops nav stack

**Files:**
- Modify: `main/menu.c`
- Modify: `main/main.c`

Activates the recursion. ENTER on a SUBMENU item pushes a new frame; BACK pops. We replace the temporary "BACK = move selection up" behavior with the real "BACK = go up one level".

- [ ] **Step 1: Update `menu_handle_event` in `main/menu.c`**

Replace the `switch (evt.button)` block with:

```c
    switch (evt.button) {
    case BTN_FORWARD:
        f->index = (f->index + 1) % n;
        changed = true;
        break;
    case BTN_BACK:
        if (s_depth > 1) {
            s_depth--;
            changed = true;
        }
        // At root: silently no-op.
        break;
    case BTN_ENTER:
        if (evt.event == BTN_EVT_REPEAT) break;  // don't retrigger on hold
        {
            const menu_item_t *it = &f->menu->items[f->index];
            if (it->kind == MENU_ITEM_SUBMENU && s_depth < MENU_MAX_DEPTH) {
                s_stack[s_depth].menu          = it->u.submenu;
                s_stack[s_depth].index         = 0;
                s_stack[s_depth].scroll_offset = 0;
                s_depth++;
                changed = true;
            }
            // ACTION handling comes in Task 4.
        }
        break;
    default:
        break;
    }
```

- [ ] **Step 2: Update `main/main.c` to a 2-level menu**

```c
#include "buttons.h"
#include "menu.h"
#include "sh1106.h"

#include "esp_log.h"

static const menu_item_t settings_items[] = {
    { .kind = MENU_ITEM_ACTION, .label = "Brightness" },
    { .kind = MENU_ITEM_ACTION, .label = "Contrast" },
    { .kind = MENU_ITEM_ACTION, .label = "Reset" },
    MENU_END,
};
static const menu_t settings_menu = {
    .title = NULL, .layout = MENU_LAYOUT_LIST, .items = settings_items, .style = NULL,
};

static const menu_item_t home_items[] = {
    { .kind = MENU_ITEM_SUBMENU, .label = "Settings", .u.submenu = &settings_menu },
    { .kind = MENU_ITEM_ACTION,  .label = "About" },
    MENU_END,
};
static const menu_t home = {
    .title = NULL, .layout = MENU_LAYOUT_LIST, .items = home_items, .style = NULL,
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
```

- [ ] **Step 3: Build, flash, verify**

Run: `idf.py build flash monitor`
Expected:

- Home menu shows `Settings`, `About`.
- FORWARD wraps between the two rows.
- ENTER on `Settings` → screen changes to `Brightness`, `Contrast`, `Reset`.
- FORWARD wraps within the settings menu.
- BACK from settings → returns to home, with `Settings` still selected.
- ENTER on `About` → no change yet (no submenu and no action defined).
- BACK on home → no change (we're at root).

- [ ] **Step 4: Commit**

```bash
git add main/menu.c main/main.c
git commit -m "feat: menu navigation via ENTER/BACK with nav stack"
```

---

## Task 4: Action items + `menu_toast`

**Files:**
- Modify: `main/menu.c`
- Modify: `main/main.c`

Wires up action callbacks and the toast helper for visual feedback. After this, pressing ENTER on a leaf item actually does something.

- [ ] **Step 1: Implement `menu_toast` in `main/menu.c`**

Replace the stub `menu_toast` with:

```c
void menu_toast(const char *msg, int duration_ms)
{
    // Save: nothing — we just redraw after.
    // Draw the toast as inverted text spanning the bottom 8 rows.
    invert_rect(0, SH1106_HEIGHT - 10, SH1106_WIDTH, 10);
    draw_string_inverse(2, SH1106_HEIGHT - 9, msg);
    sh1106_flush();
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    render();  // restore the menu
}
```

- [ ] **Step 2: Invoke actions in the ENTER handler**

In `menu_handle_event`, inside the `BTN_ENTER` block, add an `else if` for actions:

```c
            if (it->kind == MENU_ITEM_SUBMENU && s_depth < MENU_MAX_DEPTH) {
                s_stack[s_depth].menu          = it->u.submenu;
                s_stack[s_depth].index         = 0;
                s_stack[s_depth].scroll_offset = 0;
                s_depth++;
                changed = true;
            } else if (it->kind == MENU_ITEM_ACTION && it->u.action != NULL) {
                it->u.action(s_user_ctx);
                changed = true;  // re-render after the action returns
            }
```

- [ ] **Step 3: Add action callbacks in `main/main.c`**

Above `settings_items`, add:

```c
static void act_brightness(void *ctx) { (void)ctx; menu_toast("Brightness +", 600); }
static void act_contrast  (void *ctx) { (void)ctx; menu_toast("Contrast +",   600); }
static void act_reset     (void *ctx) { (void)ctx; menu_toast("Reset!",       600); }
static void act_about     (void *ctx) { (void)ctx; menu_toast("v0.1",         600); }
```

Wire them up:

```c
static const menu_item_t settings_items[] = {
    { .kind = MENU_ITEM_ACTION, .label = "Brightness", .u.action = act_brightness },
    { .kind = MENU_ITEM_ACTION, .label = "Contrast",   .u.action = act_contrast },
    { .kind = MENU_ITEM_ACTION, .label = "Reset",      .u.action = act_reset },
    MENU_END,
};
```

And:

```c
static const menu_item_t home_items[] = {
    { .kind = MENU_ITEM_SUBMENU, .label = "Settings", .u.submenu = &settings_menu },
    { .kind = MENU_ITEM_ACTION,  .label = "About",    .u.action = act_about },
    MENU_END,
};
```

- [ ] **Step 4: Build, flash, verify**

Run: `idf.py build flash monitor`
Expected:

- ENTER on `Brightness` → inverted toast `Brightness +` appears at the bottom for ~600 ms, then the settings menu re-renders with `Brightness` still selected.
- Same for `Contrast`, `Reset`, `About`.
- Holding ENTER does NOT retrigger the action (REPEAT events for ENTER are ignored).

- [ ] **Step 5: Commit**

```bash
git add main/menu.c main/main.c
git commit -m "feat: action items and menu_toast helper"
```

---

## Task 5: Title bar

**Files:**
- Modify: `main/menu.c`
- Modify: `main/main.c`

Adds an optional title at the top of each menu plus a separator line.

- [ ] **Step 1: Update `render_list` in `main/menu.c`**

Replace `render_list` with:

```c
static int draw_title(const menu_t *m)
{
    const menu_style_t *st = style_of(m);
    if (m->title == NULL || st->title_height <= 0) return 0;

    sh1106_draw_string(2, 1, m->title);
    // Separator line at the bottom of the title region.
    for (int x = 0; x < SH1106_WIDTH; x++) {
        sh1106_set_pixel(x, st->title_height - 1, true);
    }
    return st->title_height;
}

static void render_list(const menu_t *m, const frame_t *f)
{
    const menu_style_t *st = style_of(m);
    int n = item_count(m->items);
    int y = draw_title(m);
    for (int i = 0; i < n; i++) {
        bool selected = (i == f->index);
        if (selected && st->selection == MENU_SEL_INVERT) {
            invert_rect(0, y, SH1106_WIDTH, st->row_height);
            draw_string_inverse(2, y + 1, m->items[i].label);
        } else {
            sh1106_draw_string(2, y + 1, m->items[i].label);
        }
        y += st->row_height;
    }
}
```

- [ ] **Step 2: Add titles in `main/main.c`**

Update the two menu definitions:

```c
static const menu_t settings_menu = {
    .title = "Settings", .layout = MENU_LAYOUT_LIST, .items = settings_items, .style = NULL,
};
...
static const menu_t home = {
    .title = "Home", .layout = MENU_LAYOUT_LIST, .items = home_items, .style = NULL,
};
```

- [ ] **Step 3: Build, flash, verify**

Run: `idf.py build flash monitor`
Expected:

- Home screen: `Home` title on row 0, a horizontal pixel line at y=9, then `Settings` / `About` rows below it.
- Settings screen: `Settings` title at top with the same separator line.
- Selection rendering is unaffected — first item below the separator is still highlighted when selected.

- [ ] **Step 4: Commit**

```bash
git add main/menu.c main/main.c
git commit -m "feat: optional title bar with separator line"
```

---

## Task 6: Top-half scroll for long lists

**Files:**
- Modify: `main/menu.c`
- Modify: `main/main.c`

Long submenus (more items than fit on screen) scroll so the selection stays visible. The selection appears in the upper half of the viewport; the list slides up underneath.

- [ ] **Step 1: Add scroll logic in `render_list` in `main/menu.c`**

Replace `render_list` with:

```c
static void render_list(const menu_t *m, const frame_t *f_in)
{
    const menu_style_t *st = style_of(m);
    int n = item_count(m->items);

    int y_start = 0;
    if (m->title) y_start = draw_title(m);

    int visible_rows = (SH1106_HEIGHT - y_start) / st->row_height;
    if (visible_rows < 1) visible_rows = 1;

    // Mutable copy of the frame so we can update its scroll_offset without
    // making the parameter non-const at the callsite chain.
    frame_t *f = (frame_t *)f_in;

    // Top-half scroll: keep selection within [scroll_offset, scroll_offset + visible_rows - 1].
    if (f->index < f->scroll_offset)
        f->scroll_offset = f->index;
    if (f->index >= f->scroll_offset + visible_rows)
        f->scroll_offset = f->index - visible_rows + 1;

    int y = y_start;
    for (int i = f->scroll_offset; i < n && i < f->scroll_offset + visible_rows; i++) {
        bool selected = (i == f->index);
        if (selected && st->selection == MENU_SEL_INVERT) {
            invert_rect(0, y, SH1106_WIDTH, st->row_height);
            draw_string_inverse(2, y + 1, m->items[i].label);
        } else {
            sh1106_draw_string(2, y + 1, m->items[i].label);
        }
        y += st->row_height;
    }
}
```

(The `const`-cast trick is intentional — `render` passes a `const frame_t *` to `render_list` but we want to mutate `scroll_offset` for the cached value. Since the underlying storage is the static `s_stack` array, the cast is safe.)

- [ ] **Step 2: Add a long submenu in `main/main.c`**

Add a long-items menu before the `settings_items` definition:

```c
static void act_log(void *ctx) { (void)ctx; menu_toast("OK", 400); }

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
```

Add it to `home_items`:

```c
static const menu_item_t home_items[] = {
    { .kind = MENU_ITEM_SUBMENU, .label = "Settings", .u.submenu = &settings_menu },
    { .kind = MENU_ITEM_SUBMENU, .label = "Long List", .u.submenu = &long_menu },
    { .kind = MENU_ITEM_ACTION,  .label = "About",    .u.action = act_about },
    MENU_END,
};
```

- [ ] **Step 3: Build, flash, verify**

Run: `idf.py build flash monitor`
Expected:

- Entering `Long List` shows items 1–5 (about 5 visible rows with a title).
- FORWARD past row 5 → list scrolls; selection stays in view.
- BACK pops back to home (not "scroll up" — that's still FORWARD wrapping behaviour for now, but at the bottom of the list, FORWARD wraps to item 1 and scroll_offset resets).
- Holding FORWARD scrolls quickly through all 10 items.

- [ ] **Step 4: Commit**

```bash
git add main/menu.c main/main.c
git commit -m "feat: top-half scroll for long text lists"
```

---

## Task 7: ARROW selection style

**Files:**
- Modify: `main/menu.c`
- Modify: `main/main.c`

Adds the `>` cursor selection style as an alternative to INVERT. Caller picks via `menu_style_t.selection`.

- [ ] **Step 1: Update `render_list` in `main/menu.c` to handle ARROW**

Inside the per-item loop in `render_list`, replace the `if (selected && ... INVERT) ... else ...` with:

```c
        bool selected = (i == f->index);
        const char *arrow = (selected && st->selection == MENU_SEL_ARROW) ? ">" : " ";
        int text_x = 2;
        if (st->selection == MENU_SEL_ARROW) text_x = 10;  // make room for arrow

        if (selected && st->selection == MENU_SEL_INVERT) {
            invert_rect(0, y, SH1106_WIDTH, st->row_height);
            draw_string_inverse(text_x, y + 1, m->items[i].label);
        } else {
            if (st->selection == MENU_SEL_ARROW) {
                sh1106_draw_string(2, y + 1, arrow);
            }
            sh1106_draw_string(text_x, y + 1, m->items[i].label);
        }
```

- [ ] **Step 2: Use ARROW on one submenu in `main/main.c`**

Add a style above the menu definitions:

```c
static const menu_style_t arrow_style = {
    .icon_w = 32, .icon_h = 32, .row_height = 10, .title_height = 10,
    .selection = MENU_SEL_ARROW,
};
```

Apply it to `long_menu`:

```c
static const menu_t long_menu = {
    .title = "Long List", .layout = MENU_LAYOUT_LIST, .items = long_items, .style = &arrow_style,
};
```

- [ ] **Step 3: Build, flash, verify**

Run: `idf.py build flash monitor`
Expected:

- `Settings` (no style → default INVERT) still uses inverted bars.
- `Long List` shows a `>` cursor next to the selected item; other rows have a leading space.
- FORWARD moves the cursor; scroll still works correctly.

- [ ] **Step 4: Commit**

```bash
git add main/menu.c main/main.c
git commit -m "feat: ARROW selection style"
```

---

## Task 8: BORDER selection style

**Files:**
- Modify: `main/menu.c`
- Modify: `main/main.c`

Adds the 1-pixel rectangle border around the selected item.

- [ ] **Step 1: Add a `draw_rect` helper in `main/menu.c`**

Add above `render_list`:

```c
static void draw_rect(int x, int y, int w, int h)
{
    for (int xx = x; xx < x + w; xx++) {
        sh1106_set_pixel(xx, y, true);
        sh1106_set_pixel(xx, y + h - 1, true);
    }
    for (int yy = y; yy < y + h; yy++) {
        sh1106_set_pixel(x, yy, true);
        sh1106_set_pixel(x + w - 1, yy, true);
    }
}
```

- [ ] **Step 2: Update `render_list` to handle BORDER**

Replace the per-item drawing block with:

```c
        bool selected = (i == f->index);
        int text_x = 2;
        if (st->selection == MENU_SEL_ARROW) text_x = 10;

        if (selected && st->selection == MENU_SEL_INVERT) {
            invert_rect(0, y, SH1106_WIDTH, st->row_height);
            draw_string_inverse(text_x, y + 1, m->items[i].label);
        } else {
            if (selected && st->selection == MENU_SEL_ARROW) {
                sh1106_draw_string(2, y + 1, ">");
            }
            sh1106_draw_string(text_x, y + 1, m->items[i].label);
            if (selected && st->selection == MENU_SEL_BORDER) {
                draw_rect(0, y, SH1106_WIDTH, st->row_height);
            }
        }
```

- [ ] **Step 3: Use BORDER on a submenu in `main/main.c`**

Add another style:

```c
static const menu_style_t border_style = {
    .icon_w = 32, .icon_h = 32, .row_height = 10, .title_height = 10,
    .selection = MENU_SEL_BORDER,
};
```

Use it on `settings_menu`:

```c
static const menu_t settings_menu = {
    .title = "Settings", .layout = MENU_LAYOUT_LIST,
    .items = settings_items, .style = &border_style,
};
```

- [ ] **Step 4: Build, flash, verify**

Run: `idf.py build flash monitor`
Expected:

- `Home` uses INVERT (default).
- `Settings` uses a 1-pixel rectangle around the selected row.
- `Long List` uses `>` cursor.
- All three coexist in the same demo, switched purely by `menu_style_t`.

- [ ] **Step 5: Commit**

```bash
git add main/menu.c main/main.c
git commit -m "feat: BORDER selection style"
```

---

## Task 9: Icon rendering primitive + first test icon

**Files:**
- Create: `main/icons.h`
- Create: `main/icons.c`
- Modify: `main/CMakeLists.txt`
- Modify: `main/menu.c`

Adds the icon data format and a renderer that draws a 32×32 icon at (x, y). Uses a single placeholder (a 32×32 checkerboard) for verification. Real icons come in Task 10.

- [ ] **Step 1: Create `main/icons.h`**

```c
#pragma once

#include <stdint.h>

// 32×32 monochrome icons, page-major: 4 pages × 32 bytes = 128 bytes per icon.
// Byte index = page * 32 + col. LSB of each byte = topmost pixel of that page.
extern const uint8_t icon_test[128];
```

- [ ] **Step 2: Create `main/icons.c` with a checkerboard test pattern**

```c
#include "icons.h"

// 32x32 checkerboard: each byte = column of 8 vertical pixels.
// Pattern: every other 4-pixel block alternates. Each row's byte value depends
// on (col / 4 + page) parity. Within a page (8 rows), the top 4 rows and bottom
// 4 rows alternate based on column block.
const uint8_t icon_test[128] = {
    // Page 0 (rows 0-7): top 4 rows pattern
    // For columns 0-3 (block 0): rows 0-3 ON, rows 4-7 OFF -> 0x0F
    // For columns 4-7 (block 1): rows 0-3 OFF, rows 4-7 ON -> 0xF0
    // Repeat across 32 columns.
    0x0F, 0x0F, 0x0F, 0x0F, 0xF0, 0xF0, 0xF0, 0xF0,
    0x0F, 0x0F, 0x0F, 0x0F, 0xF0, 0xF0, 0xF0, 0xF0,
    0x0F, 0x0F, 0x0F, 0x0F, 0xF0, 0xF0, 0xF0, 0xF0,
    0x0F, 0x0F, 0x0F, 0x0F, 0xF0, 0xF0, 0xF0, 0xF0,
    // Page 1 (rows 8-15): inverted
    0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F,
    0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F,
    0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F,
    0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F,
    // Page 2 (rows 16-23): same as page 0
    0x0F, 0x0F, 0x0F, 0x0F, 0xF0, 0xF0, 0xF0, 0xF0,
    0x0F, 0x0F, 0x0F, 0x0F, 0xF0, 0xF0, 0xF0, 0xF0,
    0x0F, 0x0F, 0x0F, 0x0F, 0xF0, 0xF0, 0xF0, 0xF0,
    0x0F, 0x0F, 0x0F, 0x0F, 0xF0, 0xF0, 0xF0, 0xF0,
    // Page 3 (rows 24-31): same as page 1
    0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F,
    0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F,
    0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F,
    0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F,
};
```

- [ ] **Step 3: Register icons.c in CMake**

Modify `main/CMakeLists.txt`:

```cmake
idf_component_register(SRCS "sh1106.c" "main.c" "font8x8.c" "buttons.c" "menu.c" "icons.c"
                    INCLUDE_DIRS ".")
```

- [ ] **Step 4: Add `render_icon` in `main/menu.c`**

Add this helper above `render_list`:

```c
static void render_icon(int x, int y, const uint8_t *icon, int w, int h)
{
    if (icon == NULL) return;
    int pages = h / 8;
    for (int page = 0; page < pages; page++) {
        for (int col = 0; col < w; col++) {
            uint8_t bits = icon[page * w + col];
            for (int row = 0; row < 8; row++) {
                if (bits & (1 << row)) {
                    sh1106_set_pixel(x + col, y + page * 8 + row, true);
                }
            }
        }
    }
}
```

- [ ] **Step 5: Smoke-test by rendering the test icon over the home screen**

For this task we don't yet wire icons into the menu engine. We just verify `render_icon` works.

Temporarily add at the end of `menu_init` (above `render()`):

```c
    // Smoke test for render_icon — to be removed in Task 10.
    extern const uint8_t icon_test[128];
    sh1106_clear();
    render_icon(48, 16, icon_test, 32, 32);  // center-ish
    sh1106_flush();
    vTaskDelay(pdMS_TO_TICKS(1500));
```

(We'll remove this smoke test in Task 10 once real icons are wired up.)

- [ ] **Step 6: Build, flash, verify**

Run: `idf.py build flash monitor`
Expected:

- For ~1.5 seconds after boot, the OLED shows a 32×32 checkerboard pattern centered horizontally.
- Then it transitions to the normal home menu.

- [ ] **Step 7: Commit**

```bash
git add main/icons.h main/icons.c main/CMakeLists.txt main/menu.c
git commit -m "feat: 32x32 icon format and render_icon primitive (test pattern)"
```

---

## Task 10: Three real icons + ICONS layout for home

**Files:**
- Modify: `main/icons.h`
- Modify: `main/icons.c`
- Modify: `main/menu.c`
- Modify: `main/main.c`

Replaces the checkerboard with three demo icons (settings, wifi, bluetooth), implements the ICONS layout, and switches the home menu to use it.

The icon byte values will be authored during execution — the plan documents the format and intent. The executor designs simple-but-recognizable 32×32 pictograms (e.g. concentric circles with notches for a gear; arc waves above a dot for wifi; a vertical bar with two diamond bumps for bluetooth) and pastes the resulting byte arrays into `icons.c`. The shapes don't need to be artistically polished — they need to be visually distinct from each other.

- [ ] **Step 1: Replace `main/icons.h`**

```c
#pragma once

#include <stdint.h>

// 32×32 monochrome icons. Format: page-major, 4 pages × 32 bytes = 128 bytes per icon.
// Byte index = page * 32 + col. LSB of each byte = topmost pixel of that page.

extern const uint8_t icon_settings[128];
extern const uint8_t icon_wifi[128];
extern const uint8_t icon_bluetooth[128];
```

- [ ] **Step 2: Replace `main/icons.c` with three real icons**

Author three visually-distinct 32×32 icons. A simple, reliable approach: use a small Python helper to generate them procedurally (gear via radial math, wifi via arc segments, bluetooth via straight-line construction). Save the resulting C-formatted arrays into `icons.c`. Example structure:

```c
#include "icons.h"

// Settings: a gear with 8 teeth around a hollow center.
const uint8_t icon_settings[128] = {
    /* 128 bytes — generated; see plan notes */
};

// WiFi: three arcs above a dot.
const uint8_t icon_wifi[128] = {
    /* 128 bytes */
};

// Bluetooth: stylized "B" with diamond bumps.
const uint8_t icon_bluetooth[128] = {
    /* 128 bytes */
};
```

(Concrete byte values produced during execution. They must satisfy: each array is exactly 128 bytes; each icon is visually distinguishable from the other two; centered within the 32×32 box so the home screen looks balanced.)

- [ ] **Step 3: Remove the smoke test from `menu_init`**

Delete the temporary block added in Task 9, Step 5 (the 1.5-second checkerboard).

- [ ] **Step 4: Implement `render_icons` layout in `main/menu.c`**

Add this function above `render_list`:

```c
static void render_icons(const menu_t *m, const frame_t *f)
{
    const menu_style_t *st = style_of(m);
    int n = item_count(m->items);

    int y_top = 0;
    if (m->title) y_top = draw_title(m);

    // Compute horizontal spacing so n icons of width icon_w are centered with equal gaps.
    int total_w = n * st->icon_w;
    int gap     = (SH1106_WIDTH - total_w) / (n + 1);
    if (gap < 0) gap = 0;

    // Reserve 9 pixels at the bottom for the selected item's label.
    int label_h = 9;
    int icons_h = SH1106_HEIGHT - y_top - label_h;
    int icon_y  = y_top + (icons_h - st->icon_h) / 2;
    if (icon_y < y_top) icon_y = y_top;

    for (int i = 0; i < n; i++) {
        int icon_x = gap + i * (st->icon_w + gap);
        render_icon(icon_x, icon_y, m->items[i].icon, st->icon_w, st->icon_h);

        if (i == f->index) {
            if (st->selection == MENU_SEL_INVERT) {
                // XOR effect via fill+knock-out isn't trivial for icons; approximate
                // by drawing a 1-pixel border for INVERT too (visually still distinct).
                draw_rect(icon_x - 1, icon_y - 1, st->icon_w + 2, st->icon_h + 2);
            } else if (st->selection == MENU_SEL_BORDER) {
                draw_rect(icon_x - 2, icon_y - 2, st->icon_w + 4, st->icon_h + 4);
            } else if (st->selection == MENU_SEL_ARROW) {
                // Draw a small triangle below the icon.
                int tx = icon_x + st->icon_w / 2;
                int ty = icon_y + st->icon_h + 1;
                for (int dy = 0; dy < 3; dy++) {
                    for (int dx = -dy; dx <= dy; dx++) {
                        sh1106_set_pixel(tx + dx, ty + dy, true);
                    }
                }
            }
        }
    }

    // Selected item's label at the bottom.
    const char *label = m->items[f->index].label;
    if (label) {
        int label_x = (SH1106_WIDTH - (int)strlen(label) * 8) / 2;
        if (label_x < 0) label_x = 0;
        sh1106_draw_string(label_x, SH1106_HEIGHT - 8, label);
    }
}
```

Update `render` to dispatch to `render_icons`:

```c
static void render(void)
{
    if (s_depth == 0) return;
    sh1106_clear();
    const frame_t *f = &s_stack[s_depth - 1];
    if (f->menu->layout == MENU_LAYOUT_LIST) {
        render_list(f->menu, f);
    } else if (f->menu->layout == MENU_LAYOUT_ICONS) {
        render_icons(f->menu, f);
    }
    sh1106_flush();
}
```

- [ ] **Step 5: Make the home menu use ICONS layout in `main/main.c`**

Update `home_items` to reference icons and `home` to use ICONS layout:

```c
static const menu_item_t home_items[] = {
    { .kind = MENU_ITEM_SUBMENU, .label = "Settings",  .icon = icon_settings,
      .u.submenu = &settings_menu },
    { .kind = MENU_ITEM_SUBMENU, .label = "Long List", .icon = icon_wifi,
      .u.submenu = &long_menu },
    { .kind = MENU_ITEM_ACTION,  .label = "About",     .icon = icon_bluetooth,
      .u.action = act_about },
    MENU_END,
};

static const menu_style_t home_style = {
    .icon_w = 32, .icon_h = 32, .row_height = 10, .title_height = 10,
    .selection = MENU_SEL_BORDER,
};

static const menu_t home = {
    .title = "Home", .layout = MENU_LAYOUT_ICONS, .items = home_items, .style = &home_style,
};
```

Add `#include "icons.h"` at the top of `main/main.c`.

- [ ] **Step 6: Build, flash, verify**

Run: `idf.py build flash monitor`
Expected:

- Home screen shows three 32×32 icons in a horizontal row with a `Home` title above and the selected item's label at the bottom.
- The selected icon has a 1-pixel border around it (BORDER style).
- FORWARD moves the border to the next icon; label updates accordingly.
- ENTER on the first icon descends into `Settings`. BACK returns to home.
- The icons are visually distinguishable (settings ≠ wifi ≠ bluetooth).

- [ ] **Step 7: Commit**

```bash
git add main/icons.h main/icons.c main/menu.c main/main.c
git commit -m "feat: icon row layout with three demo icons"
```

---

## Task 11: Full demo tree with 3-level nesting

**Files:**
- Modify: `main/main.c`

Final task. Builds out the full demo as described in the spec: Home → Settings → About → Version/Uptime.

- [ ] **Step 1: Expand `main/main.c` with the about + bt + wifi submenus**

Add above `settings_items`:

```c
static void act_version(void *ctx) { (void)ctx; menu_toast("v0.1",      600); }
static void act_uptime (void *ctx) { (void)ctx; menu_toast("Up 0m12s",  600); }
static void act_scan   (void *ctx) { (void)ctx; menu_toast("Scanning",  600); }
static void act_connect(void *ctx) { (void)ctx; menu_toast("Connect",   600); }
static void act_status (void *ctx) { (void)ctx; menu_toast("Online",    600); }
static void act_pair   (void *ctx) { (void)ctx; menu_toast("Pairing",   600); }
static void act_devices(void *ctx) { (void)ctx; menu_toast("Devices",   600); }

static const menu_item_t about_items[] = {
    { .kind = MENU_ITEM_ACTION, .label = "Version", .u.action = act_version },
    { .kind = MENU_ITEM_ACTION, .label = "Uptime",  .u.action = act_uptime },
    MENU_END,
};
static const menu_t about_menu = {
    .title = "About", .layout = MENU_LAYOUT_LIST, .items = about_items, .style = &arrow_style,
};

static const menu_item_t wifi_items[] = {
    { .kind = MENU_ITEM_ACTION, .label = "Scan",    .u.action = act_scan },
    { .kind = MENU_ITEM_ACTION, .label = "Connect", .u.action = act_connect },
    { .kind = MENU_ITEM_ACTION, .label = "Status",  .u.action = act_status },
    MENU_END,
};
static const menu_t wifi_menu = {
    .title = "WiFi", .layout = MENU_LAYOUT_LIST, .items = wifi_items, .style = NULL,
};

static const menu_item_t bt_items[] = {
    { .kind = MENU_ITEM_ACTION, .label = "Pair",    .u.action = act_pair },
    { .kind = MENU_ITEM_ACTION, .label = "Devices", .u.action = act_devices },
    MENU_END,
};
static const menu_t bt_menu = {
    .title = "Bluetooth", .layout = MENU_LAYOUT_LIST, .items = bt_items, .style = NULL,
};
```

Update `settings_items` to include the About submenu:

```c
static const menu_item_t settings_items[] = {
    { .kind = MENU_ITEM_ACTION,  .label = "Brightness", .u.action = act_brightness },
    { .kind = MENU_ITEM_ACTION,  .label = "Contrast",   .u.action = act_contrast },
    { .kind = MENU_ITEM_SUBMENU, .label = "About",      .u.submenu = &about_menu },
    { .kind = MENU_ITEM_ACTION,  .label = "Reset",      .u.action = act_reset },
    MENU_END,
};
```

Replace `home_items` to use the real WiFi and Bluetooth submenus (no more `long_menu` placeholder):

```c
static const menu_item_t home_items[] = {
    { .kind = MENU_ITEM_SUBMENU, .label = "Settings",  .icon = icon_settings,  .u.submenu = &settings_menu },
    { .kind = MENU_ITEM_SUBMENU, .label = "WiFi",      .icon = icon_wifi,      .u.submenu = &wifi_menu },
    { .kind = MENU_ITEM_SUBMENU, .label = "Bluetooth", .icon = icon_bluetooth, .u.submenu = &bt_menu },
    MENU_END,
};
```

(The unused `long_menu` and `long_items` definitions can stay in the file as references for the scroll feature, or be removed. Recommended: leave them, mark as `__attribute__((unused))` or just leave — they're static and the compiler will warn but not error.)

If the compiler complains about unused `long_menu`, remove the definitions; the scroll feature itself is fully covered by the menu engine.

- [ ] **Step 2: Build, flash, verify the full demo**

Run: `idf.py build flash monitor`
Expected paths:

- Boot → home shows 3 icons (Settings/WiFi/Bluetooth), border around Settings.
- FORWARD twice → border around Bluetooth, label changes accordingly.
- BACK at home → no-op.
- ENTER on Settings → settings list with BORDER selection: Brightness / Contrast / About / Reset.
- ENTER on About → about list with ARROW selection: Version / Uptime.
- ENTER on Version → toast `v0.1` for 600 ms, then return to about list with Version still selected.
- BACK → settings list. BACK → home.
- ENTER on WiFi → wifi list with INVERT selection (default style).
- All paths return cleanly with selection preserved.

- [ ] **Step 3: Commit**

```bash
git add main/main.c
git commit -m "feat: full demo tree with 3-level nesting and mixed selection styles"
```

- [ ] **Step 4: Milestone reached**

The full menu system is working: 3-button navigation, recursive submenus, icon home + text submenus, three selection styles, scrolling, toasts, and a complete demo. Both modules (`menu`, `icons`) drop into other projects unchanged — just define your own tree, supply your own icons.

---

## Self-review notes

- **Spec coverage:** all sections of the spec map to tasks. Architecture/files → Tasks 1, 9. Data model → Task 1. Public API → Tasks 1–4 (init, handle, run_task) + Task 4 (toast) + Task 9 (icon helpers). Navigation behavior → Tasks 2/3. Rendering → Tasks 2 (list+invert), 5 (title), 6 (scroll), 7 (arrow), 8 (border), 10 (icons layout). Icons → Tasks 9/10. Demo tree → Tasks 3/4/6/10/11. Non-goals respected (no toggles/sliders, no animations, no persistence).
- **Placeholders:** Task 10 Step 2 references "concrete byte values produced during execution" for the three icons. This is intentional — the design step is what each icon *should look like*, the byte-level art is execution work. The plan documents the format (page-major, 128 bytes, distinguishable shapes) and a procedural-generation approach. No other placeholders.
- **Type consistency:** `menu_item_kind_t`, `menu_layout_t`, `menu_selection_t`, `menu_style_t`, `menu_t`, `menu_item_t` defined once in Task 1 and used identically through Task 11. `font8x8[96][8]` re-declared locally in Task 2's `draw_string_inverse` (matches the global definition in `font8x8.h` — fine since the linker resolves to the same symbol). Render helpers (`render_list`, `render_icons`, `draw_title`, `draw_rect`, `invert_rect`, `render_icon`, `draw_string_inverse`) named consistently. `MENU_END`, `MENU_MAX_DEPTH`, `frame_t` defined once and used throughout.
- **Spec risks addressed:** forward-declaration pattern for recursive trees → Task 1 design note + actual use in Task 3 (`extern const menu_t settings_menu;` not needed because of the static array ordering — but if a circular reference appears we'll add it). 3 selection styles = 3 paths → Tasks 7/8 each ~30 lines. Hand-drawn icons → Task 10 acknowledges them as artistic work, lists the procedural approach.
