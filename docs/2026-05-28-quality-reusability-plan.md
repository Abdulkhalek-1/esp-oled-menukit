# Quality & Reusability Pass Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the monolithic `main/`-only project into a polished collection of ESP-IDF components (`components/{sh1106,font8x8,buttons,menu}`) plus a `main/` demo, with `.clang-format` / `.clang-tidy` / `.editorconfig`, code-quality fixes, full Doxygen API docs in headers, and READMEs at the root and per component.

**Architecture:** Library code moves into `components/<name>/{include,src}/` so each library is independently consumable via `EXTRA_COMPONENT_DIRS` or by copying into another project's `components/`. The demo (`main.c`, `icons.{h,c}`) stays under `main/`. ESP-IDF auto-discovers `components/` at the project root, so the root CMakeLists is unchanged.

**Tech Stack:** ESP-IDF v5.5.4, `clang-format`, `clang-tidy` (advisory), Doxygen-style comments (no HTML generation), Markdown.

**Reference spec:** [2026-05-28-quality-reusability-design.md](./2026-05-28-quality-reusability-design.md)

**Note on testing:** Same as prior plans — each task verifies with `idf.py build`, and the four tasks that move runtime code also run `idf.py flash monitor` so we can confirm the demo still works (border + icons + menu navigation behave identically). The plan is structured so any single task's regression is immediately visible.

---

## File Structure

After all tasks complete:

```text
SH1106-learn/
├── .clang-format, .clang-tidy, .editorconfig, .clangd, .gitignore   (root configs)
├── .devcontainer/, .vscode/, .git/, .cache/                          (existing tooling)
├── CMakeLists.txt                                                    (root, unchanged)
├── README.md                                                         (NEW)
├── docs/                                                             (existing)
├── tools/gen_icons.py                                                (existing)
├── components/
│   ├── sh1106/    { CMakeLists.txt, README.md, include/sh1106.h,  src/sh1106.c  }
│   ├── font8x8/   { CMakeLists.txt, README.md, include/font8x8.h, src/font8x8.c }
│   ├── buttons/   { CMakeLists.txt, README.md, include/buttons.h, src/buttons.c }
│   └── menu/      { CMakeLists.txt, README.md, include/menu.h,    src/menu.c    }
└── main/
    ├── CMakeLists.txt   (REQUIRES sh1106 font8x8 buttons menu)
    ├── README.md        (NEW; explains the demo)
    ├── main.c           (demo, unchanged)
    ├── icons.h          (demo-specific data)
    └── icons.c
```

Responsibilities are unchanged from the working code — this plan is purely about structure, style, and docs.

---

## Task 1: `.editorconfig`

**Files:** Create `/.editorconfig`.

- [ ] **Step 1: Create the file**

```ini
root = true

[*]
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true
charset = utf-8
indent_style = space
indent_size = 4

[*.{md,yml,yaml}]
indent_size = 2

[Makefile]
indent_style = tab
```

- [ ] **Step 2: Verify nothing breaks**

Run: `idf.py build`
Expected: succeeds. `.editorconfig` is editor-side only and doesn't affect the build.

- [ ] **Step 3: Commit**

```bash
git add .editorconfig
git commit -m "chore: add .editorconfig"
```

---

## Task 2: `.clang-format`

**Files:** Create `/.clang-format`.

- [ ] **Step 1: Create the file**

```yaml
---
BasedOnStyle: LLVM
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 100
PointerAlignment: Right
AlignConsecutiveAssignments: AcrossEmptyLines
AlignConsecutiveDeclarations: AcrossEmptyLines
AlignTrailingComments: true
SpaceBeforeParens: ControlStatements
BreakBeforeBraces: Custom
BraceWrapping:
  AfterFunction: true
  AfterStruct: false
  AfterEnum: false
  AfterControlStatement: false
AllowShortFunctionsOnASingleLine: Inline
AllowShortIfStatementsOnASingleLine: WithoutElse
SortIncludes: CaseSensitive
IncludeBlocks: Regroup
IncludeCategories:
  - Regex:    '^"[a-z0-9_]+\.h"$'
    Priority: 1
  - Regex:    '^<(stdio|stdint|stdbool|stddef|string|stdlib)\.h>'
    Priority: 2
  - Regex:    '^"(freertos|driver|esp_).+\.h"$'
    Priority: 3
  - Regex:    '.*'
    Priority: 4
```

- [ ] **Step 2: Verify clang-format is available**

Run: `command -v clang-format && clang-format --version`
Expected: prints the path and a version like `clang-format version 17.0.x` (or similar).

If `clang-format` is not installed, install it via the system package manager (`sudo pacman -S clang` on Arch, `apt install clang-format` on Debian/Ubuntu) before continuing. If it cannot be installed, skip Task 3 and add a note to the commit.

- [ ] **Step 3: Commit**

```bash
git add .clang-format
git commit -m "chore: add .clang-format"
```

---

## Task 3: Apply `clang-format` to all current source

**Files:** Modify every `.c` and `.h` under `main/`.

- [ ] **Step 1: Run the formatter in place**

From the project root:

```bash
clang-format -i main/*.c main/*.h
```

Expected: silent. Any diff shown by `git diff --stat` should be whitespace-only or include-reordering.

- [ ] **Step 2: Sanity-check the diff is style-only**

Run: `git diff --stat`
Expected: file names listed with similar +/- counts (additions roughly equal to deletions). If you see new logical changes, the formatter rewrote something we didn't intend — review and decide.

- [ ] **Step 3: Verify build still works**

Run: `idf.py build`
Expected: succeeds. The format pass is supposed to be behavior-preserving.

- [ ] **Step 4: Commit (style-only, separate from logic changes)**

```bash
git add main/*.c main/*.h
git commit -m "style: apply clang-format to all source files"
```

---

## Task 4: `.clang-tidy`

**Files:** Create `/.clang-tidy`.

- [ ] **Step 1: Create the file**

```yaml
---
Checks: >
  -*,
  bugprone-*,
  cert-*,
  -cert-err58-cpp,
  -cert-dcl21-cpp,
  readability-*,
  -readability-braces-around-statements,
  -readability-magic-numbers,
  -readability-identifier-length,
  performance-*,
  clang-analyzer-*

WarningsAsErrors: ''
HeaderFilterRegex: '(components|main)/.*\.h$'
FormatStyle: file
```

