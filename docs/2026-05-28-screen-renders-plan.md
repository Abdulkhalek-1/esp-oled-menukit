# Host-side menu render harness — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `tools/host_render/` — a host-side build that compiles the real
`menu.c` + `font8x8.c` against ESP-IDF stubs and a SPI-less sh1106 framebuffer,
producing five PNG screenshots checked into `docs/img/` and embedded in the
root `README.md`.

**Architecture:** New host-only CMake project under `tools/host_render/`. Stub
headers in `stubs/` satisfy `esp_err.h`, `esp_log.h`, and the three FreeRTOS
headers referenced by `menu.h` / `buttons.h`. A new `sh1106_host.c` replaces
the SPI half of the real driver while keeping the framebuffer + drawing
functions byte-identical. `scenes.c` mirrors the menu graph from `main/main.c`
with no-op action callbacks. `render.c` drives each scene, dumps a PGM via a
captured "displayed" snapshot, and a tiny Python step scales the PGMs 4× and
emits PNGs.

**Tech Stack:** C11, CMake ≥ 3.16, plain `gcc`/`clang` (no ESP-IDF), Python 3
(stdlib only — Pillow optional).

**Spec:** [docs/2026-05-28-screen-renders-design.md](2026-05-28-screen-renders-design.md)

**Prerequisites on the host:** `cmake`, `gcc` (or clang), GNU `make`, `python3`.
No devcontainer or ESP-IDF toolchain needed for this harness — it builds on
the bare host shell.

---

## Task 1: Scaffold directory + stubs + first build (sh1106_host alone)

**Files:**

- Create: `tools/host_render/CMakeLists.txt`
- Create: `tools/host_render/stubs/esp_err.h`
- Create: `tools/host_render/stubs/esp_log.h`
- Create: `tools/host_render/stubs/freertos/FreeRTOS.h`
- Create: `tools/host_render/stubs/freertos/queue.h`
- Create: `tools/host_render/stubs/freertos/task.h`
- Create: `tools/host_render/sh1106_host.h`
- Create: `tools/host_render/sh1106_host.c`

- [ ] **Step 1: Create the directory tree**

```bash
mkdir -p tools/host_render/stubs/freertos
```

- [ ] **Step 2: Write `tools/host_render/stubs/esp_err.h`**

```c
#pragma once

typedef int esp_err_t;

#define ESP_OK 0
```

- [ ] **Step 3: Write `tools/host_render/stubs/esp_log.h`**

```c
#pragma once

#define ESP_LOGI(tag, ...) ((void)0)
#define ESP_LOGW(tag, ...) ((void)0)
#define ESP_LOGE(tag, ...) ((void)0)
#define ESP_LOGD(tag, ...) ((void)0)
#define ESP_LOGV(tag, ...) ((void)0)
```

- [ ] **Step 4: Write `tools/host_render/stubs/freertos/FreeRTOS.h`**

```c
#pragma once

#include <stdint.h>

typedef int          BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t     TickType_t;

#define pdTRUE          1
#define pdFALSE         0
#define portMAX_DELAY   0xFFFFFFFFu
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
```

- [ ] **Step 5: Write `tools/host_render/stubs/freertos/queue.h`**

```c
#pragma once

#include "FreeRTOS.h"

typedef void *QueueHandle_t;

static inline BaseType_t xQueueReceive(QueueHandle_t q, void *buf, TickType_t wait)
{
    (void)q;
    (void)buf;
    (void)wait;
    return pdFALSE;
}
```

- [ ] **Step 6: Write `tools/host_render/stubs/freertos/task.h`**

```c
#pragma once

#include "FreeRTOS.h"

static inline void vTaskDelay(TickType_t ticks)
{
    (void)ticks;
}

static inline BaseType_t xTaskCreate(void (*fn)(void *), const char *name, uint32_t stack,
                                     void *arg, UBaseType_t prio, void *out)
{
    (void)fn;
    (void)name;
    (void)stack;
    (void)arg;
    (void)prio;
    (void)out;
    return pdTRUE;
}
```

