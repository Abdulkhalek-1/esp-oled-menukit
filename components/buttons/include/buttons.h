#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @file buttons.h
 * @brief Debounced 3-button input library with timed event types.
 *
 * Spawns one polling task that scans the 3 button GPIOs every 10 ms,
 * applies a software debounce, and emits typed events onto a FreeRTOS queue.
 */

/**
 * @brief Logical button identifiers.
 */
typedef enum {
    BTN_BACK    = 0, /**< Back / cancel button. */
    BTN_ENTER   = 1, /**< Enter / confirm button. */
    BTN_FORWARD = 2, /**< Forward / next button. */
    BTN_COUNT   = 3, /**< Number of buttons; used for array sizing. */
} button_id_t;

/**
 * @brief Event types emitted by the library.
 */
typedef enum {
    BTN_EVT_PRESSED,    /**< Debounced press edge. */
    BTN_EVT_RELEASED,   /**< Debounced release edge. */
    BTN_EVT_LONG_PRESS, /**< Held for >= long_press_ms; fires once. */
    BTN_EVT_REPEAT,     /**< Held past long-press; fires every repeat_interval_ms. */
} button_event_type_t;

/**
 * @brief One event on the queue.
 */
typedef struct {
    button_id_t         button; /**< Which button generated the event. */
    button_event_type_t event;  /**< Event type. */
} button_event_t;

/**
 * @brief Configuration for buttons_init().
 *
 * Pin assignments and timing thresholds. All three buttons share the same timings.
 */
typedef struct {
    int pins[BTN_COUNT];     /**< GPIO numbers for BACK, ENTER, FORWARD respectively. */
    int debounce_ms;         /**< Debounce window, typically 10-50 ms. */
    int long_press_ms;       /**< Hold time before LONG_PRESS fires. */
    int repeat_interval_ms;  /**< Interval between REPEAT events after LONG_PRESS. */
} buttons_config_t;

/**
 * @brief Initialize the library: configure GPIOs, create the queue, spawn the polling task.
 *
 * The polling task runs at priority `tskIDLE_PRIORITY + 1` with a 3 KB stack and
 * a 10 ms period. The returned queue accepts up to 16 events before dropping new ones.
 *
 * @param cfg Configuration. Must not be NULL. Contents are copied internally.
 * @return The event queue handle on success, NULL on failure (with an ESP_LOGE explaining why).
 */
QueueHandle_t buttons_init(const buttons_config_t *cfg);
