# SH1106-learn Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Get a border + filled rectangle drawn on a 1.3" SH1106 OLED connected over SPI to an ESP32 DevKitC, via a small layered driver written from scratch.

**Architecture:** ESP-IDF project with three source files in `main/`: `sh1106.h` (public API), `sh1106.c` (SPI + GPIO + framebuffer + init/flush/pixel logic), `main.c` (demo). Driver is pixel-level only; drawing primitives live in `main.c`. Single-buffer, synchronous, polling SPI transfers — no DMA, no FreeRTOS task.

**Tech Stack:** ESP-IDF v5+, ESP32 (Xtensa, original DevKitC/WROOM-32), HSPI (SPI2_HOST), `driver/spi_master.h`, `driver/gpio.h`.

**Note on testing:** This is firmware driving a physical display. There is no host-runnable unit test for "did the OLED light up." Each task's "test" is `idf.py build` to confirm it compiles, and `idf.py flash monitor` plus a visual check of the display where applicable. The plan structures work as frequent commits with explicit visual checkpoints — that is the embedded analogue to TDD here.

**Reference spec:** [2026-05-28-sh1106-spi-learning-design.md](./2026-05-28-sh1106-spi-learning-design.md)

---

## File Structure

After all tasks complete:

```
main/
  CMakeLists.txt   (modified — adds sh1106.c)
  sh1106.h         (new — public API)
  sh1106.c         (new — driver implementation)
  main.c           (modified — demo)
docs/
  2026-05-28-sh1106-spi-learning-design.md   (existing)
  2026-05-28-sh1106-spi-learning-plan.md     (this file)
```

**Responsibilities:**
- `sh1106.h` — declares 4 public functions and `SH1106_WIDTH` / `SH1106_HEIGHT`. Nothing else.
- `sh1106.c` — owns the framebuffer, the SPI handle, the pin constants, the init sequence, and the cmd/data SPI helpers. Self-contained: changing the driver shouldn't require touching `main.c`.
- `main.c` — calls the driver. Holds drawing primitives (just border + rect for this milestone).

---

## Task 1: Scaffold the sh1106 module and wire it into the build

**Files:**
- Create: `main/sh1106.h`
- Create: `main/sh1106.c`
- Modify: `main/CMakeLists.txt`

- [ ] **Step 1: Create the header skeleton**

Create `main/sh1106.h`:

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
```

- [ ] **Step 2: Create the source skeleton**

Create `main/sh1106.c`:

```c
#include "sh1106.h"

void sh1106_init(void)  {}
void sh1106_clear(void) {}
void sh1106_set_pixel(int x, int y, bool on) { (void)x; (void)y; (void)on; }
void sh1106_flush(void) {}
```

- [ ] **Step 3: Register sh1106.c with CMake**

Modify `main/CMakeLists.txt`:

```cmake
idf_component_register(SRCS "main.c" "sh1106.c"
                    INCLUDE_DIRS ".")
```

- [ ] **Step 4: Verify the project still builds**

Run: `idf.py build`
Expected: build succeeds. `sh1106.c` compiles with no warnings (empty stubs). `app_main` still does nothing.

- [ ] **Step 5: Commit**

If the project isn't a git repo yet, initialize it first:

```bash
git init
git add -A
git commit -m "chore: initial project skeleton"
```

Otherwise just:

```bash
git add main/sh1106.h main/sh1106.c main/CMakeLists.txt
git commit -m "feat: scaffold sh1106 module"
```

---

## Task 2: Add pin constants, framebuffer, and call the driver from main

**Files:**
- Modify: `main/sh1106.c`
- Modify: `main/main.c`

- [ ] **Step 1: Add pin defines and the framebuffer to sh1106.c**

Replace `main/sh1106.c` with:

```c
#include "sh1106.h"

#include <string.h>

// Wiring — ESP32 DevKitC, HSPI bus.
#define PIN_SCK   18
#define PIN_MOSI  23
#define PIN_CS     5
#define PIN_DC    17
#define PIN_RES   16

#define SH1106_PAGES         (SH1106_HEIGHT / 8)   // 8 pages of 8 rows each.
#define SH1106_FB_SIZE       (SH1106_WIDTH * SH1106_PAGES)
#define SH1106_COL_OFFSET    2  // SH1106 has 132 columns; visible area starts at 2.

