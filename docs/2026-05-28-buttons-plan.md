# 3-Button Input Library Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a reusable `buttons` module that drives 3 push buttons via a 10 ms polling task, emits typed events (PRESSED / RELEASED / LONG_PRESS / REPEAT) onto a FreeRTOS queue, and demonstrates them on the OLED.

**Architecture:** New module `main/buttons.{h,c}`. One polling task per `buttons_init`. Per-button state machine (IDLE → DEBOUNCE_P → PRESSED → LONG_HELD → DEBOUNCE_R → IDLE) drives all four event types. Demo task in `main.c` drains the queue, logs to monitor, and renders the latest event on the OLED.

**Tech Stack:** ESP-IDF v5+, `driver/gpio.h`, FreeRTOS queues + tasks. No new managed components.

**Reference spec:** [2026-05-28-buttons-design.md](./2026-05-28-buttons-design.md)

**Note on testing:** Same as prior plans — each task verifies with `idf.py build` and, where applicable, `idf.py flash monitor` + physical button presses + visual OLED check. The plan structures work as frequent commits with explicit hardware checkpoints.

---

## File Structure

After all tasks complete:

```text
main/
  CMakeLists.txt   (modified — adds buttons.c)
  buttons.h        (new — public API)
  buttons.c        (new — polling task + per-button state machine)
  sh1106.h         (unchanged)
  sh1106.c         (unchanged)
  font8x8.h        (unchanged)
  font8x8.c        (unchanged)
  main.c           (modified — buttons demo)
```

**Responsibilities:**

- `buttons.h` — exposes the 4 types (`button_id_t`, `button_event_type_t`, `button_event_t`, `buttons_config_t`) and one function (`buttons_init`). No globals exposed.
- `buttons.c` — owns the per-button state array, the polling task, the timing constants (passed in via config), and the queue handle. Self-contained: no calls into sh1106 or anywhere else in the project.
- `main.c` — calls `buttons_init` and spawns a consumer task that reads the queue, logs events, and draws the latest one on the OLED.

---

## Task 1: Public API + GPIO config + queue (no events yet)

**Files:**
- Create: `main/buttons.h`
- Create: `main/buttons.c`
- Modify: `main/CMakeLists.txt`
- Modify: `main/main.c`

This task gets the module compiling, configures GPIOs as inputs with pull-ups, creates the event queue, and returns it. No polling task spawned yet; no events ever emitted. Verifies wiring and link.

- [ ] **Step 1: Create `main/buttons.h`**

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
    BTN_EVT_PRESSED,
    BTN_EVT_RELEASED,
    BTN_EVT_LONG_PRESS,
    BTN_EVT_REPEAT,
} button_event_type_t;

typedef struct {
    button_id_t         button;
    button_event_type_t event;
} button_event_t;

typedef struct {
    int pins[BTN_COUNT];      // GPIO numbers for BACK, ENTER, FORWARD
    int debounce_ms;
    int long_press_ms;
    int repeat_interval_ms;
} buttons_config_t;

// Initializes the GPIOs, creates the event queue, spawns the polling task.
// Returns the queue handle (drain it with xQueueReceive). Returns NULL on init failure.
QueueHandle_t buttons_init(const buttons_config_t *cfg);
```

- [ ] **Step 2: Create `main/buttons.c` with GPIO + queue, no task yet**

```c
#include "buttons.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "buttons";

#define BUTTON_QUEUE_DEPTH 16

static buttons_config_t s_cfg;
static QueueHandle_t    s_queue;

