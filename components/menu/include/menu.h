#pragma once

#include "buttons.h"

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef enum {
    MENU_ITEM_END = 0,
    MENU_ITEM_ACTION,
    MENU_ITEM_SUBMENU,
} menu_item_kind_t;

typedef struct menu_s menu_t;

typedef struct {
    menu_item_kind_t kind;
    const char      *label;
    const uint8_t   *icon;
    union {
        void (*action)(void *user_ctx);
        const menu_t *submenu;
    } u;
} menu_item_t;

typedef enum { MENU_LAYOUT_LIST, MENU_LAYOUT_ICONS } menu_layout_t;
typedef enum { MENU_SEL_INVERT, MENU_SEL_ARROW, MENU_SEL_BORDER } menu_selection_t;

typedef struct {
    int              icon_w;
    int              icon_h;
    int              row_height;
    int              title_height;
    menu_selection_t selection;
} menu_style_t;

struct menu_s {
    const char         *title;
    menu_layout_t       layout;
    const menu_item_t  *items;
    const menu_style_t *style;
};

#define MENU_END {.kind = MENU_ITEM_END}

void menu_init(const menu_t *root, void *user_ctx);
void menu_handle_event(button_event_t evt);
void menu_run_task(QueueHandle_t q, UBaseType_t priority);
void menu_redraw(void);
void menu_toast(const char *msg, int duration_ms);