- [ ] **Step 7: Write `tools/host_render/sh1106_host.h`**

```c
#pragma once

#include <stdint.h>

#include "sh1106.h"

/**
 * @file sh1106_host.h
 * @brief Host-side extensions to the sh1106 driver used by the render harness.
 *
 * The host build replaces sh1106's SPI half with no-ops and captures every
 * sh1106_flush() into a separate `snapshot` buffer. These accessors let the
 * render driver dump the snapshot and freeze it for the toast scene.
 */

/** Return the last "displayed" frame (the last unlocked sh1106_flush). */
const uint8_t *sh1106_host_snapshot(void);

/** Arm a one-shot capture lock: the next sh1106_flush snapshots, then locks. */
void sh1106_host_capture_next_flush(void);

/** Clear any pending capture lock so subsequent flushes update the snapshot. */
void sh1106_host_release(void);
```

- [ ] **Step 8: Write `tools/host_render/sh1106_host.c`**

The framebuffer ops (`sh1106_clear`, `sh1106_set_pixel`, `sh1106_draw_string`)
are copied verbatim from [`components/sh1106/src/sh1106.c`](../components/sh1106/src/sh1106.c)
lines 119-170. The SPI-touching `sh1106_init` / `sh1106_flush` are replaced
with no-op / snapshot-only versions.

```c
#include "sh1106_host.h"

#include "font8x8.h"

#include <stdint.h>
#include <string.h>

#define SH1106_PAGES   (SH1106_HEIGHT / 8)
#define SH1106_FB_SIZE (SH1106_WIDTH * SH1106_PAGES)

static uint8_t framebuffer[SH1106_FB_SIZE];
static uint8_t snapshot[SH1106_FB_SIZE];
static int     capture_pending;
static int     locked;

esp_err_t      sh1106_init(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
    memset(snapshot, 0, sizeof(snapshot));
    capture_pending = 0;
    locked          = 0;
    return ESP_OK;
}

void sh1106_clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
}

void sh1106_set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= SH1106_WIDTH) return;
    if (y < 0 || y >= SH1106_HEIGHT) return;

    int page = y / 8;
    int bit  = y % 8;
    int idx  = page * SH1106_WIDTH + x;

    if (on)
        framebuffer[idx] |= (1 << bit);
    else
        framebuffer[idx] &= ~(1 << bit);
}

static void draw_char(int x, int y, char c)
{
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc > 127) uc = '?';
    const uint8_t *glyph = font8x8[uc - 32];

    for (int col = 0; col < 8; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 8; row++) {
            if (bits & (1 << row)) {
                sh1106_set_pixel(x + col, y + row, true);
            }
        }
    }
}

void sh1106_draw_string(int x, int y, const char *s)
{
    while (*s) {
        draw_char(x, y, *s++);
        x += 8;
    }
}

void sh1106_flush(void)
{
    if (locked) return;
    memcpy(snapshot, framebuffer, sizeof(snapshot));
    if (capture_pending) {
        locked          = 1;
        capture_pending = 0;
    }
}

const uint8_t *sh1106_host_snapshot(void)
{
    return snapshot;
}

void sh1106_host_capture_next_flush(void)
{
    capture_pending = 1;
}

void sh1106_host_release(void)
{
    capture_pending = 0;
    locked          = 0;
}
```

- [ ] **Step 9: Write `tools/host_render/CMakeLists.txt` (sh1106_host-only build)**

```cmake
cmake_minimum_required(VERSION 3.16)
project(host_render C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# Step 1: build sh1106_host + font8x8 into a static lib (no executable yet).
add_library(render_core STATIC
    sh1106_host.c
    ${REPO_ROOT}/components/font8x8/src/font8x8.c
)

target_include_directories(render_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${REPO_ROOT}/components/sh1106/include
    ${REPO_ROOT}/components/font8x8/include
)
```