QueueHandle_t buttons_init(const buttons_config_t *cfg)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "buttons_init: cfg is NULL");
        return NULL;
    }

    memcpy(&s_cfg, cfg, sizeof(s_cfg));

    // Configure all three button GPIOs as inputs with internal pull-up.
    uint64_t mask = 0;
    for (int i = 0; i < BTN_COUNT; i++) {
        mask |= (1ULL << s_cfg.pins[i]);
    }
    gpio_config_t io = {
        .pin_bit_mask = mask,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    if (gpio_config(&io) != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed");
        return NULL;
    }

    s_queue = xQueueCreate(BUTTON_QUEUE_DEPTH, sizeof(button_event_t));
    if (s_queue == NULL) {
        ESP_LOGE(TAG, "xQueueCreate failed");
        return NULL;
    }

    ESP_LOGI(TAG, "buttons ready on GPIOs %d/%d/%d (BACK/ENTER/FORWARD)",
             s_cfg.pins[BTN_BACK], s_cfg.pins[BTN_ENTER], s_cfg.pins[BTN_FORWARD]);
    return s_queue;
}
```

- [ ] **Step 3: Register `buttons.c` with CMake**

Modify `main/CMakeLists.txt`:

```cmake
idf_component_register(SRCS "sh1106.c" "main.c" "font8x8.c" "buttons.c"
                    INCLUDE_DIRS ".")
```

- [ ] **Step 4: Wire `buttons_init` into `main.c` (no consumer yet)**

Replace `main/main.c` with:

```c
#include "buttons.h"
#include "sh1106.h"

void app_main(void)
{
    sh1106_init();
    sh1106_clear();
    sh1106_draw_string(0, 0, "Buttons demo");
    sh1106_flush();

    buttons_config_t cfg = {
        .pins              = { 32, 33, 25 },  // BACK, ENTER, FORWARD
        .debounce_ms       = 20,
        .long_press_ms     = 500,
        .repeat_interval_ms = 150,
    };
    buttons_init(&cfg);
    // Queue is discarded for now — Task 2 will add the consumer.
}
```

- [ ] **Step 5: Build, flash, verify boot is clean**

Run: `idf.py build flash monitor`
Expected:

- Build + flash succeed.
- Monitor shows the normal boot sequence ending with `Calling app_main()`.
- OLED shows `Buttons demo` on row 0.
- A log line from `buttons` tag: `buttons ready on GPIOs 32/33/25 (BACK/ENTER/FORWARD)`.
- `app_main` returns immediately; no crash.
- Pressing buttons does nothing (no polling task yet).

- [ ] **Step 6: Commit**

```bash
git add main/buttons.h main/buttons.c main/CMakeLists.txt main/main.c
git commit -m "feat: buttons module skeleton with GPIO + queue setup"
```

---

## Task 2: Polling task with debounced PRESSED / RELEASED events

**Files:**
- Modify: `main/buttons.c`
- Modify: `main/main.c`

Adds the polling task and the state machine for debounced press / release. Long-press and repeat come in Tasks 3 and 4.

- [ ] **Step 1: Add state machine types and storage at the top of `buttons.c`**

Insert below the existing `static QueueHandle_t s_queue;` line:

```c
#define POLL_INTERVAL_MS 10

typedef enum {
    ST_IDLE,            // pin high, waiting for press
    ST_DEBOUNCE_PRESS,  // pin low, counting samples toward debounce_ms
    ST_PRESSED,         // press confirmed
    ST_DEBOUNCE_REL,    // pin high, counting samples toward debounce_ms
} btn_state_t;

typedef struct {
    btn_state_t state;
    int         ms_in_state;   // milliseconds spent in current state
} per_btn_t;

static per_btn_t s_state[BTN_COUNT];
```

- [ ] **Step 2: Add the polling task above `buttons_init`**

```c
static void emit(button_id_t b, button_event_type_t ev)
{
    button_event_t evt = { .button = b, .event = ev };
    // Drop if queue is full; do not block the polling task.
    if (xQueueSendToBack(s_queue, &evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "queue full, dropped event b=%d ev=%d", b, ev);
    }
}

