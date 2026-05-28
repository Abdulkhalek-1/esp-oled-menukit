# SH1106-learn — Design

**Date:** 2026-05-28
**Goal:** Get pixels on a 1.3" SH1106 OLED from an ESP32 over SPI, with a small layered driver, as a starting point for learning. Iterate after first pixels.

## Hardware

- MCU: ESP32 DevKitC / WROOM-32 (original ESP32, dual-core Xtensa).
- Display: 1.3" SH1106 OLED, 7-pin SPI module (GND, VCC, D0, D1, RES, DC, CS).
- Resolution: 128×64 visible. The SH1106 controller addresses 132 columns internally; visible area starts at column offset 2. The driver must account for this on every page write.

## Wiring

| OLED pin | ESP32 GPIO  | Notes                          |
|----------|-------------|--------------------------------|
| GND      | GND         |                                |
| VCC      | 3V3         |                                |
| D0 (SCK) | GPIO 15     | HSPI SCK                       |
| D1 (MOSI)| GPIO 2      | HSPI MOSI                      |
| RES      | GPIO 4      | Manual GPIO, active low        |
| DC       | GPIO 18     | Manual GPIO; 0 = cmd, 1 = data |
| CS       | GPIO 5      | Driven by `spi_master` driver  |

SPI clock starts at 8 MHz (conservative; SH1106 datasheet allows higher, we can push later).

## Software stack

- ESP-IDF (project skeleton already in place — `CMakeLists.txt`, `main/`).
- `esp_driver_spi` (`spi_master.h`) for SPI.
- `driver/gpio.h` for RES and DC.
- No third-party display libraries.

## File layout

```
main/
  CMakeLists.txt   (add sh1106.c to SRCS)
  sh1106.h         (public API)
  sh1106.c         (driver + SPI + framebuffer)
  main.c           (demo: draw border + filled rect, flush)
```

## Driver API (`sh1106.h`)

```c
#define SH1106_WIDTH  128
#define SH1106_HEIGHT 64

void sh1106_init(void);                            // SPI + GPIO + init sequence
void sh1106_clear(void);                           // zero the framebuffer
void sh1106_set_pixel(int x, int y, bool on);      // bit math on framebuffer
void sh1106_flush(void);                           // push framebuffer to display
```

Private to `sh1106.c`:
- `framebuffer[1024]` (128×64 / 8, page-major: each byte = 8 vertical pixels of one page).
- `static spi_device_handle_t dev`.
- `sh1106_cmd(uint8_t)` and `sh1106_data(const uint8_t *buf, size_t len)`.
- Pin and timing constants.

Pixel-level only on purpose. Lines, rectangles, fonts, animations all live in `main.c` (or future files) — keeps the driver focused on "talk to the chip" and lets the higher-level stuff evolve without churning it.

## Initialization sequence

The init in `sh1106_init()` sends this canonical sequence, each line commented with what it does:

1. Configure RES and DC as outputs.
2. Pulse RES low → high (reset the controller).
3. Init SPI bus (HSPI) and add device (8 MHz, mode 0, CS=GPIO 5).
4. Send commands:
   - `0xAE` display off
   - `0xD5 0x80` set display clock divide
   - `0xA8 0x3F` multiplex ratio (64)
   - `0xD3 0x00` display offset 0
   - `0x40` display start line 0
   - `0xAD 0x8B` DC-DC charge pump on (SH1106 quirk — different from SSD1306's `0x8D 0x14`)
   - `0xA1` segment remap (column 127 → SEG0)
   - `0xC8` COM scan direction (top to bottom flipped)
   - `0xDA 0x12` COM pins config
   - `0x81 0x80` contrast
   - `0xD9 0x22` pre-charge period
   - `0xDB 0x35` VCOMH deselect level
   - `0xA4` resume to RAM content
   - `0xA6` normal display (not inverted)
   - `0xAF` display on

## Flush

`sh1106_flush()` walks 8 pages (0–7). For each page:
1. Send command `0xB0 | page` (set page address).
2. Send command `0x02` (lower column = 2 — the visible-area offset).
3. Send command `0x10` (higher column = 0).
4. Send 128 bytes of framebuffer data for that page.

## `set_pixel`

Page = `y / 8`. Bit within byte = `y % 8`. Byte index = `page * 128 + x`. Set or clear that bit.

## Demo (`app_main`)

1. `sh1106_init()`.
2. `sh1106_clear()`.
3. Draw a 1-pixel border around the full 128×64.
4. Draw a filled rectangle in the middle (e.g. 40×20 centered).
5. `sh1106_flush()`.
6. Done — task returns; image stays on screen.

## Definition of done

Flash the board → border and rectangle visible on the OLED. That's the milestone for this design. After that we iterate based on curiosity (text? animation? partial updates? buttons?) — each next step gets its own small design if it's non-trivial, otherwise just do it.

## Explicit non-goals (for this iteration)

- No font / text rendering.
- No DMA, no double-buffering, no async transfers.
- No FreeRTOS task dedicated to display (just call from `app_main`).
- No graphics library (LVGL, u8g2) integration.
- No I2C variant.
- No PlatformIO — stay on ESP-IDF.

## Risks / things to watch

- **Charge pump command:** SH1106 uses `0xAD 0x8B`, not the SSD1306 `0x8D 0x14`. Mixing these up = blank screen, no error. Worth a comment in the code.
- **Column offset:** forgetting the `+2` on every page write = the rightmost 2 columns of intended content wrap, leftmost garbage on display. Easy to miss.
- **Reset timing:** RES low needs to be held briefly (≥3 µs per datasheet); use `vTaskDelay` or `esp_rom_delay_us`.
- **DC pin timing:** DC must be set *before* the SPI transaction starts. Easiest path: set DC in `sh1106_cmd` / `sh1106_data` immediately before `spi_device_polling_transmit`.
