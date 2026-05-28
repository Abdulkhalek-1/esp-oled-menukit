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
