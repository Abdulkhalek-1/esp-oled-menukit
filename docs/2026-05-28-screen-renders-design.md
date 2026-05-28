# Host-side menu render harness — design

Date: 2026-05-28
Topic: generate canonical OLED screenshots for the project README without
hardware, without photos, and without AI.

## Goal

The README has no visuals today. Add five pixel-accurate PNG screenshots of
representative menu states, embedded inline in `README.md`, and a reproducible
pipeline so they can be regenerated whenever menu rendering changes.

The renders must be produced by the **actual firmware rendering code**, not a
mockup — so any change to `menu.c` layout math or `sh1106` text drawing
automatically updates the screenshots on rerun.

## Non-goals

- Animating menu transitions. Each screenshot is a single static frame.
- Rendering on real hardware and pulling the framebuffer over UART. The point
  is host-side, no devkit required.
- A test framework. The harness produces images; it does not assert anything
  about them. (A future visual-regression pass could compare PNGs, but is out
  of scope here.)
- Touching the firmware code. Component sources must compile unchanged.

## Architecture

New directory `tools/host_render/` containing a small host-side CMake project
that builds **the real `menu.c` and `font8x8.c`** against tiny no-op stubs for
the ESP-IDF APIs they touch, plus a replacement SPI-free implementation of the
`sh1106` framebuffer ops.

```text
tools/host_render/
├── CMakeLists.txt          # host CMake (gcc/clang, no ESP-IDF)
├── Makefile                # convenience: build + render + convert PNGs
├── README.md               # how to regenerate the screenshots
├── stubs/                  # no-op replacements for ESP-IDF headers
│   ├── esp_err.h
│   ├── esp_log.h
│   └── freertos/
│       ├── FreeRTOS.h
│       ├── task.h
│       └── queue.h
├── sh1106_host.c           # framebuffer + clear/set_pixel/draw_string
│                           # (init = no-op; flush snapshots framebuffer)
├── sh1106_host.h           # extra host-only accessors
├── scenes.c                # canonical menu graph + per-scene state setup
├── render.c                # main(): drives each scene, dumps PGM per scene
└── pgm_to_png.py           # scales each PGM 4x and writes PNG to docs/img/
```

### What gets compiled and from where

| Source                             | Origin                                          |
|------------------------------------|-------------------------------------------------|
| `components/menu/src/menu.c`       | **unchanged** — uses stubs for FreeRTOS/log     |
| `components/font8x8/src/font8x8.c` | **unchanged** — pure data                       |
| `tools/host_render/sh1106_host.c`  | new — replaces `components/sh1106/src/sh1106.c` |
| `tools/host_render/scenes.c`       | new — mirrors `main/main.c`'s menu graph        |
| `tools/host_render/render.c`       | new — scene driver + PGM writer                 |
| `main/icons.c`                     | reused via include path                         |

`buttons.h` is included by `menu.h` but no functions from it are called by the
render paths — only its types are referenced. The stubs cover its FreeRTOS
includes, so no `buttons.c` is linked.

### Stub contents (size budget: ~30 LoC total)

- `esp_err.h` — `typedef int esp_err_t; #define ESP_OK 0`
- `esp_log.h` — `#define ESP_LOGI(...) ((void)0)` for I/W/E/D
- `freertos/FreeRTOS.h` — `pdTRUE`, `portMAX_DELAY`, `pdMS_TO_TICKS(ms)` macros,
  plus the integer typedefs (`BaseType_t`, `UBaseType_t`, `TickType_t`)
- `freertos/queue.h` — `typedef void* QueueHandle_t;` + `xQueueReceive` no-op
- `freertos/task.h` — `vTaskDelay` and `xTaskCreate` no-ops

### `sh1106_host.c` (~60 LoC)

Copy of the framebuffer-touching half of `components/sh1106/src/sh1106.c`:

- `static uint8_t framebuffer[SH1106_FB_SIZE];`
- `static uint8_t snapshot[SH1106_FB_SIZE];` — last "displayed" image
- `sh1106_clear`, `sh1106_set_pixel`, `sh1106_draw_string` (verbatim)
- `sh1106_init` → return `ESP_OK`
- `sh1106_flush` → copy `framebuffer` into `snapshot`, mirroring what the
  real driver pushes to the OLED. After the first flush following a
  `sh1106_host_capture_next_flush()` call, snapshotting is locked until the
  next call to `sh1106_host_release()`.
- public helpers in `sh1106_host.h`:
  - `const uint8_t *sh1106_host_snapshot(void);` — last displayed frame
  - `void sh1106_host_capture_next_flush(void);` — arm one-shot capture lock
  - `void sh1106_host_release(void);` — clear the lock for the next scene

**Why the capture lock matters for the toast scene.** `menu_toast()` calls
`sh1106_flush()` twice: once after drawing the toast bar (the frame we want
to keep), then again at the end via `render()` to restore the menu. With our
stub `vTaskDelay()` returning instantly, both flushes happen in quick
succession. The lock-on-next-flush mechanism makes `sh1106_flush` snapshot
the toast frame, then ignore the restore flush, leaving the snapshot frozen
with the toast visible — exactly what a user sees on real hardware for the
toast's duration.

### `scenes.c`

