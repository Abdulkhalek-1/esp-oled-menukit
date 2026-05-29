# host_render — OLED screenshot harness

Compiles the firmware's real `menu.c` + `font8x8.c` against a SPI-less
sh1106 framebuffer to produce pixel-accurate PNG screenshots of canonical
menu states, used in the project root `README.md`.

## Regenerate the screenshots

From this directory:

```bash
make
```

That builds the harness, runs it, writes five PGMs to `../../docs/img/`,
then upscales them 4x into PNGs (and deletes the intermediate PGMs). The
PNGs are checked into git so GitHub renders them in the root README.

Re-run any time menu rendering, fonts, or icons change — the output will
update automatically because the harness uses the real component sources.

## Requirements

`cmake` >= 3.16, a C11 compiler (`gcc`/`clang`), GNU make, `python3`. The
Python step uses Pillow if it's installed and falls back to a stdlib-only
PNG encoder otherwise.

## How it works

See [docs/2026-05-28-screen-renders-design.md](../../docs/2026-05-28-screen-renders-design.md)
for the architecture: ESP-IDF header stubs, a host replacement for the SPI
half of `sh1106`, and a snapshot lock that captures the toast frame before
the menu repaints over it.