- [ ] **Step 10: Configure and build**

```bash
cd tools/host_render
cmake -B build -S .
cmake --build build
```

Expected: configures with no warnings; builds `build/librender_core.a`. No
errors from the stubs being incomplete (sh1106_host.c only references types
the stubs cover).

- [ ] **Step 11: Commit**

```bash
cd ../..
git add tools/host_render/CMakeLists.txt \
        tools/host_render/stubs \
        tools/host_render/sh1106_host.h \
        tools/host_render/sh1106_host.c
git commit -m "host_render: scaffold stubs + SPI-less sh1106 framebuffer"
```

---

## Task 2: Add menu.c + buttons header to the host build

**Files:**

- Modify: `tools/host_render/CMakeLists.txt`

- [ ] **Step 1: Update `tools/host_render/CMakeLists.txt`**

Add `menu.c` as a source and extend include directories so it can find
`menu.h`, `buttons.h`, and the FreeRTOS stubs.

```cmake
cmake_minimum_required(VERSION 3.16)
project(host_render C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_library(render_core STATIC
    sh1106_host.c
    ${REPO_ROOT}/components/font8x8/src/font8x8.c
    ${REPO_ROOT}/components/menu/src/menu.c
)

target_include_directories(render_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${REPO_ROOT}/components/sh1106/include
    ${REPO_ROOT}/components/menu/include
    ${REPO_ROOT}/components/buttons/include
    ${REPO_ROOT}/components/font8x8/include
)
```

- [ ] **Step 2: Build**

```bash
cd tools/host_render
cmake --build build
```

Expected: `menu.c` compiles cleanly. Any unresolved reference here means a
stub is missing — fix the stub, don't modify `menu.c`. (Likely missing
symbols, in order of probability: `tskIDLE_PRIORITY` — not used by menu.c,
so should not trigger; `QueueHandle_t` — already in queue.h stub.) If you
hit a missing symbol, add it as a no-op to the appropriate stub file and
rebuild.

- [ ] **Step 3: Commit**

```bash
cd ../..
git add tools/host_render/CMakeLists.txt
git commit -m "host_render: compile menu.c against host stubs"
```

---

## Task 3: Scene declarations + setup functions

**Files:**

- Create: `tools/host_render/scenes.h`
- Create: `tools/host_render/scenes.c`
- Modify: `tools/host_render/CMakeLists.txt`

- [ ] **Step 1: Write `tools/host_render/scenes.h`**

```c
#pragma once

/**
 * @file scenes.h
 * @brief Canonical menu screenshots driven by the render harness.
 *
 * Each scene's `setup` function navigates the menu (via the public
 * menu_init / menu_handle_event API) to the desired state and leaves
 * the displayed image in sh1106_host's snapshot buffer.
 */

typedef struct {
    const char *name;     /**< Output filename stem (no extension). */
    void (*setup)(void);  /**< Navigates the menu to the desired state. */
} scene_t;

extern const scene_t scenes[];
extern const int     scenes_count;
```

- [ ] **Step 2: Write `tools/host_render/scenes.c`**

Mirrors the menu graph from `main/main.c`. All action callbacks are NULL —
the canonical scenes never invoke leaf actions (the toast scene calls
`menu_toast` directly from its setup, bypassing the ENTER action).

```c
#include "scenes.h"

#include "buttons.h"
#include "icons.h"
#include "menu.h"
#include "sh1106_host.h"

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
```

- [ ] **Step 3: Update `tools/host_render/CMakeLists.txt`**

Append `scenes.c` to the `render_core` source list and add `main/` to the
include path (for `icons.h`). Replace the file with:

