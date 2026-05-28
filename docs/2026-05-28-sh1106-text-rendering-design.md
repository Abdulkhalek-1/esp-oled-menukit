# SH1106 Text Rendering — Design

**Date:** 2026-05-28
**Goal:** Render fixed-width 8×8 ASCII text on the SH1106 OLED, building on the existing pixel-level driver. Milestone: `Hello, OLED!` visible on the display.

## Context

Continues from [2026-05-28-sh1106-spi-learning-design.md](./2026-05-28-sh1106-spi-learning-design.md). The driver currently exposes `sh1106_init / clear / set_pixel / flush`. This sub-project adds **one** new public function — `sh1106_draw_string` — and the font data it needs.

Text rendering is a prerequisite for the upcoming buttons + menu sub-project. It is independently useful and complete on its own.

## Approach

Vendor a single 8×8 fixed-width font from the u8g2 project as static C data. Write a small renderer (~25 lines) that walks each glyph and calls the existing `sh1106_set_pixel`. No runtime library, no new managed components.

## Font

- **Source:** `u8x8_font_amstrad_cpc_extended_f` from the u8g2 project (`csrc/u8x8_fonts.c` in upstream). BSD-2-Clause. Pinned choice: it's a classic 8×8 CGA-style face, full ASCII coverage, easy to read. We vendor only the relevant byte range as `main/font8x8.c` with the u8g2 license header preserved verbatim.
- **Format:** column-major. Each glyph = 8 bytes. Byte N = column N (left → right). LSB of each byte = top pixel, MSB = bottom pixel.
- **Range:** ASCII 32–127 (printable). 96 glyphs × 8 bytes = 768 bytes of font data.
- **Out-of-range handling:** any char `< 32` or `> 127` renders as `?` (the printable substitute at index `'?' - 32 = 31`).

## File layout

```text
main/
  CMakeLists.txt   (modified — adds font8x8.c)
  sh1106.h         (modified — adds draw_string prototype)
  sh1106.c         (modified — adds draw_char static + draw_string)
  font8x8.h        (new — declares the extern array)
  font8x8.c        (new — the glyph bytes + license header)
  main.c           (modified — text demo)
```

**Responsibilities:**

- `font8x8.h / font8x8.c` — owns the glyph data only. Pure data, no logic.
- `sh1106.c` — owns rendering. Imports the font header. Pixel-level driver responsibilities unchanged.
- `main.c` — calls `draw_string` at specific pixel coordinates.

## API

```c
// in sh1106.h
void sh1106_draw_string(int x, int y, const char *s);
```

Draws null-terminated string `s` starting at pixel `(x, y)`, advancing x by 8 per character. Each glyph occupies an 8×8 pixel box. Pixels outside the 128×64 area are silently clipped by `sh1106_set_pixel` (already implemented).

## Renderer (sketch)

```c
#include "font8x8.h"

static void draw_char(int x, int y, char c)
{
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc > 127) uc = '?';
    const uint8_t *glyph = font8x8[uc - 32];

    for (int col = 0; col < 8; col++) {
        uint8_t bits = glyph[col];        // one column of the glyph
        for (int row = 0; row < 8; row++) {
            if (bits & (1 << row)) {      // LSB = top, so bit `row` = pixel row
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
```

## Demo (definition of done)

`app_main` flow:

1. `sh1106_init()`
2. `sh1106_clear()`
3. `sh1106_draw_string(0, 0, "Hello, OLED!");`
4. `sh1106_draw_string(0, 8, "SH1106-learn");`
5. `sh1106_flush()`

Visible: two lines of 8×8 text at the top-left of the display, lines stacked with no extra spacing. That's the milestone.

## Non-goals (this iteration)

- No variable-width fonts.
- No multiple fonts at runtime.
- No text wrapping, right-alignment, or center-alignment helpers.
- No printf-style formatting inside the API (caller uses `snprintf` if needed).
- No UTF-8 or non-ASCII glyphs.
- No inverted text, no bold, no underline.
- No `draw_char` exposed in the public API (kept static).

## Risks / things to watch

- **Column-major confusion.** u8x8 fonts pack bytes column-by-column with LSB at the top. Flipping the inner loop to row-major produces a 90° rotation or vertical mirror. Visual test catches this fast.
- **Glyph index math.** `(uc - 32) * 8` is the byte offset into a flat array; we use a 2D array `font8x8[96][8]` so it's `font8x8[uc - 32]`. Just don't mix the two access patterns.
- **License preservation.** u8g2 is BSD-2-Clause. We must keep the copyright header in `font8x8.c`. Mention this in the file header.
- **`char` signedness.** On platforms where `char` is signed, values `> 127` become negative. The cast to `unsigned char` before comparison handles this.
