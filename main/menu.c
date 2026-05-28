#include "menu.h"

#include <string.h>

#include "esp_log.h"
#include "sh1106.h"

static const char *TAG = "menu";

#define MENU_MAX_DEPTH 8

typedef struct {
    const menu_t *menu;
    int           index;
    int           scroll_offset;
} frame_t;

static frame_t  s_stack[MENU_MAX_DEPTH];
static int      s_depth;
static void    *s_user_ctx;

static const menu_style_t s_default_style = {
    .icon_w        = 32,
    .icon_h        = 32,
    .row_height    = 10,
    .title_height  = 10,
    .selection     = MENU_SEL_INVERT,
};

static const menu_style_t *style_of(const menu_t *m)
{
    return m->style ? m->style : &s_default_style;
}

static int item_count(const menu_item_t *items)
{
    int n = 0;
    while (items[n].kind != MENU_ITEM_END) n++;
    return n;
}

static void invert_rect(int x, int y, int w, int h)
{
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            sh1106_set_pixel(xx, yy, true);
        }
    }
}

static void draw_string_inverse(int x, int y, const char *s)
{
    extern const uint8_t font8x8[96][8];
    while (*s) {
        unsigned char uc = (unsigned char)(*s++);
        if (uc < 32 || uc > 127) uc = '?';
        const uint8_t *glyph = font8x8[uc - 32];
        for (int col = 0; col < 8; col++) {
            uint8_t bits = glyph[col];
            for (int row = 0; row < 8; row++) {
                if (bits & (1 << row)) {
                    sh1106_set_pixel(x + col, y + row, false);
                }
            }
        }
        x += 8;
    }
}

static int draw_title(const menu_t *m)
{
    const menu_style_t *st = style_of(m);
    if (m->title == NULL || st->title_height <= 0) return 0;

    sh1106_draw_string(2, 1, m->title);
    for (int x = 0; x < SH1106_WIDTH; x++) {
        sh1106_set_pixel(x, st->title_height - 1, true);
    }
    return st->title_height;
}

static void render_list(const menu_t *m, const frame_t *f)
{
    const menu_style_t *st = style_of(m);
    int n = item_count(m->items);
    int y = draw_title(m);
    for (int i = 0; i < n; i++) {
        bool selected = (i == f->index);
        if (selected && st->selection == MENU_SEL_INVERT) {
            invert_rect(0, y, SH1106_WIDTH, st->row_height);
            draw_string_inverse(2, y + 1, m->items[i].label);
        } else {
            sh1106_draw_string(2, y + 1, m->items[i].label);
        }
        y += st->row_height;
    }
}

static void render(void)
{
    if (s_depth == 0) return;
    sh1106_clear();
    const frame_t *f = &s_stack[s_depth - 1];
    if (f->menu->layout == MENU_LAYOUT_LIST) {
        render_list(f->menu, f);
    }
    sh1106_flush();
}

void menu_init(const menu_t *root, void *user_ctx)
{
    s_user_ctx = user_ctx;
    s_depth = 1;
    s_stack[0].menu          = root;
    s_stack[0].index         = 0;
    s_stack[0].scroll_offset = 0;
    ESP_LOGI(TAG, "menu_init: root=%p", root);
    render();
}

void menu_handle_event(button_event_t evt)
{
    if (s_depth == 0) return;
    frame_t *f = &s_stack[s_depth - 1];
    int n = item_count(f->menu->items);
    if (n == 0) return;

    bool changed = false;
    bool is_press_or_repeat =
        (evt.event == BTN_EVT_PRESSED) || (evt.event == BTN_EVT_REPEAT);

    if (!is_press_or_repeat) return;

    switch (evt.button) {
    case BTN_FORWARD:
        f->index = (f->index + 1) % n;
        changed = true;
        break;
    case BTN_BACK:
        if (s_depth > 1) {
            s_depth--;
            changed = true;
        }
        break;
    case BTN_ENTER:
        if (evt.event == BTN_EVT_REPEAT) break;
        {
            const menu_item_t *it = &f->menu->items[f->index];
            if (it->kind == MENU_ITEM_SUBMENU && s_depth < MENU_MAX_DEPTH) {
                s_stack[s_depth].menu          = it->u.submenu;
                s_stack[s_depth].index         = 0;
                s_stack[s_depth].scroll_offset = 0;
                s_depth++;
                changed = true;
            } else if (it->kind == MENU_ITEM_ACTION && it->u.action != NULL) {
                it->u.action(s_user_ctx);
                changed = true;
            }
        }
        break;
    default:
        break;
    }

    if (changed) render();
}

static void menu_run_task_fn(void *arg)
{
    QueueHandle_t q = (QueueHandle_t)arg;
    button_event_t evt;
    while (1) {
        if (xQueueReceive(q, &evt, portMAX_DELAY) == pdTRUE) {
            menu_handle_event(evt);
        }
    }
}

void menu_run_task(QueueHandle_t q, UBaseType_t priority)
{
    xTaskCreate(menu_run_task_fn, "menu", 4096, (void *)q, priority, NULL);
}

void menu_redraw(void) { render(); }

void menu_toast(const char *msg, int duration_ms)
{
    invert_rect(0, SH1106_HEIGHT - 10, SH1106_WIDTH, 10);
    draw_string_inverse(2, SH1106_HEIGHT - 9, msg);
    sh1106_flush();
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    render();
}