```cmake
cmake_minimum_required(VERSION 3.16)
project(host_render C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_library(render_core STATIC
    sh1106_host.c
    scenes.c
    ${REPO_ROOT}/components/font8x8/src/font8x8.c
    ${REPO_ROOT}/components/menu/src/menu.c
    ${REPO_ROOT}/main/icons.c
)

target_include_directories(render_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${REPO_ROOT}/components/sh1106/include
    ${REPO_ROOT}/components/menu/include
    ${REPO_ROOT}/components/buttons/include
    ${REPO_ROOT}/components/font8x8/include
    ${REPO_ROOT}/main
)
```

- [ ] **Step 4: Build**

```bash
cd tools/host_render
cmake --build build
```

Expected: scenes.c and icons.c compile, librender_core.a rebuilt with both
included.

- [ ] **Step 5: Commit**

```bash
cd ../..
git add tools/host_render/scenes.h \
        tools/host_render/scenes.c \
        tools/host_render/CMakeLists.txt
git commit -m "host_render: scenes mirroring the demo menu graph"
```

---

## Task 4: render.c executable + first PGM output

**Files:**

- Create: `tools/host_render/render.c`
- Modify: `tools/host_render/CMakeLists.txt`

- [ ] **Step 1: Write `tools/host_render/render.c`**

```c
#include "scenes.h"
#include "sh1106.h"
#include "sh1106_host.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SH1106_PAGES (SH1106_HEIGHT / 8)

static void write_pgm(const char *path, const uint8_t *snap)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fprintf(f, "P5\n%d %d\n255\n", SH1106_WIDTH, SH1106_HEIGHT);
    for (int y = 0; y < SH1106_HEIGHT; y++) {
        for (int x = 0; x < SH1106_WIDTH; x++) {
            int     page = y / 8;
            int     bit  = y % 8;
            int     lit  = (snap[page * SH1106_WIDTH + x] >> bit) & 1;
            uint8_t v    = lit ? 0xFF : 0x00;
            fwrite(&v, 1, 1, f);
        }
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *outdir = (argc > 1) ? argv[1] : "docs/img";

    sh1106_init();

    for (int i = 0; i < scenes_count; i++) {
        sh1106_host_release(); // clear any leftover capture lock
        scenes[i].setup();

        char path[512];
        snprintf(path, sizeof(path), "%s/%s.pgm", outdir, scenes[i].name);
        write_pgm(path, sh1106_host_snapshot());
        printf("wrote %s\n", path);
    }
    return 0;
}
```

- [ ] **Step 2: Update `tools/host_render/CMakeLists.txt` to add the executable**

Append at the end of the existing file:

```cmake
add_executable(render render.c)
target_link_libraries(render PRIVATE render_core)
```

- [ ] **Step 3: Build**

```bash
cd tools/host_render
cmake --build build
```

Expected: `build/render` executable produced.