Declares the same menu graph as `main/main.c` (Home / Settings / WiFi /
Bluetooth / About) but skips the action callbacks (they are not invoked in
render mode — for the toast scene we call `menu_toast` directly which just
draws the bar and returns since `vTaskDelay` is a stub).

Exposes:

```c
typedef struct {
    const char *name;     // becomes the output filename stem
    void (*setup)(void);  // navigates the menu to the right state,
                          // optionally calls menu_toast
} scene_t;

extern const scene_t scenes[];
extern const int scenes_count;
```

Each `setup` function uses the public `menu_init` / `menu_handle_event` API to
position the cursor — no internals are reached into. This keeps the harness
honest: it exercises the menu the same way buttons do.

### `render.c`

```c
int main(void) {
    sh1106_init();
    for (int i = 0; i < scenes_count; i++) {
        sh1106_host_release();    // clear any leftover capture lock
        scenes[i].setup();        // navigates + flushes; toast scenes
                                  // call sh1106_host_capture_next_flush()
                                  // before menu_toast()
        char path[256];
        snprintf(path, sizeof(path), "docs/img/%s.pgm", scenes[i].name);
        write_pgm(path, sh1106_host_snapshot());
    }
}
```

`write_pgm` is ~15 LoC: P5 header (`P5 128 64 255\n`) followed by
`128 * 64 = 8192` bytes, one per pixel, value 0xFF for lit, 0x00 for dark.
(SH1106 framebuffer is page-major, so the writer iterates pages and
extracts the bit for each pixel.)

### `pgm_to_png.py`

Reads every `docs/img/*.pgm`, scales 4× nearest-neighbor (→ 512×256), writes
`docs/img/<name>.png`. Tries `from PIL import Image` first; falls back to a
40-line stdlib-only PNG writer using `zlib` + `struct` + `binascii.crc32` if
Pillow is missing. The PGM files are deleted after conversion.

### Makefile

A 4-step recipe (real tabs in the actual file, not shown here):

- `all`: `cmake -B build -S .` → `cmake --build build` → `./build/render`
  → `python3 pgm_to_png.py`
- `clean`: `rm -rf build docs/img/*.pgm`

Run from `tools/host_render/`. One command end-to-end.

## Canonical scenes

Five PNGs, chosen to cover both layouts and all three selection styles plus
the toast feature:

| File                | Menu     | Layout | Selection | Notes                            |
|---------------------|----------|--------|-----------|----------------------------------|
| `home-icons.png`    | Home     | ICONS  | BORDER    | Settings selected                |
| `settings-list.png` | Settings | LIST   | BORDER    | Contrast selected                |
| `wifi-invert.png`   | WiFi     | LIST   | INVERT    | Connect selected (default style) |
| `about-arrow.png`   | About    | LIST   | ARROW     | Version selected                 |
| `toast.png`         | Settings | LIST   | BORDER    | "Brightness +" toast at bottom   |

Native 128×64, displayed at 512×256 in the README (4× nearest-neighbor scale)
so the pixel grid is visible to the reader.

## README changes

Insert a new `## Screens` section immediately after `## Components`, before
`## Hardware`. Five images with one-line captions each:

```markdown
## Screens

Rendered by `tools/host_render/` — the actual firmware menu code drawing into
a host-side framebuffer, not a photo or mockup.

![Home — icon row, BORDER selection](docs/img/home-icons.png)
![Settings — text list, BORDER selection](docs/img/settings-list.png)
![WiFi — text list, INVERT selection (default)](docs/img/wifi-invert.png)
![About — text list, ARROW selection](docs/img/about-arrow.png)
![Toast notification on Settings](docs/img/toast.png)

Regenerate after any rendering change: `cd tools/host_render && make`.
```

## .gitignore additions

```text
# Host-side render harness build artifacts
tools/host_render/build/
docs/img/*.pgm
```

The PNGs in `docs/img/` are checked in so GitHub renders them.

## Trade-offs

- **Stubs as a maintenance debt.** Today the menu rendering path is 100%
  RTOS-free, so the stubs need no real behavior. If a future change calls
  FreeRTOS APIs from `render()`, `render_list()`, `render_icons()`, or
  `draw_*`, the stubs must grow to match. This is detectable: the host build
  will fail to link or produce wrong output. Acceptable cost given the
  faithfulness it buys.
- **`sh1106_host.c` is duplicated from `sh1106.c`.** ~50 lines of overlap.
  Avoidable by extracting the framebuffer half of `sh1106.c` into a separate
  source file shared by firmware and host — but that's a refactor of working
  firmware code outside this design's scope. Revisit if the overlap drifts.
- **Single command, two languages.** C harness + Python converter feels like
  one tool too many. A pure-C PNG writer (via vendored `stb_image_write.h`,
  ~1500 LoC) would eliminate the Python step at the cost of carrying a much
  larger third-party header. Python is already a tooling dependency in this
  repo (`tools/gen_icons.py`), so the cost of adding another script is
  smaller than the cost of vendoring `stb_image_write`.

## Out of scope (deferred)

- Visual regression CI (diff new PNGs against committed ones, fail on
  unexpected change).
- Animating multi-frame sequences (e.g. navigation flow as a GIF).
- Rendering at full 128×64 *and* a higher-resolution "marketing" version.
