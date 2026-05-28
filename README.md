# esp-oled-menukit

OLED-driven menu system for ESP32 with a 1.3" SH1106 SPI display and three push buttons.
Built as a set of independent ESP-IDF components, plus a working demo.

## Components

| Component                                            | Purpose                                                                                            | Public API summary                                                |
|------------------------------------------------------|----------------------------------------------------------------------------------------------------|-------------------------------------------------------------------|
| [sh1106](components/sh1106/README.md)                | Hand-rolled SH1106 SPI driver, framebuffer, pixel + 8x8 text drawing.                              | `sh1106_init / clear / set_pixel / draw_string / flush`           |
| [font8x8](components/font8x8/README.md)              | 8x8 ASCII bitmap font (BSD-2-Clause from u8g2).                                                    | `font8x8[96][8]`                                                  |
| [buttons](components/buttons/README.md)              | Debounced 3-button library with PRESSED/RELEASED/LONG_PRESS/REPEAT events on a FreeRTOS queue.     | `buttons_init`                                                    |
| [menu](components/menu/README.md)                    | Recursive data-driven menu engine — two layouts, three selection styles, scrolling, toasts.        | `menu_init / handle_event / run_task / toast / redraw`            |

## Hardware

ESP32 DevKitC / WROOM-32 + 1.3" SH1106 SPI OLED + three momentary push buttons.

OLED (7-pin SPI module):

| OLED pin | ESP32 GPIO |
|----------|------------|
| GND      | GND        |
| VCC      | 3V3        |
| D0 (SCK) | 15         |
| D1 (MOSI)| 2          |
| RES      | 4          |
| DC       | 18         |
| CS       | 5          |

Buttons (one side → GPIO, other side → GND; internal pull-ups enabled):

| Button   | ESP32 GPIO |
|----------|------------|
| BACK     | 25         |
| ENTER    | 33         |
| FORWARD  | 32         |

Both pin sets are software-configurable — see `main/main.c` and each component's README.

## Build and flash the demo

Inside an ESP-IDF v5.5+ environment (devcontainer in `.devcontainer/` works):

```bash
idf.py build flash monitor
```

You should see a `Home` title with three icons (gear, wifi arcs, bluetooth), borders around the selected one, and full navigation via the buttons.

## Using a component in another project

Two equivalent ways:

1. **EXTRA_COMPONENT_DIRS.** In your other project's root `CMakeLists.txt`, before `project(...)`, add:

   ```cmake
   set(EXTRA_COMPONENT_DIRS "/path/to/esp-oled-menukit/components")
   ```

   Then list the components you want in your `main/CMakeLists.txt`:

   ```cmake
   idf_component_register(SRCS "main.c" REQUIRES menu sh1106 buttons font8x8)
   ```

2. **Copy.** Copy the component folders into your project's own `components/` tree:

   ```bash
   cp -r esp-oled-menukit/components/{font8x8,sh1106,buttons,menu} my-project/components/
   ```

Each component is fully self-contained — its `CMakeLists.txt` declares its own dependencies via `REQUIRES` / `PRIV_REQUIRES`.

## Project layout

See [docs/](docs/) for the original design specs and implementation plans that produced this code.

```text
.
├── components/{sh1106,font8x8,buttons,menu}/    # the reusable libraries
├── main/                                        # the demo
├── docs/                                        # design specs and plans
└── tools/gen_icons.py                           # procedural icon generator
```

## Status

Personal learning project. The font in `components/font8x8` is vendored from the u8g2 project under BSD-2-Clause — see [components/font8x8/README.md](components/font8x8/README.md) for attribution.