static uint8_t framebuffer[SH1106_FB_SIZE];

void sh1106_init(void)  {}
void sh1106_clear(void) { memset(framebuffer, 0, sizeof(framebuffer)); }
void sh1106_set_pixel(int x, int y, bool on) { (void)x; (void)y; (void)on; }
void sh1106_flush(void) {}
```

- [ ] **Step 2: Wire the driver into app_main**

Replace `main/main.c` with:

```c
#include "sh1106.h"

void app_main(void)
{
    sh1106_init();
    sh1106_clear();
    sh1106_flush();
}
```

- [ ] **Step 3: Verify it builds**

Run: `idf.py build`
Expected: build succeeds. `sh1106_clear` zeroes the framebuffer; `init` and `flush` still do nothing.

- [ ] **Step 4: Commit**

```bash
git add main/sh1106.c main/main.c
git commit -m "feat: add framebuffer, pin constants, and driver entry calls"
```

---

## Task 3: Initialize the RES and DC GPIOs and the SPI bus

**Files:**
- Modify: `main/sh1106.c`

This task sets up the hardware interfaces but does not yet talk to the OLED. After flashing you'll see the ESP32 boot normally and the RES line will pulse low once. Nothing on the display yet.

- [ ] **Step 1: Add the SPI + GPIO includes and the device handle**

At the top of `main/sh1106.c`, add (after `#include <string.h>`):

```c
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
```

And below the pin defines, add the SPI handle:

```c
static spi_device_handle_t dev;
```

- [ ] **Step 2: Implement sh1106_init's GPIO + SPI setup**

Replace the body of `sh1106_init` in `main/sh1106.c`:

```c
void sh1106_init(void)
{
    // 1. Configure RES and DC as outputs.
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_RES) | (1ULL << PIN_DC),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    // 2. Pulse RES low → high to reset the controller.
    gpio_set_level(PIN_RES, 0);
    esp_rom_delay_us(10);              // datasheet: >=3us
    gpio_set_level(PIN_RES, 1);
    vTaskDelay(pdMS_TO_TICKS(10));     // let the chip come up

    // 3. Init the SPI bus (HSPI = SPI2_HOST).
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,             // OLED is write-only
        .sclk_io_num = PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SH1106_WIDTH,  // one page at a time
    };
    spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);

    // 4. Add the OLED as a device on that bus.
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000,  // 8 MHz — conservative
        .mode = 0,                          // CPOL=0, CPHA=0
        .spics_io_num = PIN_CS,             // SPI driver toggles CS
        .queue_size = 1,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &dev);
}
```

- [ ] **Step 3: Build and flash**

