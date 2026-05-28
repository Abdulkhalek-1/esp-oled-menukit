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