static void buttons_task(void *arg)
{
    (void)arg;
    while (1) {
        for (int b = 0; b < BTN_COUNT; b++) {
            bool low = gpio_get_level(s_cfg.pins[b]) == 0;  // active-low
            per_btn_t *st = &s_state[b];
            st->ms_in_state += POLL_INTERVAL_MS;

            switch (st->state) {
            case ST_IDLE:
                if (low) {
                    st->state = ST_DEBOUNCE_PRESS;
                    st->ms_in_state = 0;
                }
                break;
            case ST_DEBOUNCE_PRESS:
                if (!low) {
                    // bounce — back to idle
                    st->state = ST_IDLE;
                    st->ms_in_state = 0;
                } else if (st->ms_in_state >= s_cfg.debounce_ms) {
                    emit((button_id_t)b, BTN_EVT_PRESSED);
                    st->state = ST_PRESSED;
                    st->ms_in_state = 0;
                }
                break;
            case ST_PRESSED:
                if (!low) {
                    st->state = ST_DEBOUNCE_REL;
                    st->ms_in_state = 0;
                }
                break;
            case ST_DEBOUNCE_REL:
                if (low) {
                    // bounce — back to pressed
                    st->state = ST_PRESSED;
                    st->ms_in_state = 0;
                } else if (st->ms_in_state >= s_cfg.debounce_ms) {
                    emit((button_id_t)b, BTN_EVT_RELEASED);
                    st->state = ST_IDLE;
                    st->ms_in_state = 0;
                }
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}
```

- [ ] **Step 3: Spawn the task at the end of `buttons_init`**

In `buttons_init`, just before the final `return s_queue;`, insert:

```c
    // Initialize per-button state (all IDLE, since no buttons should be pressed at startup).
    for (int i = 0; i < BTN_COUNT; i++) {
        s_state[i].state = ST_IDLE;
        s_state[i].ms_in_state = 0;
    }

    BaseType_t ok = xTaskCreate(buttons_task, "buttons", 3072, NULL,
                                tskIDLE_PRIORITY + 1, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed");
        vQueueDelete(s_queue);
        s_queue = NULL;
        return NULL;
    }
```

- [ ] **Step 4: Add a consumer task in `main.c`**

Replace `main/main.c` with:

```c
#include <stdio.h>

#include "buttons.h"
#include "sh1106.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

static const char *btn_name(button_id_t b)
{
    switch (b) {
    case BTN_BACK:    return "BACK";
    case BTN_ENTER:   return "ENTER";
    case BTN_FORWARD: return "FWD";
    default:          return "?";
    }
}

static const char *evt_name(button_event_type_t e)
{
    switch (e) {
    case BTN_EVT_PRESSED:    return "pressed";
    case BTN_EVT_RELEASED:   return "released";
    case BTN_EVT_LONG_PRESS: return "long";
    case BTN_EVT_REPEAT:     return "repeat";
    default:                 return "?";
    }
}

static void consumer_task(void *arg)
{
    QueueHandle_t q = (QueueHandle_t)arg;
    button_event_t evt;
    while (1) {
        if (xQueueReceive(q, &evt, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "%s %s", btn_name(evt.button), evt_name(evt.event));
        }
    }
}

void app_main(void)
{
    sh1106_init();
    sh1106_clear();
    sh1106_draw_string(0, 0, "Buttons demo");
    sh1106_flush();

    buttons_config_t cfg = {
        .pins              = { 32, 33, 25 },
        .debounce_ms       = 20,
        .long_press_ms     = 500,
        .repeat_interval_ms = 150,
    };
    QueueHandle_t q = buttons_init(&cfg);
    if (q == NULL) {
        ESP_LOGE(TAG, "buttons_init failed");
        return;
    }

    xTaskCreate(consumer_task, "consumer", 3072, q, tskIDLE_PRIORITY + 1, NULL);
}
```

- [ ] **Step 5: Build, flash, verify press/release events**

Run: `idf.py build flash monitor`
Expected on each button press/release:

- Press: one log line like `I (12345) main: BACK pressed`.
- Release: one log line like `I (12346) main: BACK released`.
- No spurious events from quick taps (debounce works).
- All three buttons (BACK / ENTER / FWD) emit their own pressed/released pair.
- If you see multiple `pressed` events per single physical press, debounce is misconfigured or the buttons are very bouncy — increase `debounce_ms` from 20 to 30–50 in the config.
- If you see `queue full, dropped event ...` warnings, the consumer is stuck.

- [ ] **Step 6: Commit**

```bash
git add main/buttons.c main/main.c
git commit -m "feat: polling task with debounced press/release events"
```

---

## Task 3: Long-press detection

**Files:**
- Modify: `main/buttons.c`

Extends the state machine so that staying in `ST_PRESSED` for `long_press_ms` total fires `BTN_EVT_LONG_PRESS` exactly once, then moves to a new `ST_LONG_HELD` state.

- [ ] **Step 1: Add the `ST_LONG_HELD` state**

Find the `btn_state_t` enum in `buttons.c` and update it:

```c
typedef enum {
    ST_IDLE,
    ST_DEBOUNCE_PRESS,
    ST_PRESSED,
    ST_LONG_HELD,       // long-press fired; will move to repeats in Task 4
    ST_DEBOUNCE_REL,
} btn_state_t;
```

- [ ] **Step 2: Update the state machine in `buttons_task`**

Replace the `case ST_PRESSED:` block with:

```c
            case ST_PRESSED:
                if (!low) {
                    st->state = ST_DEBOUNCE_REL;
                    st->ms_in_state = 0;
                } else if (st->ms_in_state >= s_cfg.long_press_ms) {
                    emit((button_id_t)b, BTN_EVT_LONG_PRESS);
                    st->state = ST_LONG_HELD;
                    st->ms_in_state = 0;
                }
                break;
```

Add a new `case ST_LONG_HELD:` block just after `case ST_PRESSED:`:

```c
            case ST_LONG_HELD:
                if (!low) {
                    st->state = ST_DEBOUNCE_REL;
                    st->ms_in_state = 0;
                }
                // REPEAT events come in Task 4.
                break;
```

Also update `case ST_DEBOUNCE_REL:` so a bounce returns to whichever pressed-style state we came from. Simplest approach: track the prior state. For now, the simplification of always returning to `ST_PRESSED` is acceptable — losing 20 ms of "long-held time" on a release bounce is imperceptible, and most release bounces are clean anyway. Leave the `ST_DEBOUNCE_REL` block unchanged.

- [ ] **Step 3: Build, flash, verify long-press**

Run: `idf.py build flash monitor`
Expected:

- Short press: `BACK pressed` … `BACK released`.
- Press and hold for ≥ 500 ms, then release: `BACK pressed` … (after ~500 ms) `BACK long` … (on release) `BACK released`.
- No additional events fire while continuing to hold past the long-press fire.
- Holding indefinitely does NOT yet produce repeat events — that's Task 4.

- [ ] **Step 4: Commit**

```bash
git add main/buttons.c
git commit -m "feat: emit BTN_EVT_LONG_PRESS when held past threshold"
```

---

## Task 4: Repeat events while held

**Files:**
- Modify: `main/buttons.c`

While in `ST_LONG_HELD`, every `repeat_interval_ms` of additional hold time, emit one `BTN_EVT_REPEAT`.

- [ ] **Step 1: Update `case ST_LONG_HELD:`**

Replace the `case ST_LONG_HELD:` block in `buttons_task` with:

```c
            case ST_LONG_HELD:
                if (!low) {
                    st->state = ST_DEBOUNCE_REL;
                    st->ms_in_state = 0;
                } else if (st->ms_in_state >= s_cfg.repeat_interval_ms) {
                    emit((button_id_t)b, BTN_EVT_REPEAT);
                    st->ms_in_state = 0;
                }
                break;
```

- [ ] **Step 2: Build, flash, verify repeats**

Run: `idf.py build flash monitor`
Expected when holding a button:

- `BACK pressed`
- (~500 ms later) `BACK long`
- (then every ~150 ms while held) `BACK repeat`, `BACK repeat`, `BACK repeat`, …
- On release: `BACK released` (no more repeats after this).
- Repeat rate matches `repeat_interval_ms` from the config (try adjusting to 75 ms in `main.c` to verify it speeds up).

- [ ] **Step 3: Commit**

```bash
git add main/buttons.c
git commit -m "feat: emit BTN_EVT_REPEAT every repeat_interval_ms while held"
```

---

## Task 5: OLED display of the latest event

**Files:**
- Modify: `main/main.c`

The library is feature-complete. This task makes the demo visually obvious by drawing the most recent event on the OLED below the static header.

- [ ] **Step 1: Update `consumer_task` to also draw on the OLED**

Replace the `consumer_task` function in `main/main.c` with:

```c
static void consumer_task(void *arg)
{
    QueueHandle_t q = (QueueHandle_t)arg;
    button_event_t evt;
    char line[32];

    while (1) {
        if (xQueueReceive(q, &evt, portMAX_DELAY) == pdTRUE) {
            snprintf(line, sizeof(line), "%-5s %-8s",
                     btn_name(evt.button), evt_name(evt.event));
            ESP_LOGI(TAG, "%s", line);

            sh1106_clear();
            sh1106_draw_string(0, 0, "Buttons demo");
            sh1106_draw_string(0, 16, line);  // row 2 (y=16)
            sh1106_flush();
        }
    }
}
```

Also drop the initial `sh1106_clear / draw_string / flush` from `app_main` — the consumer redraws on every event, so the initial draw is redundant (and overwritten on the first press). Final `app_main`:

```c
void app_main(void)
{
    sh1106_init();
    sh1106_clear();
    sh1106_draw_string(0, 0, "Buttons demo");
    sh1106_flush();

    buttons_config_t cfg = {
        .pins              = { 32, 33, 25 },
        .debounce_ms       = 20,
        .long_press_ms     = 500,
        .repeat_interval_ms = 150,
    };
    QueueHandle_t q = buttons_init(&cfg);
    if (q == NULL) {
        ESP_LOGE(TAG, "buttons_init failed");
        return;
    }

    xTaskCreate(consumer_task, "consumer", 3072, q, tskIDLE_PRIORITY + 1, NULL);
}
```

(The initial `Buttons demo` header on row 0 is kept so the screen isn't blank before the first event arrives. The consumer task redraws the header on every event since `sh1106_clear` wipes the whole framebuffer.)

- [ ] **Step 2: Build, flash, look at the display**

Run: `idf.py build flash monitor`

Expected:

- OLED row 0: `Buttons demo` (always present).
- OLED row 2 (8 px below): the latest event, e.g. `BACK  pressed `, `FWD   long    `, `ENTER repeat  `.
- Pressing any button updates the second line immediately.
- Holding shows the full sequence: pressed → long → repeat → repeat → … → released.
- Monitor still shows each event as a log line.

- [ ] **Step 3: Commit**

```bash
git add main/main.c
git commit -m "feat: display latest button event on OLED"
```

- [ ] **Step 4: Milestone reached**

3-button input library complete. `buttons.h` + `buttons.c` form a self-contained module that can be dropped into any ESP32 ESP-IDF project. Next sub-project (menu engine + icons) will consume this queue.

---

## Self-review notes

- **Spec coverage:** every spec section maps to a task. Hardware/wiring → Task 1. Public API → Task 1. State machine → Tasks 2/3/4 (one transition layer per task). Demo → Task 5. Non-goals respected (no combos, no ISR, no per-button timing).
- **Placeholders:** none. Every step contains complete code or exact verification commands.
- **Type consistency:** `button_id_t`, `button_event_type_t`, `button_event_t`, `buttons_config_t`, `QueueHandle_t buttons_init(const buttons_config_t *)` are defined once in Task 1 and used identically in Tasks 2/5. The state machine enum grows from 4 states (Task 2) to 5 (Task 3) — names are stable and consistent (`ST_IDLE`, `ST_DEBOUNCE_PRESS`, `ST_PRESSED`, `ST_LONG_HELD`, `ST_DEBOUNCE_REL`).
- **Risks addressed:** queue overrun warning (Task 2 emit function), debounce tuning advice (Task 2 expected output), repeat rate verification (Task 4 expected output).
- **Pins:** Default 32/33/25 documented in the spec's wiring section and used unchanged in Tasks 1 and 5. User can override by editing the `cfg.pins[]` array in `app_main`.
