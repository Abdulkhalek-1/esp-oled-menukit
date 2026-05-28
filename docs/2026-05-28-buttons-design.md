# 3-Button Input Library — Design

**Date:** 2026-05-28
**Goal:** Provide a small, reusable ESP-IDF module that drives three push buttons (back / enter / forward) and emits typed events (pressed, released, long-press, repeat-while-held) onto a FreeRTOS queue. Will be consumed by the upcoming menu sub-project but should drop into other projects unchanged.

## Context

This is sub-project 2a of the navigation feature. Sub-project 2b (menu engine + icons + demo) consumes the queue this library produces. The library has zero dependency on the SH1106 driver or the text-rendering module — they're orthogonal concerns.

## Hardware

- Three momentary push buttons.
- Wiring: one side of each button → GPIO, other side → GND.
- Internal pull-ups enabled on each GPIO; pressed state pulls the pin low.
- Default pins (overridable via the init config):

| Button   | GPIO |
|----------|------|
| BACK     | 32   |
| ENTER    | 33   |
| FORWARD  | 25   |

These three avoid the OLED pins (15/2/18/5/4), the flash pins (6–11), the UART pins (1/3), and the strapping pins (0/12 — used at boot, free at runtime but easy to clash with). GPIO 32/33/25 are full general-purpose GPIOs on the original ESP32 (32–39 are also valid input GPIOs; 34–39 are input-only, so we deliberately avoid those for buttons that might one day need pull-ups driven externally).

## Public API (`main/buttons.h`)

```c
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    BTN_BACK    = 0,
    BTN_ENTER   = 1,
    BTN_FORWARD = 2,
    BTN_COUNT   = 3,
} button_id_t;

typedef enum {
    BTN_EVT_PRESSED,     // debounced press edge
    BTN_EVT_RELEASED,    // debounced release edge
    BTN_EVT_LONG_PRESS,  // fires once when held >= long_press_ms
    BTN_EVT_REPEAT,      // fires every repeat_interval_ms after long-press kicks in
} button_event_type_t;

typedef struct {
    button_id_t         button;
    button_event_type_t event;
} button_event_t;

typedef struct {
    int pins[BTN_COUNT];       // GPIO numbers for BACK, ENTER, FORWARD
    int debounce_ms;           // typical: 20
    int long_press_ms;         // typical: 500
    int repeat_interval_ms;    // typical: 150
} buttons_config_t;

// Initializes the GPIOs, spawns one polling task, returns the event queue.
// Returns NULL on init failure. The queue holds up to 16 button_event_t.
QueueHandle_t buttons_init(const buttons_config_t *cfg);
```

## State machine (per button)

```text
IDLE
  ↓ pin low for >= debounce_ms
PRESSED                       → emit BTN_EVT_PRESSED
  ↓ stays pressed for long_press_ms
LONG_PRESSED                  → emit BTN_EVT_LONG_PRESS
  ↓ stays pressed for repeat_interval_ms
REPEATING                     → emit BTN_EVT_REPEAT (and re-arm)
  ↓ stays pressed for repeat_interval_ms
REPEATING (again)             → emit BTN_EVT_REPEAT
  ...
  ↓ pin high for >= debounce_ms (from any pressed state)
IDLE                          → emit BTN_EVT_RELEASED
```

Each button runs this state machine independently. State lives in a static `per_button_t state[BTN_COUNT]` array inside `buttons.c`, indexed by `button_id_t`. No allocations.

## Internal architecture (`main/buttons.c`)

- `buttons_init(cfg)`:
  1. Configure each pin in `cfg->pins[]` as input with internal pull-up.
  2. Copy timing config to a static struct.
  3. Create an event queue (`xQueueCreate(16, sizeof(button_event_t))`).
  4. Spawn one task at low-medium priority (≈ tskIDLE_PRIORITY + 1, ~2 KB stack).
  5. Return the queue.
- Polling task: every 10 ms, `gpio_get_level()` each pin and drive each button's state machine forward. Push events onto the queue as transitions happen. `xQueueSendToBack` with zero timeout; if the queue fills, oldest events get dropped (caller is presumably stuck or absent — losing input is preferable to blocking the polling task).

10 ms poll cadence × 16-slot queue = 160 ms of buffering before drops. More than enough for a responsive menu.

## Demo (definition of done)

`app_main`:

1. Init OLED + clear + draw a static header `"Buttons demo"` on row 0.
2. Init buttons with default config (pins 32/33/25; timing 20/500/150 ms).
3. Spawn (or run inline) a task that:
   - `xQueueReceive(queue, &evt, portMAX_DELAY)`.
   - Formats `evt` into a string (e.g. `"FWD repeat"`, `"BACK pressed"`, `"ENTER long"`, `"FWD released"`).
   - Logs it via `ESP_LOGI`.
   - Clears row 2 (or 3) and `sh1106_draw_string`s the latest event there, then `sh1106_flush`.

The milestone is reached when:

- Pressing each of the three buttons logs a `*_PRESSED` event and shows it on the screen.
- Releasing the button logs a `*_RELEASED`.
- Holding for ≥ 500 ms produces a `*_LONG_PRESS`, then `*_REPEAT` events at ~150 ms intervals until released.

## Non-goals (this iteration)

- No multi-button combos (e.g. "BACK + ENTER simultaneously").
- No per-button independent timing — all three buttons share the same thresholds via the single config.
- No GPIO ISR — pure polling. Simpler timing model. 10 ms latency is fine for buttons.
- No GPIO debounce hardware (RC filter / cap) assumptions — software debounce only.
- No button matrix or shift-register expansion.
- No "click vs. double-click" distinction.
- No power-saving wake-from-deep-sleep integration.

## Risks / things to watch

- **Pin choice clashes.** GPIO 32/33/25 are full general-purpose GPIOs on the original ESP32. If the board has PSRAM, GPIO 16/17 are reserved by the PSRAM controller — we already avoid those.
- **Pull-up strength.** Internal pull-ups are ~45 kΩ. Long wires to bouncy buttons may still need an external 10 kΩ. 20 ms debounce should mask most chatter.
- **Queue overrun.** If the consumer task gets stuck for > 160 ms while buttons are mashed, events get dropped. Logged via `ESP_LOGW` from the polling task, not silently.
- **Timing precision.** The polling task uses `vTaskDelay(pdMS_TO_TICKS(10))`. At the default ESP-IDF tick rate (100 Hz), that's a 10 ms tick; at higher tick rates the cadence is the same. Long-press / repeat thresholds are rounded to the nearest poll, so a 500 ms threshold may actually fire at 500–510 ms. Imperceptible.
- **Config struct lifetime.** `buttons_init` copies the config into a static struct. Caller may pass a stack-allocated config.

## Future hooks (not for this iteration, just noted)

- Per-button timing override (replace `int long_press_ms` with `int long_press_ms[BTN_COUNT]`).
- Optional callback API layered on top of the queue (`buttons_set_handler(cb)` that spawns an internal consumer).
- ISR-driven first edge + timer for subsequent timing (lower idle CPU usage).
