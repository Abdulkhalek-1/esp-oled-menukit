# buttons

Debounced 3-button input library that polls GPIO every 10 ms and emits typed events (PRESSED / RELEASED / LONG_PRESS / REPEAT) on a FreeRTOS queue.

## Public API

| Symbol                                                       | Description                                                                              |
|--------------------------------------------------------------|------------------------------------------------------------------------------------------|
| `enum button_id_t { BTN_BACK, BTN_ENTER, BTN_FORWARD, BTN_COUNT }` | Logical button names.                                                              |
| `enum button_event_type_t { BTN_EVT_PRESSED, BTN_EVT_RELEASED, BTN_EVT_LONG_PRESS, BTN_EVT_REPEAT }` | Event types.                                                          |
| `struct button_event_t { button_id_t button; button_event_type_t event; }` | One event on the queue.                                                |
| `struct buttons_config_t { int pins[3]; int debounce_ms; int long_press_ms; int repeat_interval_ms; }` | Pins + timing.                                                |
| `QueueHandle_t buttons_init(const buttons_config_t *cfg)`    | Set up GPIOs, create the queue, spawn the polling task. Returns NULL on failure.        |

## Dependencies

- `freertos` — public (`buttons_init` returns `QueueHandle_t`).
- `driver` — private (`gpio.h`).
- `esp_common` — private (logging).

## Usage

```c
#include "buttons.h"

buttons_config_t cfg = {
    .pins              = { 25, 33, 32 },  // BACK, ENTER, FORWARD
    .debounce_ms       = 20,
    .long_press_ms     = 500,
    .repeat_interval_ms = 150,
};
QueueHandle_t q = buttons_init(&cfg);
if (q == NULL) { /* handle */ }

// Consumer task:
button_event_t evt;
while (xQueueReceive(q, &evt, portMAX_DELAY) == pdTRUE) {
    // dispatch on (evt.button, evt.event)
}
```

## Notes

- Buttons are active-low; internal pull-ups are enabled by `buttons_init`. External 10k pull-ups are not required but don't hurt for long or noisy wiring.
- The polling task runs at `tskIDLE_PRIORITY + 1` with a 3 KB stack.
- If the consumer task stalls, queue fills after ~16 events (~160 ms of buttons) and further events are dropped — logged as `ESP_LOGW`.
- All three buttons share the same `debounce_ms` / `long_press_ms` / `repeat_interval_ms`; per-button overrides are not supported in this version.