- [ ] **Step 4: Run the binary and verify five PGMs land in docs/img/**

```bash
mkdir -p ../../docs/img
./build/render ../../docs/img
ls ../../docs/img/
```

Expected output:

```text
wrote ../../docs/img/home-icons.pgm
wrote ../../docs/img/settings-list.pgm
wrote ../../docs/img/wifi-invert.pgm
wrote ../../docs/img/about-arrow.pgm
wrote ../../docs/img/toast.pgm
```

And the `ls` should show all five `.pgm` files.

- [ ] **Step 5: Visually sanity-check one PGM**

If you have ImageMagick installed:

```bash
identify ../../docs/img/home-icons.pgm
```

Expected: `… PGM 128x64 128x64+0+0 8-bit Gray …`. If you also have `display`
or `feh`, open it — you should see a tiny image: title bar "Home", three
icons (gear, wifi, bluetooth), a border rectangle around the gear (selected).
If the image is blank or garbled, debug `sh1106_host.c` before proceeding.

If you have no image viewer, skip the visual check — Task 5 will produce PNGs.

- [ ] **Step 6: Commit**

```bash
cd ../..
git add tools/host_render/render.c tools/host_render/CMakeLists.txt
git commit -m "host_render: scene driver writes 128x64 PGMs"
```

---

## Task 5: PGM → PNG converter (4× scale)

**Files:**

- Create: `tools/host_render/pgm_to_png.py`

- [ ] **Step 1: Write `tools/host_render/pgm_to_png.py`**

```python
#!/usr/bin/env python3
"""Convert docs/img/*.pgm produced by host_render to scaled PNGs.

Reads each P5 PGM (128×64, 8-bit grayscale), upscales 4× nearest-neighbor
(→ 512×256 so the OLED pixel grid is visible in the README), and writes
docs/img/<name>.png. The PGM files are deleted after conversion.

Uses Pillow when available; otherwise falls back to a stdlib-only PNG
writer using zlib + struct + binascii.crc32.
"""

import binascii
import glob
import os
import struct
import sys
import zlib

SCALE = 4


def read_pgm(path):
    with open(path, "rb") as fh:
        data = fh.read()
    if not data.startswith(b"P5"):
        raise ValueError(f"{path}: not a P5 PGM")
    i = 2
    tokens = []
    while len(tokens) < 3:
        while i < len(data) and chr(data[i]).isspace():
            i += 1
        if i < len(data) and data[i:i + 1] == b"#":
            while i < len(data) and data[i:i + 1] != b"\n":
                i += 1
            continue
        j = i
        while j < len(data) and not chr(data[j]).isspace():
            j += 1
        tokens.append(data[i:j])
        i = j
    width = int(tokens[0])
    height = int(tokens[1])
    _maxval = int(tokens[2])
    i += 1  # single whitespace after maxval, then raw bytes
    pixels = data[i:i + width * height]
    if len(pixels) != width * height:
        raise ValueError(f"{path}: expected {width * height} pixel bytes, got {len(pixels)}")
    return width, height, pixels


def scale_nn(pixels, w, h, factor):
    sw, sh = w * factor, h * factor
    out = bytearray(sw * sh)
    for y in range(sh):
        src_y = y // factor
        row_src = pixels[src_y * w:(src_y + 1) * w]
        for x in range(sw):
            out[y * sw + x] = row_src[x // factor]
    return bytes(out), sw, sh


def write_png_stdlib(path, w, h, pixels):
    sig = b"\x89PNG\r\n\x1a\n"

    def chunk(tag, body):
        return (struct.pack(">I", len(body)) + tag + body
                + struct.pack(">I", binascii.crc32(tag + body) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0)  # 8-bit grayscale
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter byte: None
        raw += pixels[y * w:(y + 1) * w]
    idat = zlib.compress(bytes(raw), 9)
    with open(path, "wb") as fh:
        fh.write(sig)
        fh.write(chunk(b"IHDR", ihdr))
        fh.write(chunk(b"IDAT", idat))
        fh.write(chunk(b"IEND", b""))


def write_png_pil(path, w, h, pixels):
    from PIL import Image
    Image.frombytes("L", (w, h), pixels).save(path, "PNG")


try:
    from PIL import Image  # noqa: F401
    write_png = write_png_pil
    backend = "Pillow"
except ImportError:
    write_png = write_png_stdlib
    backend = "stdlib"


def main():
    img_dir = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "..", "..", "docs", "img"))
    pgms = sorted(glob.glob(os.path.join(img_dir, "*.pgm")))
    if not pgms:
        print(f"No PGMs found in {img_dir}", file=sys.stderr)
        sys.exit(1)
    print(f"PNG backend: {backend}")
    for pgm in pgms:
        w, h, px = read_pgm(pgm)
        scaled, sw, sh = scale_nn(px, w, h, SCALE)
        png = pgm[:-4] + ".png"
        write_png(png, sw, sh, scaled)
        os.remove(pgm)
        print(f"{os.path.basename(pgm)} -> {os.path.basename(png)} ({sw}x{sh})")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Make it executable**

```bash
chmod +x tools/host_render/pgm_to_png.py
```

- [ ] **Step 3: Regenerate PGMs (Task 4's run deleted them if you cleaned up) and convert**

```bash
cd tools/host_render
./build/render ../../docs/img
python3 pgm_to_png.py
ls -la ../../docs/img/
```

Expected: each `*.pgm` is replaced by a `*.png`. The `ls` should show only
`.png` files, no `.pgm`. Each PNG should be ~1-3 KB (compressed monochrome
content).

- [ ] **Step 4: Verify PNG dimensions**

```bash
file ../../docs/img/home-icons.png
```

Expected: something like `home-icons.png: PNG image data, 512 x 256, 8-bit grayscale, non-interlaced`.

If you have an image viewer, open one of the PNGs and confirm it looks like
a chunky 4×-scaled OLED screenshot.

- [ ] **Step 5: Commit (script only — PNGs come later in Task 7)**

```bash
cd ../..
git add tools/host_render/pgm_to_png.py
git commit -m "host_render: pgm→png converter with 4x scale"
```

---

## Task 6: Makefile + tools/host_render/README.md

**Files:**

- Create: `tools/host_render/Makefile`
- Create: `tools/host_render/README.md`

- [ ] **Step 1: Write `tools/host_render/Makefile`**

The recipe lines below begin with a real tab character (Make requires it).

```make
BUILD_DIR := build
OUT_DIR := ../../docs/img

.PHONY: all configure build render convert clean

all: convert

configure:
	cmake -B $(BUILD_DIR) -S .

build: configure
	cmake --build $(BUILD_DIR)

render: build
	mkdir -p $(OUT_DIR)
	./$(BUILD_DIR)/render $(OUT_DIR)

convert: render
	python3 pgm_to_png.py

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(OUT_DIR)/*.pgm
```

- [ ] **Step 2: Write `tools/host_render/README.md`**

```markdown
# host_render — OLED screenshot harness

Compiles the firmware's real `menu.c` + `font8x8.c` against a SPI-less
sh1106 framebuffer to produce pixel-accurate PNG screenshots of canonical
menu states, used in the project root `README.md`.

## Regenerate the screenshots

From this directory:

```bash
make
```

That builds the harness, runs it, writes five PGMs to `../../docs/img/`,
then upscales them 4× into PNGs (and deletes the intermediate PGMs). The
PNGs are checked into git so GitHub renders them in the root README.

Re-run any time menu rendering, fonts, or icons change — the output will
update automatically because the harness uses the real component sources.

## Requirements

`cmake` ≥ 3.16, a C11 compiler (`gcc`/`clang`), GNU make, `python3`. The
Python step uses Pillow if it's installed and falls back to a stdlib-only
PNG encoder otherwise.

## How it works

See [docs/2026-05-28-screen-renders-design.md](../../docs/2026-05-28-screen-renders-design.md)
for the architecture: ESP-IDF header stubs, a host replacement for the SPI
half of `sh1106`, and a snapshot lock that captures the toast frame before
the menu repaints over it.
```

- [ ] **Step 3: Verify the Makefile end-to-end**

```bash
cd tools/host_render
make clean
make
ls ../../docs/img/
```

Expected: `make clean` removes `build/` and any stray PGMs; `make` reconfigures,
rebuilds, runs the harness, and produces five PNGs. The `ls` should show only
`*.png` files.

- [ ] **Step 4: Commit**

```bash
cd ../..
git add tools/host_render/Makefile tools/host_render/README.md
git commit -m "host_render: one-command Makefile + README"
```

---

## Task 7: .gitignore, root README, and commit the PNGs

**Files:**

- Modify: `.gitignore`
- Modify: `README.md`
- Create: `docs/img/home-icons.png`, `settings-list.png`, `wifi-invert.png`,
  `about-arrow.png`, `toast.png` (generated)

- [ ] **Step 1: Update `.gitignore`**

Append these lines to the existing `.gitignore` (after the "User-specific
configuration files" section):

```text
# Host-side render harness build artifacts
tools/host_render/build/
docs/img/*.pgm
```

- [ ] **Step 2: Update root `README.md`**

Insert a new `## Screens` section immediately after the `## Components`
table and before the `## Hardware` section. The exact block to insert:

```markdown
## Screens

Rendered by [`tools/host_render/`](tools/host_render/) — the actual firmware
menu code drawing into a host-side framebuffer, not a photo or mockup.

![Home — icon row, BORDER selection](docs/img/home-icons.png)
![Settings — text list, BORDER selection](docs/img/settings-list.png)
![WiFi — text list, INVERT selection (default)](docs/img/wifi-invert.png)
![About — text list, ARROW selection](docs/img/about-arrow.png)
![Toast notification on Settings](docs/img/toast.png)

Regenerate any time the menu code changes: `cd tools/host_render && make`.
```

For reference, the existing `## Components` section ends with the table that
includes the `menu` row, and the next existing heading is `## Hardware`. The
new section goes between them.

- [ ] **Step 3: Regenerate the PNGs (paranoia check) and confirm they exist**

```bash
cd tools/host_render
make clean
make
cd ../..
ls -la docs/img/
```

Expected: exactly five `.png` files, no `.pgm` files. Each PNG ~1-3 KB.

- [ ] **Step 4: Verify git sees what we expect**

```bash
git status
```

Expected: `.gitignore` and `README.md` modified; five new PNGs untracked under
`docs/img/`. No `.pgm` files or `tools/host_render/build/` listed (gitignore
filtering them).

- [ ] **Step 5: Commit everything**

```bash
git add .gitignore README.md docs/img/*.png
git commit -m "docs: embed OLED screenshots in README via host_render"
```

- [ ] **Step 6: Visually verify the README in a markdown previewer**

Open `README.md` in your editor's markdown preview (VSCode: Ctrl+Shift+V).
Confirm all five images render and the section sits between `## Components`
and `## Hardware`. If an image is broken, check that the path
`docs/img/<name>.png` matches the file on disk exactly.

- [ ] **Step 7 (optional): Push**

Only after you've eyeballed the README and PNGs:

```bash
git push
```

---

## Self-review

Checked against [docs/2026-05-28-screen-renders-design.md](2026-05-28-screen-renders-design.md):

- **Goal — five canonical screenshots embedded in README:** Tasks 4 (PGMs)
  + 5 (PNGs) + 7 (embed). ✓
- **Renders use real firmware code:** Task 2 adds `menu.c` to the build;
  Task 3 adds `font8x8.c` and `icons.c`. `menu.c` is compiled unmodified. ✓
- **`sh1106_host.c` ~60 LoC, with snapshot + capture lock:** Task 1, Step 8. ✓
- **Stubs for esp_err, esp_log, FreeRTOS/{FreeRTOS,task,queue}:** Task 1,
  Steps 2-6. ✓
- **`scenes.c` mirrors main.c's menu graph with NULL actions:** Task 3,
  Step 2. ✓
- **Toast scene uses capture-next-flush before `menu_toast`:** Task 3,
  Step 2, `setup_toast`. ✓
- **`render.c` writes PGMs via `sh1106_host_snapshot()` with release between
  scenes:** Task 4, Step 1. ✓
- **`pgm_to_png.py` 4× scale with Pillow + stdlib fallback:** Task 5,
  Step 1. ✓
- **Makefile chains the pipeline:** Task 6, Step 1. ✓
- **`.gitignore` adds build/ and docs/img/*.pgm:** Task 7, Step 1. ✓
- **README has `## Screens` between Components and Hardware:** Task 7,
  Step 2. ✓
- **All five scenes covered (home-icons, settings-list, wifi-invert,
  about-arrow, toast):** Task 3 setup functions, listed in the `scenes[]`
  table. ✓

No spec items left uncovered. No placeholders in the plan. Type/symbol
consistency between tasks verified: `sh1106_host_snapshot`,
`sh1106_host_capture_next_flush`, `sh1106_host_release`, `scene_t`,
`scenes`, `scenes_count` used identically across header and consumers.
