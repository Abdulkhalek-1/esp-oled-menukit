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
