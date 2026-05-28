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
