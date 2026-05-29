#pragma once

#include <stdint.h>

#include "sh1106.h"

/**
 * @file sh1106_host.h
 * @brief Host-side extensions to the sh1106 driver used by the render harness.
 *
 * The host build replaces sh1106's SPI half with no-ops and captures every
 * sh1106_flush() into a separate `snapshot` buffer. These accessors let the
 * render driver dump the snapshot and freeze it for the toast scene.
 */

/** Return the last "displayed" frame (the last unlocked sh1106_flush). */
const uint8_t *sh1106_host_snapshot(void);

/** Arm a one-shot capture lock: the next sh1106_flush snapshots, then locks. */
void sh1106_host_capture_next_flush(void);

/** Clear any pending capture lock so subsequent flushes update the snapshot. */
void sh1106_host_release(void);
