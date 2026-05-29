#pragma once

/**
 * @file scenes.h
 * @brief Canonical menu screenshots driven by the render harness.
 *
 * Each scene's `setup` function navigates the menu (via the public
 * menu_init / menu_handle_event API) to the desired state and leaves
 * the displayed image in sh1106_host's snapshot buffer.
 */

typedef struct {
    const char *name;     /**< Output filename stem (no extension). */
    void (*setup)(void);  /**< Navigates the menu to the desired state. */
} scene_t;

extern const scene_t scenes[];
extern const int     scenes_count;
