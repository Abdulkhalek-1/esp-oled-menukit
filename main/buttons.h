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
