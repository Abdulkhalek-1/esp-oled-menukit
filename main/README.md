# main (demo)

This is the demo application that ties the four components together. It is not part of any reusable library — it shows how a project consumes them.

## What it does

Boot → home screen with three icons (Settings / WiFi / Bluetooth) → 3-button navigation into nested submenus → action callbacks fire short toast notifications.

The full menu tree:

```text
Home (icons)
├── Settings ─→ list (border selection)
│   ├── Brightness  → toast
│   ├── Contrast    → toast
│   ├── About ──→ list (arrow selection)
│   │   ├── Version  → toast
│   │   └── Uptime   → toast
│   └── Reset       → toast
├── WiFi ────→ list (invert selection)
│   ├── Scan
│   ├── Connect
│   └── Status
└── Bluetooth → list (invert selection)
    ├── Pair
    └── Devices
```

This exercises both layouts, all three selection styles, three nesting levels, and multiple action callbacks.

## Files

| File          | Purpose                                                                |
|---------------|------------------------------------------------------------------------|
| `main.c`      | Wiring + static menu tree + action callbacks + entry point.            |
| `icons.h`     | Declarations for the three 32×32 demo icons.                           |
| `icons.c`     | Bitmap data for `icon_settings`, `icon_wifi`, `icon_bluetooth`.        |

The icons are generated procedurally by [tools/gen_icons.py](../tools/gen_icons.py). To regenerate (or design replacements), edit that script and run it to produce new bytes for `icons.c`.

## Pin configuration

OLED and button GPIOs are defined in `main.c`. Adjust those values to match your wiring.

## How to extend

Replace the menu tree in `main.c` with your own — the engine is data-driven, so no engine changes are needed. See [../components/menu/README.md](../components/menu/README.md) for the data model.
