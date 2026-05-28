#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "buttons.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/**
 * @file menu.h
 * @brief Recursive data-driven menu engine for the SH1106 OLED.
 *
 * Renders static menu trees on the display, dispatches button events for navigation,
 * and invokes user-provided action callbacks. Two layouts (text list, icon row) and
 * three selection styles (invert, arrow, border) are supported, all configurable per-menu.
 */

/**
 * @brief Discriminator for menu_item_t::u.
 *
 * MENU_ITEM_END must be 0 so zero-initialized arrays end in a sentinel.
 */
typedef enum {
    MENU_ITEM_END = 0,    /**< Array sentinel. */
    MENU_ITEM_ACTION,     /**< Leaf item; invokes a callback on ENTER. */
    MENU_ITEM_SUBMENU,    /**< Branch item; ENTER descends into another menu. */
} menu_item_kind_t;

typedef struct menu_s menu_t;

/**
 * @brief One menu item (label, optional icon, and either an action or a submenu).
 */
typedef struct {
    menu_item_kind_t kind;   /**< Discriminator for u. */
    const char      *label;  /**< Display text. Required. */
    const uint8_t   *icon;   /**< Optional 32x32 page-major bitmap; NULL for text-only items. */
    union {
        void (*action)(void *user_ctx);  /**< Called when kind == MENU_ITEM_ACTION. */
        const menu_t *submenu;           /**< Used when kind == MENU_ITEM_SUBMENU. */
    } u;
} menu_item_t;

/**
 * @brief Layout selector for a menu screen.
 */
typedef enum {
    MENU_LAYOUT_LIST,    /**< Vertical text list with optional title bar. */
    MENU_LAYOUT_ICONS,   /**< Horizontal icon row with the selected item's label below. */
} menu_layout_t;

/**
 * @brief How the selected item is visually distinguished.
 */
typedef enum {
    MENU_SEL_INVERT,   /**< Fill the item's row with white, draw text in inverse. */
    MENU_SEL_ARROW,    /**< Draw a '>' cursor to the left of the item. */
    MENU_SEL_BORDER,   /**< 1-pixel rectangle around the item's bounding box. */
} menu_selection_t;

/**
 * @brief Per-menu visual style overrides.
 *
 * If a menu's `style` is NULL, sensible defaults are used.
 */
typedef struct {
    int              icon_w;        /**< Icon width in pixels. Default 32. */
    int              icon_h;        /**< Icon height in pixels. Default 32. */
    int              row_height;    /**< LIST row pitch in pixels. Default 10. */
    int              title_height;  /**< Title bar height in pixels (0 disables). Default 10. */
    menu_selection_t selection;     /**< Selection rendering style. Default MENU_SEL_INVERT. */
} menu_style_t;

/**
 * @brief One menu screen: optional title + layout + items + optional style.
 */
struct menu_s {
    const char         *title;   /**< Optional title shown at top. NULL = no title bar. */
    menu_layout_t       layout;  /**< Layout selector. */
    const menu_item_t  *items;   /**< Array of items terminated by MENU_END. */
    const menu_style_t *style;   /**< Optional style override; NULL uses defaults. */
};

/** Sentinel value used to terminate menu_item_t arrays. */
#define MENU_END { .kind = MENU_ITEM_END }

/**
 * @brief Set the root menu and the user context passed to action callbacks.
 *
 * Renders the root immediately. Subsequent calls reset the navigation state.
 *
 * @param root     Pointer to the root menu. Must not be NULL.
 * @param user_ctx Opaque pointer forwarded to every action callback. May be NULL.
 */
void menu_init(const menu_t *root, void *user_ctx);

/**
 * @brief Drive the engine with one button event.
 *
 * - FORWARD (press or repeat): advance selection within the current menu (wraps).
 * - BACK    (press or repeat): pop one nav frame; no-op at the root.
 * - ENTER   (press only):      invoke the item's action OR descend into its submenu.
 *
 * Re-renders only if state changed. Not thread-safe — call from a single consumer task.
 *
 * @param evt The button event to process.
 */
void menu_handle_event(button_event_t evt);

/**
 * @brief Convenience: spawn a task that drains @p q and calls menu_handle_event.
 *
 * @param q        Button event queue (typically from buttons_init).
 * @param priority FreeRTOS task priority.
 */
void menu_run_task(QueueHandle_t q, UBaseType_t priority);

/**
 * @brief Force an immediate redraw of the current menu.
 *
 * Useful from action callbacks that mutate external state which should be reflected.
 */
void menu_redraw(void);

/**
 * @brief Show a brief inverted-bar message at the bottom of the screen, then restore.
 *
 * Blocks the calling (menu) task for @p duration_ms. Button events arriving during the
 * toast remain queued and are processed afterward.
 *
 * @param msg         Null-terminated string to display.
 * @param duration_ms How long to show the toast, in milliseconds.
 */
void menu_toast(const char *msg, int duration_ms);
