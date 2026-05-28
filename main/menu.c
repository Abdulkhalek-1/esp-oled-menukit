#include "menu.h"

#include "esp_log.h"

static const char *TAG = "menu";

void menu_init(const menu_t *root, void *user_ctx)
{
    (void)root; (void)user_ctx;
    ESP_LOGI(TAG, "menu_init (stub)");
}

void menu_handle_event(button_event_t evt) { (void)evt; }
void menu_run_task(QueueHandle_t q, UBaseType_t priority) { (void)q; (void)priority; }
void menu_redraw(void) {}
void menu_toast(const char *msg, int duration_ms) { (void)msg; (void)duration_ms; }
