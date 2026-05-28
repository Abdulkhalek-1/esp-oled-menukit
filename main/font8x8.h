#pragma once

#include <stdint.h>

/* 8x8 bitmap font, printable ASCII (32-127).
 * Source: u8g2 project, font `u8x8_font_amstrad_cpc_extended_f`.
 * Layout: column-major. font8x8[c - 32][col] is one column of glyph c.
 *   LSB of that byte = top pixel; MSB = bottom pixel.
 */
extern const uint8_t font8x8[96][8];
