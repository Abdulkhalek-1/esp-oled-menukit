#include "sh1106.h"

#include <string.h>

// Wiring — ESP32 DevKitC, HSPI bus.
#define PIN_SCK   18
#define PIN_MOSI  23
#define PIN_CS     5
#define PIN_DC    17
#define PIN_RES   16

#define SH1106_PAGES         (SH1106_HEIGHT / 8)   // 8 pages of 8 rows each.
#define SH1106_FB_SIZE       (SH1106_WIDTH * SH1106_PAGES)
#define SH1106_COL_OFFSET    2  // SH1106 has 132 columns; visible area starts at 2.

static uint8_t framebuffer[SH1106_FB_SIZE];

void sh1106_init(void)  {}
void sh1106_clear(void) { memset(framebuffer, 0, sizeof(framebuffer)); }
void sh1106_set_pixel(int x, int y, bool on) { (void)x; (void)y; (void)on; }
void sh1106_flush(void) {}
