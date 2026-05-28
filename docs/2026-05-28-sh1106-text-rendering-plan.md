# SH1106 Text Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render fixed-width 8×8 ASCII text on the SH1106 OLED by adding one new public function (`sh1106_draw_string`) and a vendored u8g2 bitmap font.

**Architecture:** New files `main/font8x8.{h,c}` own the font data (BSD-2-Clause from u8g2). `main/sh1106.c` gains an internal `draw_char` and the public `sh1106_draw_string`. `main/main.c` switches its demo from border+rect to two lines of text. Existing pixel-level driver is untouched.

**Tech Stack:** ESP-IDF v5+, ESP32, the existing hand-rolled SH1106 driver. No new runtime libraries, no new managed components.

**Reference spec:** [2026-05-28-sh1106-text-rendering-design.md](./2026-05-28-sh1106-text-rendering-design.md)

**Note on testing:** Same as the prior plan — each task's verification is `idf.py build` and, where applicable, `idf.py flash monitor` plus a visual check on the OLED. There is no host-runnable unit test.

---

## File Structure

After all tasks complete:

```text
main/
  CMakeLists.txt   (modified — adds font8x8.c)
  sh1106.h         (modified — adds sh1106_draw_string prototype)
  sh1106.c         (modified — adds draw_char static + sh1106_draw_string)
  font8x8.h        (new — extern declaration)
  font8x8.c        (new — 768 bytes of glyph data + BSD-2-Clause header)
  main.c           (modified — text demo)
```

**Responsibilities:**

- `font8x8.h / font8x8.c` — owns the glyph data only. Pure data, no logic. Layout: `const uint8_t font8x8[96][8]`, indexed by `c - 32` for printable ASCII (32–127). Each glyph stored column-major, LSB = top pixel.
- `sh1106.c` — owns rendering. Adds a static `draw_char` and public `sh1106_draw_string`. Pixel-level driver responsibilities unchanged.
- `main.c` — calls `sh1106_draw_string` for the demo. The previous border+rect drawing helpers are removed.

---

## Task 1: Vendor the u8g2 font as `font8x8.{h,c}`

**Files:**
- Create: `main/font8x8.h`
- Create: `main/font8x8.c`
- Modify: `main/CMakeLists.txt`

**Font source:** `u8x8_font_amstrad_cpc_extended_f` from u8g2 upstream. Canonical URL:

```
https://raw.githubusercontent.com/olikraus/u8g2/master/csrc/u8x8_fonts.c
```

License: BSD-2-Clause. The full header at the top of `u8x8_fonts.c` must be reproduced verbatim at the top of `font8x8.c`.

**Format note.** u8x8 8×8 fonts are stored column-major: byte N of a glyph = column N (left → right). LSB of each byte = top pixel, MSB = bottom. For `_f` (full) variants, all 256 codepoints are present (or a defined range — we only need 32–127).

- [ ] **Step 1: Fetch upstream and extract the font bytes for ASCII 32–127**

Fetch `https://raw.githubusercontent.com/olikraus/u8g2/master/csrc/u8x8_fonts.c`. Locate the `u8x8_font_amstrad_cpc_extended_f` declaration. Extract its glyph bytes corresponding to the printable ASCII range (codepoint 32 through 127 inclusive). If the upstream font includes a leading header (start_codepoint / end_codepoint marker bytes), skip the header — only the raw 8-bytes-per-glyph payload goes into our array.

Result: 96 glyphs × 8 bytes = 768 bytes, in codepoint order starting at space (32).

- [ ] **Step 2: Write `main/font8x8.h`**

```c
#pragma once

#include <stdint.h>

// 8x8 bitmap font, printable ASCII (32-127).
// Source: u8g2 project, font `u8x8_font_amstrad_cpc_extended_f` (BSD-2-Clause).
// Layout: column-major. font8x8[c - 32][col] is one column of glyph c.
//   LSB of that byte = top pixel; MSB = bottom pixel.
extern const uint8_t font8x8[96][8];
```

