#pragma once

#include <stdint.h>

// 32×32 monochrome icons, page-major: 4 pages × 32 bytes = 128 bytes per icon.
// Byte index = page * 32 + col. LSB of each byte = topmost pixel of that page.
extern const uint8_t icon_test[128];
