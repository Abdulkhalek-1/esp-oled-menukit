#include "sh1106_host.h"

#include "font8x8.h"

#include <stdint.h>
#include <string.h>

#define SH1106_PAGES   (SH1106_HEIGHT / 8)
#define SH1106_FB_SIZE (SH1106_WIDTH * SH1106_PAGES)

static uint8_t framebuffer[SH1106_FB_SIZE];
static uint8_t snapshot[SH1106_FB_SIZE];
static int     capture_pending;
static int     locked;

esp_err_t      sh1106_init(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
    memset(snapshot, 0, sizeof(snapshot));
    capture_pending = 0;
    locked          = 0;
    return ESP_OK;
}

void sh1106_clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
}

void sh1106_set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= SH1106_WIDTH) return;
    if (y < 0 || y >= SH1106_HEIGHT) return;

    int page = y / 8;
    int bit  = y % 8;
    int idx  = page * SH1106_WIDTH + x;

    if (on)
        framebuffer[idx] |= (1 << bit);
    else
        framebuffer[idx] &= ~(1 << bit);
}

static void draw_char(int x, int y, char c)
{
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc > 127) uc = '?';
    const uint8_t *glyph = font8x8[uc - 32];

    for (int col = 0; col < 8; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 8; row++) {
            if (bits & (1 << row)) {
                sh1106_set_pixel(x + col, y + row, true);
            }
        }
    }
}

void sh1106_draw_string(int x, int y, const char *s)
{
    while (*s) {
        draw_char(x, y, *s++);
        x += 8;
    }
}

void sh1106_flush(void)
{
    if (locked) return;
    memcpy(snapshot, framebuffer, sizeof(snapshot));
    if (capture_pending) {
        locked          = 1;
        capture_pending = 0;
    }
}

const uint8_t *sh1106_host_snapshot(void)
{
    return snapshot;
}

void sh1106_host_capture_next_flush(void)
{
    capture_pending = 1;
}

void sh1106_host_release(void)
{
    capture_pending = 0;
    locked          = 0;
}