- [ ] **Step 2: Verify clang-tidy is available (optional)**

Run: `command -v clang-tidy && clang-tidy --version` (if not installed, that's fine — config still lands).

- [ ] **Step 3: Commit**

```bash
git add .clang-tidy
git commit -m "chore: add .clang-tidy advisory config"
```

---

## Task 5: Code quality — fix `menu.c`'s `font8x8` extern

**Files:** Modify `main/menu.c`.

Inside `menu.c` there's a local `extern const uint8_t font8x8[96][8];` in `draw_string_inverse` that bypasses the proper header. This works today because the two files compile together, but it will break the moment `menu` becomes its own component. Fix now.

- [ ] **Step 1: Add the header include at the top of `menu.c`**

After the existing `#include "menu.h"` block in `main/menu.c`, add:

```c
#include "font8x8.h"
```

- [ ] **Step 2: Remove the local extern**

In `draw_string_inverse`, delete the line:

```c
extern const uint8_t font8x8[96][8];
```

(The reference to `font8x8[...]` below it is now resolved via the proper header.)

- [ ] **Step 3: Build**

Run: `idf.py build`
Expected: succeeds. Same behavior as before.

- [ ] **Step 4: Commit**

```bash
git add main/menu.c
git commit -m "refactor: include font8x8.h instead of local extern"
```

---

## Task 6: Code quality — `sh1106_init` returns `esp_err_t`

**Files:** Modify `main/sh1106.h`, `main/sh1106.c`, `main/main.c`.

Today `sh1106_init` silently ignores SPI/GPIO failures. Promote it to return `esp_err_t` and propagate.

- [ ] **Step 1: Update the public declaration in `main/sh1106.h`**

Replace `void sh1106_init(void);` with:

```c
#include "esp_err.h"

esp_err_t sh1106_init(void);
```

- [ ] **Step 2: Update the definition in `main/sh1106.c`**

Change the signature and propagate failures. Replace the `void sh1106_init(void)` function body with:

```c
esp_err_t sh1106_init(void)
{
    // 1. Configure RES and DC as outputs.
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_RES) | (1ULL << PIN_DC),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) return err;

    // 2. Reset pulse.
    gpio_set_level(PIN_RES, 0);
    esp_rom_delay_us(10);
    gpio_set_level(PIN_RES, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    // 3. SPI bus.
    spi_bus_config_t bus = {
        .mosi_io_num     = PIN_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = PIN_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = SH1106_WIDTH,
    };
    err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return err;

    // 4. Add device.
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000,
        .mode           = 0,
        .spics_io_num   = PIN_CS,
        .queue_size     = 1,
    };
    err = spi_bus_add_device(SPI2_HOST, &devcfg, &dev);
    if (err != ESP_OK) return err;

    // 5. SH1106 power-on init sequence.
    sh1106_cmd(0xAE);
    sh1106_cmd(0xD5); sh1106_cmd(0x80);
    sh1106_cmd(0xA8); sh1106_cmd(0x3F);
    sh1106_cmd(0xD3); sh1106_cmd(0x00);
    sh1106_cmd(0x40);
    sh1106_cmd(0xAD); sh1106_cmd(0x8B);
    sh1106_cmd(0xA1);
    sh1106_cmd(0xC8);
    sh1106_cmd(0xDA); sh1106_cmd(0x12);
    sh1106_cmd(0x81); sh1106_cmd(0x80);
    sh1106_cmd(0xD9); sh1106_cmd(0x22);
    sh1106_cmd(0xDB); sh1106_cmd(0x35);
    sh1106_cmd(0xA4);
    sh1106_cmd(0xA6);
    sh1106_cmd(0xAF);

    return ESP_OK;
}
```

- [ ] **Step 3: Update `main/main.c` to check the return**

Replace the call line `sh1106_init();` with:

```c
    if (sh1106_init() != ESP_OK) {
        ESP_LOGE("main", "sh1106_init failed");
        return;
    }
```

- [ ] **Step 4: Build, flash, verify**

Run: `idf.py build flash monitor`
Expected: identical visual output to before (the demo still works). On boot the menu home screen appears as usual. If init silently fails, the new error path would log and exit `app_main` — verify by reading the monitor for the absence of any `sh1106_init failed` line.

- [ ] **Step 5: Commit**

```bash
git add main/sh1106.h main/sh1106.c main/main.c
git commit -m "refactor: sh1106_init returns esp_err_t and propagates failures"
```

---

## Task 7: Code quality — `_Static_assert` invariants

**Files:** Modify `main/sh1106.c`, `main/font8x8.c`.

Encode layout invariants so the compiler enforces them.

- [ ] **Step 1: Add static asserts to `main/sh1106.c`**

Just below the existing `#define SH1106_COL_OFFSET 2` line, add:

```c
_Static_assert(SH1106_PAGES * 8 == SH1106_HEIGHT,
               "page count must match height");
_Static_assert(SH1106_FB_SIZE == SH1106_WIDTH * SH1106_PAGES,
               "framebuffer size mismatch");
```

- [ ] **Step 2: Add static assert to `main/font8x8.c`**

Just below `#include "font8x8.h"`, add:

```c
_Static_assert(sizeof(font8x8) == 96 * 8,
               "font8x8 must be 96 glyphs of 8 bytes");
```

- [ ] **Step 3: Build**

Run: `idf.py build`
Expected: succeeds. If any assert fails, the compiler reports the assertion message and aborts.

- [ ] **Step 4: Commit**

```bash
git add main/sh1106.c main/font8x8.c
git commit -m "chore: add static_assert layout invariants"
```

---

## Task 8: Code quality — assertions + NULL guards + drop const-cast

**Files:** Modify `main/menu.c`, `main/buttons.c`.

- [ ] **Step 1: Drop the `const`-cast in `render_list`**

In `main/menu.c`, change the `render_list` signature from `const frame_t *f_in` to `frame_t *f` and remove the cast. Replace the existing definition with:

```c
static void render_list(const menu_t *m, frame_t *f)
{
    const menu_style_t *st = style_of(m);
    int n = item_count(m->items);

    int y_start = draw_title(m);
    int visible_rows = (SH1106_HEIGHT - y_start) / st->row_height;
    if (visible_rows < 1) visible_rows = 1;

    if (f->index < f->scroll_offset)
        f->scroll_offset = f->index;
    if (f->index >= f->scroll_offset + visible_rows)
        f->scroll_offset = f->index - visible_rows + 1;

    int y = y_start;
    for (int i = f->scroll_offset; i < n && i < f->scroll_offset + visible_rows; i++) {
        bool selected = (i == f->index);
        int text_x = (st->selection == MENU_SEL_ARROW) ? 10 : 2;

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
        y += st->row_height;
    }
}
```

Also update the corresponding `render_icons` signature to take `frame_t *` (read-only inside, but for consistency). Find:

```c
static void render_icons(const menu_t *m, const frame_t *f)
```

Change to:

```c
static void render_icons(const menu_t *m, frame_t *f)
```

And update the dispatching `render()` function — change the local `const frame_t *f = ...;` to `frame_t *f = ...;`:

```c
static void render(void)
{
    if (s_depth == 0) return;
    sh1106_clear();
    frame_t *f = &s_stack[s_depth - 1];
    if (f->menu->layout == MENU_LAYOUT_LIST) {
        render_list(f->menu, f);
    } else if (f->menu->layout == MENU_LAYOUT_ICONS) {
        render_icons(f->menu, f);
    }
    sh1106_flush();
}
```

- [ ] **Step 2: Add NULL-input assertion to `menu_init` in `main/menu.c`**

Add `#include <assert.h>` near the top of `main/menu.c`. Update `menu_init`:

```c
void menu_init(const menu_t *root, void *user_ctx)
{
    assert(root != NULL);
    s_user_ctx = user_ctx;
    s_depth = 1;
    s_stack[0].menu          = root;
    s_stack[0].index         = 0;
    s_stack[0].scroll_offset = 0;
    ESP_LOGI(TAG, "menu_init: root=%p", root);
    render();
}
```

- [ ] **Step 3: Guard `buttons.c emit()` against NULL queue**

In `main/buttons.c`, replace the `emit` function with:

```c
static void emit(button_id_t b, button_event_type_t ev)
{
    if (s_queue == NULL) return;
    button_event_t evt = {.button = b, .event = ev};
    if (xQueueSendToBack(s_queue, &evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "queue full, dropped event b=%d ev=%d", b, ev);
    }
}
```

- [ ] **Step 4: Build, flash, verify**

Run: `idf.py build flash monitor`
Expected: identical demo behavior. Menu navigation still works, including scroll in `Long List`.

- [ ] **Step 5: Commit**

```bash
git add main/menu.c main/buttons.c
git commit -m "refactor: assertions, NULL guards, drop const-cast"
```

---

## Task 9: Componentize `font8x8`

**Files:**
- Create: `components/font8x8/CMakeLists.txt`
- Create: `components/font8x8/include/font8x8.h` (via `git mv`)
- Create: `components/font8x8/src/font8x8.c` (via `git mv`)
- Modify: `main/CMakeLists.txt`

We start with `font8x8` because it has no dependencies — easiest to isolate first.

- [ ] **Step 1: Create the component directory structure**

```bash
mkdir -p components/font8x8/include components/font8x8/src
```

- [ ] **Step 2: Move the files (preserve git history)**

```bash
git mv main/font8x8.h components/font8x8/include/font8x8.h
git mv main/font8x8.c components/font8x8/src/font8x8.c
```

- [ ] **Step 3: Create `components/font8x8/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS         "src/font8x8.c"
    INCLUDE_DIRS "include"
)
```

- [ ] **Step 4: Update `main/CMakeLists.txt`**

Replace the entire file with:

```cmake
idf_component_register(
    SRCS         "sh1106.c" "main.c" "buttons.c" "menu.c" "icons.c"
    INCLUDE_DIRS "."
    REQUIRES     font8x8
)
```

(Other libraries still in `main/` for now; we'll move them in the next tasks.)

- [ ] **Step 5: Clean and rebuild**

Run: `idf.py fullclean && idf.py build`
Expected: succeeds. font8x8 builds as its own component; main pulls it in.

- [ ] **Step 6: Commit**

```bash
git add components/font8x8 main/CMakeLists.txt
git commit -m "refactor: move font8x8 into components/font8x8"
```

---

## Task 10: Componentize `sh1106`

**Files:**
- Create: `components/sh1106/CMakeLists.txt`
- Create: `components/sh1106/include/sh1106.h` (via `git mv`)
- Create: `components/sh1106/src/sh1106.c` (via `git mv`)
- Modify: `main/CMakeLists.txt`

- [ ] **Step 1: Move the files**

```bash
mkdir -p components/sh1106/include components/sh1106/src
git mv main/sh1106.h components/sh1106/include/sh1106.h
git mv main/sh1106.c components/sh1106/src/sh1106.c
```

- [ ] **Step 2: Create `components/sh1106/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS          "src/sh1106.c"
    INCLUDE_DIRS  "include"
    PRIV_REQUIRES driver esp_rom freertos
)
```

- [ ] **Step 3: Update `main/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS         "main.c" "buttons.c" "menu.c" "icons.c"
    INCLUDE_DIRS "."
    REQUIRES     font8x8 sh1106
)
```

- [ ] **Step 4: Clean and build, flash to verify**

Run: `idf.py fullclean && idf.py build flash monitor`
Expected: demo still works (border + icons + scroll + toasts all visible).

- [ ] **Step 5: Commit**

```bash
git add components/sh1106 main/CMakeLists.txt
git commit -m "refactor: move sh1106 into components/sh1106"
```

---

## Task 11: Componentize `buttons`

**Files:**
- Create: `components/buttons/{CMakeLists.txt,include/buttons.h,src/buttons.c}`
- Modify: `main/CMakeLists.txt`

- [ ] **Step 1: Move the files**

```bash
mkdir -p components/buttons/include components/buttons/src
git mv main/buttons.h components/buttons/include/buttons.h
git mv main/buttons.c components/buttons/src/buttons.c
```

- [ ] **Step 2: Create `components/buttons/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS          "src/buttons.c"
    INCLUDE_DIRS  "include"
    REQUIRES      freertos
    PRIV_REQUIRES driver esp_common
)
```

- [ ] **Step 3: Update `main/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS         "main.c" "menu.c" "icons.c"
    INCLUDE_DIRS "."
    REQUIRES     font8x8 sh1106 buttons
)
```

- [ ] **Step 4: Build, flash, verify**

Run: `idf.py fullclean && idf.py build flash monitor`
Expected: demo unchanged. Press buttons → navigation works.

- [ ] **Step 5: Commit**

```bash
git add components/buttons main/CMakeLists.txt
git commit -m "refactor: move buttons into components/buttons"
```

---

## Task 12: Componentize `menu`

**Files:**
- Create: `components/menu/{CMakeLists.txt,include/menu.h,src/menu.c}`
- Modify: `main/CMakeLists.txt`

Last library to move. After this, `main/` only holds the demo (`main.c`, `icons.{h,c}`).

- [ ] **Step 1: Move the files**

```bash
mkdir -p components/menu/include components/menu/src
git mv main/menu.h components/menu/include/menu.h
git mv main/menu.c components/menu/src/menu.c
```

- [ ] **Step 2: Create `components/menu/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS          "src/menu.c"
    INCLUDE_DIRS  "include"
    REQUIRES      buttons freertos
    PRIV_REQUIRES sh1106 font8x8
)
```

`buttons` and `freertos` are public because `menu.h` declares `void menu_handle_event(button_event_t evt);` and `QueueHandle_t buttons_init(...)`-style references. `sh1106` and `font8x8` are private because they're only used in `menu.c` for rendering.

- [ ] **Step 3: Update `main/CMakeLists.txt` (final form)**

```cmake
idf_component_register(
    SRCS         "main.c" "icons.c"
    INCLUDE_DIRS "."
    REQUIRES     font8x8 sh1106 buttons menu
)
```

- [ ] **Step 4: Build, flash, full integration check**

Run: `idf.py fullclean && idf.py build flash monitor`

Expected — the full demo works exactly as before:

- Boot → home screen with 3 icons, border selection.
- FORWARD walks across icons; label updates at the bottom.
- ENTER on Settings → list with border; ENTER on About → list with arrow; ENTER on Version → toast.
- BACK pops correctly back to home.
- WiFi / Bluetooth submenus accessible from home.

- [ ] **Step 5: Commit**

```bash
git add components/menu main/CMakeLists.txt
git commit -m "refactor: move menu into components/menu"
```

---

## Task 13: Update `.clangd` for the new layout

**Files:** Modify `/.clangd`.

The existing `.clangd` points its compile-commands at `build/`, which is still right after the restructure. But its include search hints need refreshing so the editor finds the new component headers.

- [ ] **Step 1: Inspect current `.clangd`**

Run: `cat .clangd`
The existing content (from earlier in the project) sets `CompileFlags` with `--query-driver` and `--compile-commands-dir`. No changes are required to the compile-commands path because ESP-IDF still writes `build/compile_commands.json` after the restructure.

- [ ] **Step 2: Trigger a fresh build so `compile_commands.json` reflects the new layout**

Run: `idf.py build`
Expected: completes. The build directory now contains a refreshed `compile_commands.json` pointing at `components/*/src/*.c`.

- [ ] **Step 3: Verify clangd picks up the new paths**

Open one of the component source files (e.g. `components/menu/src/menu.c`) in the editor. Wait for clangd to index. You should no longer see "machine/endian.h not found" or "Use of undeclared identifier" errors for symbols defined in sibling components. If you do, restart the clangd LSP from the editor's command palette.

(There's no commit step here unless `.clangd` itself needed changes. If it didn't, just move on.)

---

## Task 14: Root `README.md`

**Files:** Create `/README.md`.

- [ ] **Step 1: Create the file**

```markdown
# SH1106-learn

OLED-driven menu system for ESP32 with a 1.3" SH1106 SPI display and three push buttons.
Built as a set of independent ESP-IDF components, plus a working demo.

## Components

| Component                                            | Purpose                                                                                            | Public API summary                                                |
|------------------------------------------------------|----------------------------------------------------------------------------------------------------|-------------------------------------------------------------------|
| [sh1106](components/sh1106/README.md)                | Hand-rolled SH1106 SPI driver, framebuffer, pixel + 8x8 text drawing.                              | `sh1106_init / clear / set_pixel / draw_string / flush`           |
| [font8x8](components/font8x8/README.md)              | 8x8 ASCII bitmap font (BSD-2-Clause from u8g2).                                                    | `font8x8[96][8]`                                                  |
| [buttons](components/buttons/README.md)              | Debounced 3-button library with PRESSED/RELEASED/LONG_PRESS/REPEAT events on a FreeRTOS queue.     | `buttons_init`                                                    |
| [menu](components/menu/README.md)                    | Recursive data-driven menu engine — two layouts, three selection styles, scrolling, toasts.        | `menu_init / handle_event / run_task / toast / redraw`            |

## Hardware

ESP32 DevKitC / WROOM-32 + 1.3" SH1106 SPI OLED + three momentary push buttons.

OLED (7-pin SPI module):

| OLED pin | ESP32 GPIO |
|----------|------------|
| GND      | GND        |
| VCC      | 3V3        |
| D0 (SCK) | 15         |
| D1 (MOSI)| 2          |
| RES      | 4          |
| DC       | 18         |
| CS       | 5          |

Buttons (one side → GPIO, other side → GND; internal pull-ups enabled):

| Button   | ESP32 GPIO |
|----------|------------|
| BACK     | 25         |
| ENTER    | 33         |
| FORWARD  | 32         |

Both pin sets are software-configurable — see `main/main.c` and each component's README.

## Build and flash the demo

Inside an ESP-IDF v5.5+ environment (devcontainer in `.devcontainer/` works):

```bash
idf.py build flash monitor
```

You should see a `Home` title with three icons (gear, wifi arcs, bluetooth), borders around the selected one, and full navigation via the buttons.

## Using a component in another project

Two equivalent ways:

1. **EXTRA_COMPONENT_DIRS.** In your other project's root `CMakeLists.txt`, before `project(...)`, add:

   ```cmake
   set(EXTRA_COMPONENT_DIRS "/path/to/SH1106-learn/components")
   ```

   Then list the components you want in your `main/CMakeLists.txt`:

   ```cmake
   idf_component_register(SRCS "main.c" REQUIRES menu sh1106 buttons font8x8)
   ```

2. **Copy.** Copy the component folders into your project's own `components/` tree:

   ```bash
   cp -r SH1106-learn/components/{font8x8,sh1106,buttons,menu} my-project/components/
   ```

Each component is fully self-contained — its `CMakeLists.txt` declares its own dependencies via `REQUIRES` / `PRIV_REQUIRES`.

## Project layout

See [docs/](docs/) for the original design specs and implementation plans that produced this code.

```text
.
├── components/{sh1106,font8x8,buttons,menu}/    # the reusable libraries
├── main/                                        # the demo
├── docs/                                        # design specs and plans
└── tools/gen_icons.py                           # procedural icon generator
```

## Status

Personal learning project. The font in `components/font8x8` is vendored from the u8g2 project under BSD-2-Clause — see [components/font8x8/README.md](components/font8x8/README.md) for attribution.
```

- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "docs: add root README"
```

---

## Task 15: `components/font8x8/README.md`

**Files:** Create `components/font8x8/README.md`.

- [ ] **Step 1: Create the file**

```markdown
# font8x8

Vendored 8×8 monochrome bitmap font covering printable ASCII (codepoints 0x20–0x7F).

Source: [u8g2](https://github.com/olikraus/u8g2) — font `u8x8_font_amstrad_cpc_extended_f`, BSD-2-Clause.

## Public API

| Symbol               | Type                | Description                                                                  |
|----------------------|---------------------|------------------------------------------------------------------------------|
| `font8x8`            | `const uint8_t [96][8]` | 96 glyphs, 8 bytes each. Column-major: byte N = column N, LSB = top.    |

Glyph lookup for character `c`: `font8x8[c - 32]`. Out-of-range characters should be substituted with `'?'` by the caller.

## Dependencies

None.

## Usage

```c
#include "font8x8.h"

// Draw 'A' as pixels (assuming a 1-bit framebuffer with a set_pixel(x, y, on)):
const uint8_t *glyph = font8x8['A' - 32];
for (int col = 0; col < 8; col++) {
    uint8_t bits = glyph[col];
    for (int row = 0; row < 8; row++) {
        if (bits & (1 << row)) set_pixel(x + col, y + row, true);
    }
}
```

## Notes

- The original font is 1796 bytes covering 0x20–0xFF; we vendor only the printable-ASCII subset (768 bytes) plus a license header.
- Each glyph is in u8x8's column-major layout. If your rendering loop assumes row-major bytes, you will see 90°-rotated glyphs — adjust accordingly.
- Replacement fonts are easy: drop in any 96×8 byte array with the same layout.
```

- [ ] **Step 2: Commit**

```bash
git add components/font8x8/README.md
git commit -m "docs: README for font8x8 component"
```

---

## Task 16: `components/sh1106/README.md`

**Files:** Create `components/sh1106/README.md`.

- [ ] **Step 1: Create the file**

```markdown
# sh1106

Hand-rolled ESP-IDF driver for SH1106-controller monochrome OLEDs (commonly sold as 1.3" 128×64 modules) over 4-wire SPI. Single-buffered, polling SPI transfers, no DMA, no FreeRTOS task.

## Public API

| Function                                    | Description                                                       |
|---------------------------------------------|-------------------------------------------------------------------|
| `esp_err_t sh1106_init(void)`               | GPIO + HSPI bus setup, sends the SH1106 init sequence.            |
| `void sh1106_clear(void)`                   | Zero the 1024-byte framebuffer (RAM only; doesn't flush).         |
| `void sh1106_set_pixel(int x, int y, bool on)` | Stage one pixel; out-of-range silently ignored.                |
| `void sh1106_draw_string(int x, int y, const char *s)` | Draw 8×8 ASCII text at the given pixel coordinates.    |
| `void sh1106_flush(void)`                   | Push the framebuffer to the OLED page-by-page.                    |

Constants: `SH1106_WIDTH` (128), `SH1106_HEIGHT` (64).

## Dependencies

- `driver` (gpio, spi_master) — private.
- `esp_rom` — private (for `esp_rom_delay_us` in reset pulse).
- `freertos` — private (for `vTaskDelay`).

## Wiring (default pins)

Edit the `#define`s at the top of `src/sh1106.c` to match your wiring.

| OLED pin | ESP32 GPIO (default) |
|----------|----------------------|
| D0 (SCK) | 15                   |
| D1 (MOSI)| 2                    |
| RES      | 4                    |
| DC       | 18                   |
| CS       | 5                    |

## Usage

```c
#include "sh1106.h"

if (sh1106_init() != ESP_OK) { /* handle */ }
sh1106_clear();
sh1106_draw_string(0, 0, "Hello");
sh1106_set_pixel(64, 32, true);
sh1106_flush();
```

## Notes

- The SH1106 controller has 132 columns but only 128 are visible — the driver handles the column offset of 2 automatically.
- Charge-pump enable is `0xAD 0x8B` (SH1106-specific). Do NOT use the SSD1306 sequence `0x8D 0x14` — that yields a blank screen with no error.
- If the display lights up but stays as power-on garbage, check DC/CS wiring. Symptom: init commands work (so the display turns on) but data writes are silently dropped.
```

- [ ] **Step 2: Commit**

```bash
git add components/sh1106/README.md
git commit -m "docs: README for sh1106 component"
```

---

## Task 17: `components/buttons/README.md`

**Files:** Create `components/buttons/README.md`.

- [ ] **Step 1: Create the file**

```markdown
# buttons

Debounced 3-button input library that polls GPIO every 10 ms and emits typed events (PRESSED / RELEASED / LONG_PRESS / REPEAT) on a FreeRTOS queue.

## Public API

| Symbol                                                       | Description                                                                              |
|--------------------------------------------------------------|------------------------------------------------------------------------------------------|
| `enum button_id_t { BTN_BACK, BTN_ENTER, BTN_FORWARD, BTN_COUNT }` | Logical button names.                                                              |
| `enum button_event_type_t { BTN_EVT_PRESSED, BTN_EVT_RELEASED, BTN_EVT_LONG_PRESS, BTN_EVT_REPEAT }` | Event types.                                                          |
| `struct button_event_t { button_id_t button; button_event_type_t event; }` | One event on the queue.                                                |
| `struct buttons_config_t { int pins[3]; int debounce_ms; int long_press_ms; int repeat_interval_ms; }` | Pins + timing.                                                |
| `QueueHandle_t buttons_init(const buttons_config_t *cfg)`    | Set up GPIOs, create the queue, spawn the polling task. Returns NULL on failure.        |

## Dependencies

- `freertos` — public (`buttons_init` returns `QueueHandle_t`).
- `driver` — private (`gpio.h`).
- `esp_common` — private (logging).

## Usage

```c
#include "buttons.h"

buttons_config_t cfg = {
    .pins              = { 25, 33, 32 },  // BACK, ENTER, FORWARD
    .debounce_ms       = 20,
    .long_press_ms     = 500,
    .repeat_interval_ms = 150,
};
QueueHandle_t q = buttons_init(&cfg);
if (q == NULL) { /* handle */ }

// Consumer task:
button_event_t evt;
while (xQueueReceive(q, &evt, portMAX_DELAY) == pdTRUE) {
    // dispatch on (evt.button, evt.event)
}
```

## Notes

- Buttons are active-low; internal pull-ups are enabled by `buttons_init`. External 10k pull-ups are not required but don't hurt for long or noisy wiring.
- The polling task runs at `tskIDLE_PRIORITY + 1` with a 3 KB stack.
- If the consumer task stalls, queue fills after ~16 events (~160 ms of buttons) and further events are dropped — logged as `ESP_LOGW`.
- All three buttons share the same `debounce_ms` / `long_press_ms` / `repeat_interval_ms`; per-button overrides are not supported in this version.
```

- [ ] **Step 2: Commit**

```bash
git add components/buttons/README.md
git commit -m "docs: README for buttons component"
```

---

## Task 18: `components/menu/README.md`

**Files:** Create `components/menu/README.md`.

- [ ] **Step 1: Create the file**

```markdown
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
```

- [ ] **Step 2: Commit**

```bash
git add components/menu/README.md
git commit -m "docs: README for menu component"
```

---

## Task 19: `main/README.md`

**Files:** Create `main/README.md`.

- [ ] **Step 1: Create the file**

```markdown
# main (demo)

This is the demo application that ties the four components together. It is not part of any reusable library — it shows how a project consumes them.

## What it does

Boot → home screen with three icons (Settings / WiFi / Bluetooth) → 3-button navigation into nested submenus → action callbacks fire short toast notifications.

The full menu tree:

```text
Home (icons)
├── Settings ─→ list (border selection)
│   ├── Brightness  → toast
│   ├── Contrast    → toast
│   ├── About ──→ list (arrow selection)
│   │   ├── Version  → toast
│   │   └── Uptime   → toast
│   └── Reset       → toast
├── WiFi ────→ list (invert selection)
│   ├── Scan
│   ├── Connect
│   └── Status
└── Bluetooth → list (invert selection)
    ├── Pair
    └── Devices
```

This exercises both layouts, all three selection styles, three nesting levels, and multiple action callbacks.

## Files

| File          | Purpose                                                                |
|---------------|------------------------------------------------------------------------|
| `main.c`      | Wiring + static menu tree + action callbacks + entry point.            |
| `icons.h`     | Declarations for the three 32×32 demo icons.                           |
| `icons.c`     | Bitmap data for `icon_settings`, `icon_wifi`, `icon_bluetooth`.        |

The icons are generated procedurally by [tools/gen_icons.py](../tools/gen_icons.py). To regenerate (or design replacements), edit that script and run it to produce new bytes for `icons.c`.

## Pin configuration

OLED and button GPIOs are defined in `main.c`. Adjust those values to match your wiring.

## How to extend

Replace the menu tree in `main.c` with your own — the engine is data-driven, so no engine changes are needed. See [../components/menu/README.md](../components/menu/README.md) for the data model.
```

- [ ] **Step 2: Commit**

```bash
git add main/README.md
git commit -m "docs: README for the demo (main/)"
```

---

## Task 20: Doxygen blocks for `sh1106.h` and `font8x8.h`

**Files:**
- Modify: `components/sh1106/include/sh1106.h`
- Modify: `components/font8x8/include/font8x8.h`

- [ ] **Step 1: Replace `components/font8x8/include/font8x8.h`**

```c
#pragma once

#include <stdint.h>

/**
 * @file font8x8.h
 * @brief 8x8 monochrome bitmap font (printable ASCII, vendored from u8g2).
 *
 * Source: u8g2 project, font `u8x8_font_amstrad_cpc_extended_f` (BSD-2-Clause).
 * Layout: column-major. byte N of a glyph = column N (left-to-right);
 *   LSB of each byte = top pixel of the glyph's row, MSB = bottom.
 */

/**
 * @brief Glyph table for printable ASCII (codepoints 0x20-0x7F).
 *
 * To look up a glyph for character @p c:
 *   @code const uint8_t *g = font8x8[c - 32]; @endcode
 *
 * Out-of-range characters should be substituted with `'?'` (index 31) by the caller.
 */
extern const uint8_t font8x8[96][8];
```

- [ ] **Step 2: Replace `components/sh1106/include/sh1106.h`**

```c
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * @file sh1106.h
 * @brief SH1106 SPI OLED driver (128x64 monochrome).
 *
 * Single-buffered, polling SPI, no DMA. The framebuffer is staged in RAM;
 * call sh1106_flush() to push the contents to the display.
 */

/** Visible display width in pixels. */
#define SH1106_WIDTH  128

/** Visible display height in pixels. */
#define SH1106_HEIGHT 64

/**
 * @brief Initialize GPIOs (RES, DC), bring up HSPI, and send the SH1106 power-on sequence.
 *
 * Default pin assignments are baked into `src/sh1106.c`; edit there to match your wiring.
 *
 * @return ESP_OK on success.
 *         An esp_err_t from gpio_config / spi_bus_initialize / spi_bus_add_device on failure.
 */
esp_err_t sh1106_init(void);

/**
 * @brief Zero the framebuffer (in-RAM). Does not push to the display.
 *
 * Call sh1106_flush() afterward to actually clear the screen.
 */
void sh1106_clear(void);

/**
 * @brief Set or clear a single pixel in the framebuffer.
 *
 * Coordinates outside the visible area are silently ignored. The pixel is staged
 * in RAM; call sh1106_flush() to push it to the display.
 *
 * @param x  Column, 0..SH1106_WIDTH-1.
 * @param y  Row, 0..SH1106_HEIGHT-1.
 * @param on true = pixel lit, false = pixel cleared.
 */
void sh1106_set_pixel(int x, int y, bool on);

/**
 * @brief Draw a null-terminated ASCII string in 8x8 glyphs.
 *
 * Each character advances x by 8 pixels. Non-printable characters render as '?'.
 * The pixels are staged in RAM; call sh1106_flush() to push them to the display.
 *
 * @param x  Pixel x of the leftmost column of the first glyph.
 * @param y  Pixel y of the topmost row of the glyphs.
 * @param s  Null-terminated string. Must not be NULL.
 */
void sh1106_draw_string(int x, int y, const char *s);

/**
 * @brief Push the framebuffer to the display.
 *
 * Sends 8 pages of 128 bytes each via polling SPI transfers. Blocks until the
 * full transfer completes (typically a few milliseconds at 8 MHz).
 */
void sh1106_flush(void);
```

- [ ] **Step 3: Build**

Run: `idf.py build`
Expected: succeeds. Doxygen blocks are comments only — no behavior change.

- [ ] **Step 4: Commit**

```bash
git add components/sh1106/include/sh1106.h components/font8x8/include/font8x8.h
git commit -m "docs: full Doxygen blocks for sh1106.h and font8x8.h"
```

---

## Task 21: Doxygen blocks for `buttons.h` and `menu.h`

**Files:**
- Modify: `components/buttons/include/buttons.h`
- Modify: `components/menu/include/menu.h`

- [ ] **Step 1: Replace `components/buttons/include/buttons.h`**

```c
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @file buttons.h
 * @brief Debounced 3-button input library with timed event types.
 *
 * Spawns one polling task that scans the 3 button GPIOs every 10 ms,
 * applies a software debounce, and emits typed events onto a FreeRTOS queue.
 */

/**
 * @brief Logical button identifiers.
 */
typedef enum {
    BTN_BACK    = 0, /**< Back / cancel button. */
    BTN_ENTER   = 1, /**< Enter / confirm button. */
    BTN_FORWARD = 2, /**< Forward / next button. */
    BTN_COUNT   = 3, /**< Number of buttons; used for array sizing. */
} button_id_t;

/**
 * @brief Event types emitted by the library.
 */
typedef enum {
    BTN_EVT_PRESSED,    /**< Debounced press edge. */
    BTN_EVT_RELEASED,   /**< Debounced release edge. */
    BTN_EVT_LONG_PRESS, /**< Held for >= long_press_ms; fires once. */
    BTN_EVT_REPEAT,     /**< Held past long-press; fires every repeat_interval_ms. */
} button_event_type_t;

/**
 * @brief One event on the queue.
 */
typedef struct {
    button_id_t         button; /**< Which button generated the event. */
    button_event_type_t event;  /**< Event type. */
} button_event_t;

/**
 * @brief Configuration for buttons_init().
 *
 * Pin assignments and timing thresholds. All three buttons share the same timings.
 */
typedef struct {
    int pins[BTN_COUNT];     /**< GPIO numbers for BACK, ENTER, FORWARD respectively. */
    int debounce_ms;         /**< Debounce window, typically 10-50 ms. */
    int long_press_ms;       /**< Hold time before LONG_PRESS fires. */
    int repeat_interval_ms;  /**< Interval between REPEAT events after LONG_PRESS. */
} buttons_config_t;

/**
 * @brief Initialize the library: configure GPIOs, create the queue, spawn the polling task.
 *
 * The polling task runs at priority `tskIDLE_PRIORITY + 1` with a 3 KB stack and
 * a 10 ms period. The returned queue accepts up to 16 events before dropping new ones.
 *
 * @param cfg Configuration. Must not be NULL. Contents are copied internally.
 * @return The event queue handle on success, NULL on failure (with an ESP_LOGE explaining why).
 */
QueueHandle_t buttons_init(const buttons_config_t *cfg);
```

- [ ] **Step 2: Replace `components/menu/include/menu.h`**

```c
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "buttons.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/**
 * @file menu.h
 * @brief Recursive data-driven menu engine for the SH1106 OLED.
 *
 * Renders static menu trees on the display, dispatches button events for navigation,
 * and invokes user-provided action callbacks. Two layouts (text list, icon row) and
 * three selection styles (invert, arrow, border) are supported, all configurable per-menu.
 */

/**
 * @brief Discriminator for menu_item_t::u.
 *
 * MENU_ITEM_END must be 0 so zero-initialized arrays end in a sentinel.
 */
typedef enum {
    MENU_ITEM_END = 0,    /**< Array sentinel. */
    MENU_ITEM_ACTION,     /**< Leaf item; invokes a callback on ENTER. */
    MENU_ITEM_SUBMENU,    /**< Branch item; ENTER descends into another menu. */
} menu_item_kind_t;

typedef struct menu_s menu_t;

/**
 * @brief One menu item (label, optional icon, and either an action or a submenu).
 */
typedef struct {
    menu_item_kind_t kind;   /**< Discriminator for u. */
    const char      *label;  /**< Display text. Required. */
    const uint8_t   *icon;   /**< Optional 32x32 page-major bitmap; NULL for text-only items. */
    union {
        void (*action)(void *user_ctx);  /**< Called when kind == MENU_ITEM_ACTION. */
        const menu_t *submenu;           /**< Used when kind == MENU_ITEM_SUBMENU. */
    } u;
} menu_item_t;

/**
 * @brief Layout selector for a menu screen.
 */
typedef enum {
    MENU_LAYOUT_LIST,    /**< Vertical text list with optional title bar. */
    MENU_LAYOUT_ICONS,   /**< Horizontal icon row with the selected item's label below. */
} menu_layout_t;

/**
 * @brief How the selected item is visually distinguished.
 */
typedef enum {
    MENU_SEL_INVERT,   /**< Fill the item's row with white, draw text in inverse. */
    MENU_SEL_ARROW,    /**< Draw a '>' cursor to the left of the item. */
    MENU_SEL_BORDER,   /**< 1-pixel rectangle around the item's bounding box. */
} menu_selection_t;

/**
 * @brief Per-menu visual style overrides.
 *
 * If a menu's `style` is NULL, sensible defaults are used.
 */
typedef struct {
    int              icon_w;        /**< Icon width in pixels. Default 32. */
    int              icon_h;        /**< Icon height in pixels. Default 32. */
    int              row_height;    /**< LIST row pitch in pixels. Default 10. */
    int              title_height;  /**< Title bar height in pixels (0 disables). Default 10. */
    menu_selection_t selection;     /**< Selection rendering style. Default MENU_SEL_INVERT. */
} menu_style_t;

/**
 * @brief One menu screen: optional title + layout + items + optional style.
 */
struct menu_s {
    const char         *title;   /**< Optional title shown at top. NULL = no title bar. */
    menu_layout_t       layout;  /**< Layout selector. */
    const menu_item_t  *items;   /**< Array of items terminated by MENU_END. */
    const menu_style_t *style;   /**< Optional style override; NULL uses defaults. */
};

/** Sentinel value used to terminate menu_item_t arrays. */
#define MENU_END { .kind = MENU_ITEM_END }

/**
 * @brief Set the root menu and the user context passed to action callbacks.
 *
 * Renders the root immediately. Subsequent calls reset the navigation state.
 *
 * @param root     Pointer to the root menu. Must not be NULL.
 * @param user_ctx Opaque pointer forwarded to every action callback. May be NULL.
 */
void menu_init(const menu_t *root, void *user_ctx);

/**
 * @brief Drive the engine with one button event.
 *
 * - FORWARD (press or repeat): advance selection within the current menu (wraps).
 * - BACK    (press or repeat): pop one nav frame; no-op at the root.
 * - ENTER   (press only):      invoke the item's action OR descend into its submenu.
 *
 * Re-renders only if state changed. Not thread-safe — call from a single consumer task.
 *
 * @param evt The button event to process.
 */
void menu_handle_event(button_event_t evt);

/**
 * @brief Convenience: spawn a task that drains @p q and calls menu_handle_event.
 *
 * @param q        Button event queue (typically from buttons_init).
 * @param priority FreeRTOS task priority.
 */
void menu_run_task(QueueHandle_t q, UBaseType_t priority);

/**
 * @brief Force an immediate redraw of the current menu.
 *
 * Useful from action callbacks that mutate external state which should be reflected.
 */
void menu_redraw(void);

/**
 * @brief Show a brief inverted-bar message at the bottom of the screen, then restore.
 *
 * Blocks the calling (menu) task for @p duration_ms. Button events arriving during the
 * toast remain queued and are processed afterward.
 *
 * @param msg         Null-terminated string to display.
 * @param duration_ms How long to show the toast, in milliseconds.
 */
void menu_toast(const char *msg, int duration_ms);
```

- [ ] **Step 3: Build, flash, final integration check**

Run: `idf.py build flash monitor`
Expected: full demo works exactly as before. Navigation, scrolling, toasts, all three selection styles, three icons, three nesting levels.

- [ ] **Step 4: Commit**

```bash
git add components/buttons/include/buttons.h components/menu/include/menu.h
git commit -m "docs: full Doxygen blocks for buttons.h and menu.h"
```

- [ ] **Step 5: Milestone reached**

Quality & reusability pass complete. The project now ships:

- Four self-contained ESP-IDF components under `components/`.
- A clean demo under `main/`.
- `.clang-format` + `.clang-tidy` + `.editorconfig` driving consistent style and advisory static analysis.
- Code-quality fixes: error returns from `sh1106_init`, NULL-input assertions, no const-casts, static-assert invariants.
- A root README and one per component, with full Doxygen API documentation in every public header.

---

## Self-review notes

- **Spec coverage:** every section of the spec maps to one or more tasks.
  - File layout → Tasks 9–12 (componentize) + 13 (clangd).
  - Component dependencies → Tasks 9–12 (CMakeLists per component).
  - Code style → Tasks 1–3 (editorconfig + clang-format + format pass).
  - Static analysis → Task 4 (config) + Task 8 (fixes inspired by the spec's findings).
  - editorconfig → Task 1.
  - Code quality fixes (all 9 items) → Tasks 5, 6, 7, 8.
  - Doxygen in headers → Tasks 20, 21.
  - Root README → Task 14.
  - Per-component READMEs → Tasks 15, 16, 17, 18.
  - main/README → Task 19.
- **Placeholders:** none. Every step has concrete code, a concrete command, or an exact path.
- **Type consistency:** `esp_err_t sh1106_init(void)` introduced in Task 6 and used in main.c through Task 6's Step 3. `frame_t *f` (non-const) introduced in Task 8 across `render_list`, `render_icons`, and `render`. `MENU_END`, `menu_item_kind_t`, `menu_layout_t`, `menu_selection_t`, `menu_style_t`, `menu_t`, `menu_item_t` consistent throughout. CMakeLists `REQUIRES` / `PRIV_REQUIRES` split matches the spec dependency table.
- **Order:** structure-touching tasks (componentize) come after style + quality fixes, so the moved code is already clean. Docs land last, against the final structure.
- **Build verification cadence:** every task that touches code ends in `idf.py build`. Tasks that touch runtime behavior (6, 10, 11, 12, 21) also end in `flash monitor` + visual demo check.
