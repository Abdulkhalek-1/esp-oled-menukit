#include "buttons.h"

#include <stdbool.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "buttons";

#define BUTTON_QUEUE_DEPTH 16

static buttons_config_t s_cfg;
static QueueHandle_t    s_queue;

#define POLL_INTERVAL_MS 10

typedef enum {
    ST_IDLE,            // pin high, waiting for press
    ST_DEBOUNCE_PRESS,  // pin low, counting samples toward debounce_ms
    ST_PRESSED,         // press confirmed
    ST_LONG_HELD,       // long-press fired; repeats will come in Task 4
    ST_DEBOUNCE_REL,    // pin high, counting samples toward debounce_ms
} btn_state_t;

typedef struct {
    btn_state_t state;
    int         ms_in_state;   // milliseconds spent in current state
} per_btn_t;

static per_btn_t s_state[BTN_COUNT];

static void emit(button_id_t b, button_event_type_t ev)
{
    button_event_t evt = { .button = b, .event = ev };
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
                } else if (st->ms_in_state >= s_cfg.long_press_ms) {
                    emit((button_id_t)b, BTN_EVT_LONG_PRESS);
                    st->state = ST_LONG_HELD;
                    st->ms_in_state = 0;
                }
                break;
            case ST_LONG_HELD:
                if (!low) {
                    st->state = ST_DEBOUNCE_REL;
                    st->ms_in_state = 0;
                }
                // REPEAT events come in Task 4.
                break;
            case ST_DEBOUNCE_REL:
                if (low) {
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

    ESP_LOGI(TAG, "buttons ready on GPIOs %d/%d/%d (BACK/ENTER/FORWARD)",
             s_cfg.pins[BTN_BACK], s_cfg.pins[BTN_ENTER], s_cfg.pins[BTN_FORWARD]);
    return s_queue;
}