- [ ] **Step 3: Write `main/font8x8.c`**

File structure:

```c
/*
  font8x8.c — vendored from the u8g2 project.

  Original source: https://github.com/olikraus/u8g2 (csrc/u8x8_fonts.c)
  Font: u8x8_font_amstrad_cpc_extended_f
  License: BSD-2-Clause

  ----------------------------------------------------------------------------
  <PASTE THE FULL u8g2 BSD-2-CLAUSE LICENSE BLOCK FROM csrc/u8x8_fonts.c HERE>
  ----------------------------------------------------------------------------
*/

#include "font8x8.h"

const uint8_t font8x8[96][8] = {
    /* 0x20 ' '  */ { /* 8 bytes */ },
    /* 0x21 '!'  */ { /* 8 bytes */ },
    /* ... 94 more glyphs through 0x7F ... */
};
```

The 8 bytes per row come from Step 1.

- [ ] **Step 4: Register `font8x8.c` with CMake**

Modify `main/CMakeLists.txt`:

```cmake
idf_component_register(SRCS "sh1106.c" "main.c" "font8x8.c"
                    INCLUDE_DIRS ".")
```

(Order of SRCS doesn't matter to the build — match whichever order is already there.)

- [ ] **Step 5: Verify build**

Run: `idf.py build`
Expected: succeeds. `font8x8.c` compiles cleanly. The array is unused so far (no callers), and ESP-IDF tolerates that for global data without warnings.

- [ ] **Step 6: Commit**

```bash
git add main/font8x8.h main/font8x8.c main/CMakeLists.txt
git commit -m "feat: vendor u8g2 8x8 font as font8x8"
```

---

## Task 2: Add the `sh1106_draw_string` API as a stub

**Files:**
- Modify: `main/sh1106.h`
- Modify: `main/sh1106.c`

- [ ] **Step 1: Declare the function in `sh1106.h`**

Add the prototype below the existing function declarations:

```c
void sh1106_draw_string(int x, int y, const char *s);
```

Final `main/sh1106.h` body (for clarity):

```c
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SH1106_WIDTH  128
#define SH1106_HEIGHT 64

void sh1106_init(void);
void sh1106_clear(void);
void sh1106_set_pixel(int x, int y, bool on);
void sh1106_flush(void);
void sh1106_draw_string(int x, int y, const char *s);
```

- [ ] **Step 2: Add a no-op stub in `sh1106.c`**

At the bottom of `main/sh1106.c` (after `sh1106_flush`):

```c
void sh1106_draw_string(int x, int y, const char *s)
{
    (void)x; (void)y; (void)s;
}
```

- [ ] **Step 3: Verify build**

Run: `idf.py build`
Expected: succeeds. New symbol exists, isn't called yet.

- [ ] **Step 4: Commit**

```bash
git add main/sh1106.h main/sh1106.c
git commit -m "feat: declare sh1106_draw_string API stub"
```

---

## Task 3: Implement `draw_char` and `sh1106_draw_string`

**Files:**
- Modify: `main/sh1106.c`

- [ ] **Step 1: Include the font header**

Add to the top of `main/sh1106.c` (with the other includes):

```c
#include "font8x8.h"
```

- [ ] **Step 2: Add the static `draw_char` helper**

Place above `sh1106_draw_string` in `main/sh1106.c`:

```c
static void draw_char(int x, int y, char c)
{
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc > 127) uc = '?';
    const uint8_t *glyph = font8x8[uc - 32];

    for (int col = 0; col < 8; col++) {
        uint8_t bits = glyph[col];      // one column of the glyph
        for (int row = 0; row < 8; row++) {
            if (bits & (1 << row)) {    // LSB = top, so bit `row` = pixel row
                sh1106_set_pixel(x + col, y + row, true);
            }
        }
    }
}
```

- [ ] **Step 3: Replace the `sh1106_draw_string` stub**

Replace the stub body in `main/sh1106.c` with:

```c
void sh1106_draw_string(int x, int y, const char *s)
{
    while (*s) {
        draw_char(x, y, *s++);
        x += 8;
    }
}
```

- [ ] **Step 4: Verify build**

Run: `idf.py build`
Expected: succeeds. No compiler warnings.

- [ ] **Step 5: Commit**

```bash
git add main/sh1106.c
git commit -m "feat: implement draw_char and sh1106_draw_string"
```

---

## Task 4: Text demo — milestone

**Files:**
- Modify: `main/main.c`

- [ ] **Step 1: Replace `main.c` with the text demo**

Replace `main/main.c` entirely with:

```c
#include "sh1106.h"

void app_main(void)
{
    sh1106_init();
    sh1106_clear();
    sh1106_draw_string(0, 0, "Hello, OLED!");
    sh1106_draw_string(0, 8, "SH1106-learn");
    sh1106_flush();
}
```

The previous `draw_border` and `draw_filled_rect` helpers are intentionally removed — this milestone is about text. They live in git history (commit `fd8d9ed`) if we ever want them back.

- [ ] **Step 2: Build, flash, look at the display**

Run: `idf.py build flash monitor`

Expected on the OLED:

- Top-left: `Hello, OLED!` in 8×8 glyphs, 12 chars wide = 96 px wide.
- Just below it (y=8): `SH1106-learn`, 12 chars wide = 96 px wide.
- No other content. Rest of the screen black.

Common failures:

- **Glyphs upside-down or vertically mirrored** → inner loop bit-order wrong (LSB-top vs MSB-top swap). Re-check the `(1 << row)` math against the font format note.
- **Glyphs 90° rotated** → row/col swap in the renderer. We want byte = column, bit-in-byte = row.
- **First column of every glyph blank or extra pixels at start** → off-by-one in the codepoint math; `(c - 32)` should map ' ' (0x20) to index 0.
- **All glyphs render as `?`** → all incoming chars failed the printable check; verify the cast to `unsigned char`.

- [ ] **Step 3: Commit**

```bash
git add main/main.c
git commit -m "feat: render Hello, OLED! text demo"
```

- [ ] **Step 4: Milestone reached**

Text rendering sub-project complete. The next sub-project (buttons + menu) will reuse `sh1106_draw_string` and gets its own design + plan + execution cycle.

---

## Self-review notes

- **Spec coverage:** every section of the spec maps to a task. Font sourcing → Task 1. File layout → Tasks 1–4. API → Tasks 2–3. Renderer sketch → Task 3. Demo → Task 4. Non-goals respected (no variable width, no multi-font, no formatting).
- **Placeholders:** Task 1, Step 3 references `<PASTE THE FULL u8g2 BSD-2-CLAUSE LICENSE BLOCK ... HERE>` and `/* 8 bytes */` — these are placeholders that the executor must replace with the real upstream license text and the real glyph bytes. They are intentional and named — the executor (me, during inline execution) fetches the upstream file in Step 1 and substitutes the real content in Step 3. No vague TODOs anywhere else.
- **Type consistency:** `font8x8[96][8]` declared in the header (Task 1, Step 2) and used identically in the renderer (Task 3, Step 2). `sh1106_draw_string(int x, int y, const char *s)` matches between header (Task 2, Step 1), stub (Task 2, Step 2), final implementation (Task 3, Step 3), and call sites (Task 4, Step 1). `(unsigned char)c` cast is consistent with the format and signedness risk callout in the spec.
- **Spec risks addressed:** column-major confusion → format note in Task 1 + inline comment in Task 3 + diagnostic in Task 4. License preservation → explicit step to reproduce header. Glyph index math → addressed by Task 4 diagnostic. `char` signedness → explicit cast in Task 3.
