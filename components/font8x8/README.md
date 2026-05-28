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
