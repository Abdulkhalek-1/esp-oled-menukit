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