Run: `idf.py build flash monitor`
Expected: build succeeds, board boots normally. Display still shows nothing (we haven't sent any commands yet). If `idf.py` complains "no port", connect the ESP32 and pass `-p /dev/ttyUSB0` (or whatever your port is). Exit monitor with `Ctrl-]`.

- [ ] **Step 4: Commit**

```bash
git add main/sh1106.c
git commit -m "feat: init RES/DC GPIOs and HSPI bus"
```

---

## Task 4: Implement the cmd / data SPI helpers

**Files:**
- Modify: `main/sh1106.c`

- [ ] **Step 1: Add the helpers above sh1106_init**

In `main/sh1106.c`, add these two static functions before `sh1106_init`:

```c
static void sh1106_cmd(uint8_t c)
{
    gpio_set_level(PIN_DC, 0);  // 0 = command
    spi_transaction_t t = {
        .length = 8,            // bits, not bytes
        .tx_buffer = &c,
    };
    spi_device_polling_transmit(dev, &t);
}

static void sh1106_data(const uint8_t *buf, size_t len)
{
    gpio_set_level(PIN_DC, 1);  // 1 = data
    spi_transaction_t t = {
        .length = len * 8,      // bits
        .tx_buffer = buf,
    };
    spi_device_polling_transmit(dev, &t);
}
```

- [ ] **Step 2: Build**

Run: `idf.py build`
Expected: build succeeds. The two helpers compile but aren't called yet — that's fine; ESP-IDF tolerates unused statics, but if you get a `-Wunused-function` warning, ignore it; the next task uses them. If it's an error in your toolchain config, briefly suppress with `__attribute__((unused))` and the next task will remove that.

- [ ] **Step 3: Commit**

```bash
git add main/sh1106.c
git commit -m "feat: add sh1106_cmd and sh1106_data SPI helpers"
```

---

## Task 5: Send the SH1106 initialization sequence

**Files:**
- Modify: `main/sh1106.c`

This task makes the OLED power-on. After flashing, the display should turn on. The screen content will likely look like random noise or garbage (because the controller's internal RAM has random power-on contents and we haven't cleared it yet). That's the expected visual checkpoint for this task.

- [ ] **Step 1: Append the init sequence to sh1106_init**

At the bottom of `sh1106_init` in `main/sh1106.c` (after `spi_bus_add_device`), append:

```c
    // 5. SH1106 power-on init sequence.
    sh1106_cmd(0xAE);              // display off
    sh1106_cmd(0xD5); sh1106_cmd(0x80);  // clock divide ratio / oscillator freq
    sh1106_cmd(0xA8); sh1106_cmd(0x3F);  // multiplex ratio = 64
    sh1106_cmd(0xD3); sh1106_cmd(0x00);  // display offset = 0
    sh1106_cmd(0x40);              // display start line = 0
    sh1106_cmd(0xAD); sh1106_cmd(0x8B);  // DC-DC charge pump ON (SH1106-specific; do NOT use SSD1306's 0x8D 0x14)
    sh1106_cmd(0xA1);              // segment remap: column 127 -> SEG0
    sh1106_cmd(0xC8);              // COM scan direction: remapped (top-to-bottom flipped)
    sh1106_cmd(0xDA); sh1106_cmd(0x12);  // COM pins hardware config
    sh1106_cmd(0x81); sh1106_cmd(0x80);  // contrast = 128/255
    sh1106_cmd(0xD9); sh1106_cmd(0x22);  // pre-charge period
    sh1106_cmd(0xDB); sh1106_cmd(0x35);  // VCOMH deselect level
    sh1106_cmd(0xA4);              // resume to RAM content (not all-on)
    sh1106_cmd(0xA6);              // normal display (not inverted)
    sh1106_cmd(0xAF);              // display on
```

- [ ] **Step 2: Build, flash, and look at the display**

Run: `idf.py build flash monitor`
Expected:
- Build and flash succeed.
- After the ESP32 boots and `app_main` runs, the OLED powers on.
- You will probably see random/garbage pixels on the screen. **This is correct** — we haven't pushed framebuffer contents yet, so the OLED is showing whatever was in its internal RAM at power-up.
- If the screen stays completely dark: double-check wiring (especially RES, DC, CS, MOSI, SCK), confirm `0xAD 0x8B` is the charge-pump line (not `0x8D 0x14`), and check that VCC is 3.3V.

- [ ] **Step 3: Commit**

```bash
git add main/sh1106.c
git commit -m "feat: send SH1106 power-on init sequence"
```

---

## Task 6: Implement flush, and verify the display goes fully dark

**Files:**
- Modify: `main/sh1106.c`

After this task the framebuffer (all zeros from `sh1106_clear`) gets pushed to the display, replacing the power-on garbage with a clean black screen. That's the visual checkpoint.

- [ ] **Step 1: Implement sh1106_flush**

Replace the empty `sh1106_flush` in `main/sh1106.c` with:

```c
void sh1106_flush(void)
{
    for (uint8_t page = 0; page < SH1106_PAGES; page++) {
        sh1106_cmd(0xB0 | page);                          // set page address (0xB0-0xB7)
        sh1106_cmd(0x00 | (SH1106_COL_OFFSET & 0x0F));    // lower column nibble  = 2
        sh1106_cmd(0x10 | ((SH1106_COL_OFFSET >> 4) & 0x0F)); // higher column nibble = 0
        sh1106_data(&framebuffer[page * SH1106_WIDTH], SH1106_WIDTH);
    }
}
```

- [ ] **Step 2: Build, flash, and look at the display**

Run: `idf.py build flash monitor`
Expected:
- The OLED is fully black after boot (every pixel off).
- If you still see garbage: most likely the column offset is wrong — re-check that we send `0x02` for lower-column and `0x10` for higher-column on every page.
- If only part of the screen is black: check that `SH1106_PAGES` is 8 and the loop ran 8 times.

- [ ] **Step 3: Commit**

```bash
git add main/sh1106.c
git commit -m "feat: implement sh1106_flush with column offset for visible area"
```

---

## Task 7: Implement set_pixel and draw the border + filled rectangle

**Files:**
- Modify: `main/sh1106.c`
- Modify: `main/main.c`

Final task. After this, the OLED shows a 1-pixel border around the whole 128×64 area and a 40×20 filled rectangle centered in it. That's the milestone defined in the spec.

- [ ] **Step 1: Implement sh1106_set_pixel**

Replace the empty `sh1106_set_pixel` in `main/sh1106.c` with:

```c
void sh1106_set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= SH1106_WIDTH)  return;
    if (y < 0 || y >= SH1106_HEIGHT) return;

    // Page-major framebuffer: each byte holds 8 vertically-stacked pixels of one page.
    int page = y / 8;
    int bit  = y % 8;
    int idx  = page * SH1106_WIDTH + x;

    if (on) framebuffer[idx] |=  (1 << bit);
    else    framebuffer[idx] &= ~(1 << bit);
}
```

- [ ] **Step 2: Replace main.c with the demo**

Replace `main/main.c` with:

```c
#include "sh1106.h"

static void draw_border(void)
{
    for (int x = 0; x < SH1106_WIDTH; x++) {
        sh1106_set_pixel(x, 0,                  true);
        sh1106_set_pixel(x, SH1106_HEIGHT - 1,  true);
    }
    for (int y = 0; y < SH1106_HEIGHT; y++) {
        sh1106_set_pixel(0,                 y, true);
        sh1106_set_pixel(SH1106_WIDTH - 1,  y, true);
    }
}

static void draw_filled_rect(int x0, int y0, int w, int h)
{
    for (int y = y0; y < y0 + h; y++) {
        for (int x = x0; x < x0 + w; x++) {
            sh1106_set_pixel(x, y, true);
        }
    }
}

void app_main(void)
{
    sh1106_init();
    sh1106_clear();
    draw_border();
    draw_filled_rect((SH1106_WIDTH - 40) / 2, (SH1106_HEIGHT - 20) / 2, 40, 20);
    sh1106_flush();
}
```

- [ ] **Step 3: Build, flash, look at the display**

Run: `idf.py build flash monitor`
Expected:
- A 1-pixel rectangle border framing the entire 128×64 area.
- A solid filled 40×20 rectangle centered horizontally and vertically.
- If the border is missing the top or right edge: probably an off-by-one in `draw_border`.
- If the filled rect is off-center: re-check the math `(SH1106_WIDTH - 40) / 2` etc.
- If everything is shifted left/right by 2 columns and the rightmost 2 columns are blank: the column offset isn't being applied — go back to `sh1106_flush`.

- [ ] **Step 4: Commit**

```bash
git add main/sh1106.c main/main.c
git commit -m "feat: draw border and centered filled rectangle"
```

- [ ] **Step 5: You are done with the milestone**

The spec's "definition of done" is satisfied. From here, iterate based on curiosity — text? animation? a small graphics library? Each next step gets its own short design + plan if it's non-trivial; otherwise just hack on it.

---

## Self-review notes

Verified before handoff:
- Every spec section maps to a task: hardware/wiring → Task 3; file layout → Task 1; driver API → Tasks 1, 4, 6, 7; init sequence → Task 5; flush → Task 6; set_pixel → Task 7; demo → Task 7; non-goals respected (no DMA, no fonts, no LVGL, no FreeRTOS task).
- No placeholders, TBDs, or "implement later" stubs.
- Type consistency: `sh1106_set_pixel(int x, int y, bool on)` matches in header, in skeleton, in final implementation, and in callers. `SH1106_WIDTH`, `SH1106_HEIGHT`, `SH1106_PAGES`, `SH1106_COL_OFFSET` are introduced once and used consistently.
- Spec mentions `vTaskDelay` or `esp_rom_delay_us` for reset timing — Task 3 uses both (`esp_rom_delay_us(10)` for the >=3µs low pulse, `vTaskDelay(pdMS_TO_TICKS(10))` for the post-reset settle).
- Spec mentions DC must be set before the SPI transaction starts — Task 4 sets DC immediately before `spi_device_polling_transmit` in both `sh1106_cmd` and `sh1106_data`.
- Spec calls out `0xAD 0x8B` vs `0x8D 0x14` charge-pump confusion — Task 5 has an inline comment naming this trap.
- Spec calls out the column-offset trap — Task 6 has an inline comment naming this too.
