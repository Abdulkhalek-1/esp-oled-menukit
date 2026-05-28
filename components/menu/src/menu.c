#include "menu.h"

#include "esp_log.h"
#include "font8x8.h"
#include "sh1106.h"

#include <assert.h>
#include <string.h>

static const char *TAG = "menu";

#define MENU_MAX_DEPTH 8

typedef struct {
    const menu_t *menu;
    int           index;
    int           scroll_offset;
} frame_t;

static frame_t            s_stack[MENU_MAX_DEPTH];
static int                s_depth;
static void              *s_user_ctx;

static const menu_style_t s_default_style = {
    .icon_w       = 32,
    .icon_h       = 32,
    .row_height   = 10,
    .title_height = 10,
    .selection    = MENU_SEL_INVERT,
};

static const menu_style_t *style_of(const menu_t *m)
{
    return m->style ? m->style : &s_default_style;
}

static int item_count(const menu_item_t *items)
{
    int n = 0;
    while (items[n].kind != MENU_ITEM_END)
        n++;
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

static void render_icon(int x, int y, const uint8_t *icon, int w, int h)
{
    if (icon == NULL) return;
    int pages = h / 8;
    for (int page = 0; page < pages; page++) {
        for (int col = 0; col < w; col++) {
            uint8_t bits = icon[page * w + col];
            for (int row = 0; row < 8; row++) {
                if (bits & (1 << row)) {
                    sh1106_set_pixel(x + col, y + page * 8 + row, true);
                }
            }
        }
    }
}

static void draw_rect(int x, int y, int w, int h)
{
    for (int xx = x; xx < x + w; xx++) {
        sh1106_set_pixel(xx, y, true);
        sh1106_set_pixel(xx, y + h - 1, true);
    }
    for (int yy = y; yy < y + h; yy++) {
        sh1106_set_pixel(x, yy, true);
        sh1106_set_pixel(x + w - 1, yy, true);
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

static void render_list(const menu_t *m, frame_t *f)
{
    const menu_style_t *st           = style_of(m);
    int                 n            = item_count(m->items);

    int                 y_start      = draw_title(m);
    int                 visible_rows = (SH1106_HEIGHT - y_start) / st->row_height;
    if (visible_rows < 1) visible_rows = 1;

    if (f->index < f->scroll_offset) f->scroll_offset = f->index;
    if (f->index >= f->scroll_offset + visible_rows) f->scroll_offset = f->index - visible_rows + 1;

    int y = y_start;
    for (int i = f->scroll_offset; i < n && i < f->scroll_offset + visible_rows; i++) {
        bool selected = (i == f->index);
        int  text_x   = (st->selection == MENU_SEL_ARROW) ? 10 : 2;

        if (selected && st->selection == MENU_SEL_INVERT) {
            invert_rect(0, y, SH1106_WIDTH, st->row_height);
            draw_string_inverse(text_x, y + 1, m->items[i].label);
        } else {
            if (selected && st->selection == MENU_SEL_ARROW) {
                sh1106_draw_string(2, y + 1, ">");
            }
            sh1106_draw_string(text_x, y + 1, m->items[i].label);
            if (selected && st->selection == MENU_SEL_BORDER) {
                draw_rect(0, y, SH1106_WIDTH, st->row_height);
            }
        }
        y += st->row_height;
    }
}

static void render_icons(const menu_t *m, frame_t *f)
{
    const menu_style_t *st    = style_of(m);
    int                 n     = item_count(m->items);

    int                 y_top = 0;
    if (m->title) y_top = draw_title(m);

    int total_w = n * st->icon_w;
    int gap     = (SH1106_WIDTH - total_w) / (n + 1);
    if (gap < 0) gap = 0;

    int label_h = 9;
    int icons_h = SH1106_HEIGHT - y_top - label_h;
    int icon_y  = y_top + (icons_h - st->icon_h) / 2;
    if (icon_y < y_top) icon_y = y_top;

    for (int i = 0; i < n; i++) {
        int icon_x = gap + i * (st->icon_w + gap);
        render_icon(icon_x, icon_y, m->items[i].icon, st->icon_w, st->icon_h);

        if (i == f->index) {
            if (st->selection == MENU_SEL_INVERT) {
                draw_rect(icon_x - 1, icon_y - 1, st->icon_w + 2, st->icon_h + 2);
            } else if (st->selection == MENU_SEL_BORDER) {
                draw_rect(icon_x - 2, icon_y - 2, st->icon_w + 4, st->icon_h + 4);
            } else if (st->selection == MENU_SEL_ARROW) {
                int tx = icon_x + st->icon_w / 2;
                int ty = icon_y + st->icon_h + 1;
                for (int dy = 0; dy < 3; dy++) {
                    for (int dx = -dy; dx <= dy; dx++) {
                        sh1106_set_pixel(tx + dx, ty + dy, true);
                    }
                }
            }
        }
    }

    const char *label = m->items[f->index].label;
    if (label) {
        int label_x = (SH1106_WIDTH - (int)strlen(label) * 8) / 2;
        if (label_x < 0) label_x = 0;
        sh1106_draw_string(label_x, SH1106_HEIGHT - 8, label);
    }
}

static void render(void)
{
    if (s_depth == 0) return;
    sh1106_clear();
    frame_t *f = &s_stack[s_depth - 1];
    if (f->menu->layout == MENU_LAYOUT_LIST) {
        render_list(f->menu, f);
    } else if (f->menu->layout == MENU_LAYOUT_ICONS) {
        render_icons(f->menu, f);
    }
    sh1106_flush();
}

void menu_init(const menu_t *root, void *user_ctx)
{
    assert(root != NULL);
    s_user_ctx               = user_ctx;
    s_depth                  = 1;
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
    int      n = item_count(f->menu->items);
    if (n == 0) return;

    bool changed            = false;
    bool is_press_or_repeat = (evt.event == BTN_EVT_PRESSED) || (evt.event == BTN_EVT_REPEAT);

    if (!is_press_or_repeat) return;

    switch (evt.button) {
    case BTN_FORWARD:
        f->index = (f->index + 1) % n;
        changed  = true;
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
    QueueHandle_t  q = (QueueHandle_t)arg;
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

void menu_redraw(void)
{
    render();
}

void menu_toast(const char *msg, int duration_ms)
{
    invert_rect(0, SH1106_HEIGHT - 10, SH1106_WIDTH, 10);
    draw_string_inverse(2, SH1106_HEIGHT - 9, msg);
    sh1106_flush();
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    render();
}
