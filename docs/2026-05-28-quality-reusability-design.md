# Quality & Reusability Pass — Design

**Date:** 2026-05-28
**Goal:** Convert the monolithic `main/`-based project into a collection of reusable ESP-IDF components with consistent formatting, advisory static analysis, full Doxygen API docs in headers, and READMEs at root + per-component.

## Context

The project currently has six modules (`sh1106`, `font8x8`, `buttons`, `menu`, `icons`, plus the demo `main`) all sitting in `main/`. Library code and demo code are intermixed, headers reference each other via `main/` siblings, and there is no formatter, no linter config, and no documentation outside the design / plan docs in `docs/`.

This iteration restructures the libraries into independent ESP-IDF components, sets up `.clang-format` / `.clang-tidy` / `.editorconfig`, fixes the code smells we noticed during implementation, and writes Doxygen-style API documentation in every public header plus a README at the project root and one per component.

Explicitly **out of scope**: publishing to the ESP Component Registry (`idf_component.yml`), `LICENSE`, `CHANGELOG.md`, CI pipelines, Kconfig integration, and any new runtime functionality.

## Target file layout

```text
SH1106-learn/
├── .clang-format               (NEW)
├── .clang-tidy                 (NEW)
├── .editorconfig               (NEW)
├── .clangd                     (existing — minor update)
├── .gitignore, .devcontainer/, .vscode/   (existing, unchanged)
├── CMakeLists.txt              (root — unchanged in shape, may need EXTRA_COMPONENT_DIRS)
├── README.md                   (NEW)
├── docs/                       (existing)
├── tools/gen_icons.py          (existing)
├── components/
│   ├── sh1106/
│   │   ├── CMakeLists.txt
│   │   ├── README.md
│   │   ├── include/sh1106.h
│   │   └── src/sh1106.c
│   ├── font8x8/
│   │   ├── CMakeLists.txt
│   │   ├── README.md
│   │   ├── include/font8x8.h
│   │   └── src/font8x8.c
│   ├── buttons/
│   │   ├── CMakeLists.txt
│   │   ├── README.md
│   │   ├── include/buttons.h
│   │   └── src/buttons.c
│   └── menu/
│       ├── CMakeLists.txt
│       ├── README.md
│       ├── include/menu.h
│       └── src/menu.c
└── main/
    ├── CMakeLists.txt          (REQUIRES sh1106 font8x8 buttons menu)
    ├── main.c                  (demo)
    ├── icons.h, icons.c        (demo-specific data)
    └── README.md
```

ESP-IDF auto-discovers anything under `components/` at the project root, so no `EXTRA_COMPONENT_DIRS` is strictly required. The root `CMakeLists.txt` stays as-is.

## Component dependencies

Split into public (`REQUIRES`, things the public header references) and private (`PRIV_REQUIRES`, only used inside the `.c`). Consumers of a component automatically see its `REQUIRES` deps transitively but not its `PRIV_REQUIRES`.

| Component | `REQUIRES`                              | `PRIV_REQUIRES`                  | Why                                                                |
|-----------|-----------------------------------------|----------------------------------|--------------------------------------------------------------------|
| font8x8   | (none)                                  | (none)                           | Pure data: header declares one `extern` array, no types.           |
| sh1106    | (none)                                  | `driver`, `esp_rom`, `freertos`  | Public header is plain types only; .c uses GPIO/SPI/delay.         |
| buttons   | `freertos`                              | `driver`, `esp_common`           | Public header exposes `QueueHandle_t buttons_init(...)`.           |
| menu      | `buttons`, `freertos`                   | `sh1106`, `font8x8`              | Public header uses `button_event_t`, `QueueHandle_t`; .c renders.  |
| main      | `sh1106`, `font8x8`, `buttons`, `menu`  | (none)                           | Demo references everything directly.                               |

`icons.{h,c}` live in `main/` (demo-specific data, not a library).

## Code style (`.clang-format`)

Matches the style we've already been using (close to ESP-IDF conventions):

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 100
PointerAlignment: Right
AlignConsecutiveAssignments: AcrossEmptyLines
AlignConsecutiveDeclarations: AcrossEmptyLines
AlignTrailingComments: true
SpaceBeforeParens: ControlStatements
BreakBeforeBraces: Custom
BraceWrapping:
  AfterFunction: true
  AfterStruct: false
  AfterEnum: false
  AfterControlStatement: false
AllowShortFunctionsOnASingleLine: Inline
AllowShortIfStatementsOnASingleLine: WithoutElse
SortIncludes: CaseSensitive
IncludeBlocks: Regroup
IncludeCategories:
  - Regex: '^"[a-z0-9_]+\.h"$'                                   # local headers
    Priority: 1
  - Regex: '^<(stdio|stdint|stdbool|stddef|string|stdlib)\.h>'   # C stdlib
    Priority: 2
  - Regex: '^"(freertos|driver|esp_).+\.h"$'                     # ESP-IDF / FreeRTOS
    Priority: 3
  - Regex: '.*'
    Priority: 4
```

After the config is committed, run `clang-format -i` over every `.c` and `.h` once. Format-only diffs land in a single dedicated commit so behavior changes don't get hidden in formatting noise.

## Static analysis (`.clang-tidy`)

Advisory only — we run it and fix what's real; we don't fail the build on warnings.

Enabled check categories:

- `bugprone-*`
- `cert-*` minus `-cert-err58-cpp`, `-cert-dcl21-cpp`
- `readability-*` minus `-readability-braces-around-statements`, `-readability-magic-numbers`, `-readability-identifier-length`
- `performance-*`
- `clang-analyzer-*`

Disabled (fight ESP-IDF idioms or are pure noise):

- `-llvm-include-order` (clang-format handles)
- `-llvmlibc-*`
- `-fuchsia-*`
- `-modernize-*` (some warn about K&R / C99 patterns we keep on purpose)

Run policy: `clang-tidy components/*/src/*.c main/*.c -p build` after each significant change. Findings get fixed in the code-quality task batch. Anything intentionally left in (e.g. a `static` global that the checker doesn't like) gets a per-line `// NOLINT(check-name)` with a reason.

## `.editorconfig`

```ini
root = true

[*]
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true
charset = utf-8
indent_style = space
indent_size = 4

[*.{md,yml,yaml}]
indent_size = 2

[Makefile]
indent_style = tab
```

## Code quality fixes

Concrete issues found during implementation, batched into one cleanup pass:

1. **`menu.c` extern redeclaration of `font8x8`.** Fix: add `#include "font8x8.h"` at the top of menu.c, remove the inline `extern const uint8_t font8x8[96][8];` from inside `draw_string_inverse`.
2. **`sh1106_init` returns void**, swallows `spi_bus_initialize` / `spi_bus_add_device` / `gpio_config` errors. Fix: change signature to `esp_err_t sh1106_init(void)`, propagate failures via `ESP_RETURN_ON_ERROR` or equivalent. Update `main.c` and any other caller to check the return.
3. **`menu_init` accepts NULL `root` silently.** Fix: `assert(root != NULL);` and document the precondition in the Doxygen block. Keep return type `void`.
4. **`render_list` cast-away-const on `frame_t`.** Fix: change the parameter to `frame_t *` (mutable); the storage is the static `s_stack[]` so this is honest.
5. **Magic numbers in `sh1106.c`** — most are SH1106 command bytes and are best left as the hex literals with a comment. Already done. Pull any remaining nameable constants (`SH1106_PAGES`, `SH1106_FB_SIZE`, `SH1106_COL_OFFSET` already exist) — no additional changes needed here. Skip per YAGNI.
6. **Missing assertions** at API entry — add `assert(s != NULL)` in `sh1106_draw_string`, `sh1106_set_pixel` (already clipped, but assert framebuffer-init order via a static guard), and similar in `menu`/`buttons` public functions. Keep cheap (`#include <assert.h>`).
7. **`buttons.c emit()` doesn't guard against NULL queue.** Add `if (s_queue == NULL) return;` as defensive prefix.
8. **`static_assert` invariants.** Add to `sh1106.c`:
   - `_Static_assert(SH1106_PAGES * 8 == SH1106_HEIGHT, "page count must match height");`
   - `_Static_assert(SH1106_FB_SIZE == SH1106_WIDTH * SH1106_PAGES, "framebuffer size mismatch");`
   And to `font8x8.c`:
   - `_Static_assert(sizeof(font8x8) == 96 * 8, "font8x8 must be 96 glyphs of 8 bytes");`
9. **Include hygiene.** After the format pass, audit each file's include block: remove unused headers (clangd warnings give us the list), put them in the order the `.clang-format` IncludeCategories define.

## Documentation

### Doxygen in public headers

Every public function in each component's header gets a block like:

```c
/**
 * @brief Set or clear a single pixel in the framebuffer.
 *
 * Coordinates outside the 128x64 visible area are silently ignored.
 * The pixel is staged in RAM; call sh1106_flush() to push it to the display.
 *
 * @param x  Column, 0..SH1106_WIDTH-1.
 * @param y  Row, 0..SH1106_HEIGHT-1.
 * @param on true = pixel lit, false = pixel cleared.
 */
void sh1106_set_pixel(int x, int y, bool on);
```

Every public type / enum / struct gets a `@brief` plus per-field documentation:

```c
/**
 * @brief One menu item (label, optional icon, and either an action or a submenu).
 */
typedef struct {
    menu_item_kind_t kind;   /**< Discriminator for the union below. */
    const char      *label;  /**< Display text. Required. */
    const uint8_t   *icon;   /**< Optional 32x32 page-major bitmap; NULL for text-only items. */
    union {
        void (*action)(void *user_ctx);     /**< Called when kind == MENU_ITEM_ACTION. */
        const menu_t *submenu;              /**< Used when kind == MENU_ITEM_SUBMENU. */
    } u;
} menu_item_t;
```

Internal (file-local) helpers do not get Doxygen blocks — they get plain one-line comments only if their purpose isn't obvious from the name.

### Root `README.md`

Sections:

1. **Title + one-paragraph description.**
2. **Components table** — name / purpose / public API summary.
3. **Hardware** — OLED wiring table, button wiring table.
4. **Build & flash the demo** — short prereq list + the `idf.py build flash monitor` commands.
5. **Using a component in your own project** — two patterns: `EXTRA_COMPONENT_DIRS` or copy the dir into your project's `components/`.
6. **Project layout** — tree showing what's where.
7. **Status / license** — "personal learning project; u8g2 font is BSD-2-Clause, see components/font8x8/README.md".

### Per-component `README.md`

Same template for all four components:

1. **Title** = component name.
2. **One-paragraph description.**
3. **Public API** — table or list with one-line briefs for each function and type.
4. **Dependencies** — REQUIRES list with one-line reasons.
5. **Usage** — minimal working `app_main`-style example.
6. **Notes / caveats** — non-obvious behavior, configuration hooks, gotchas (e.g. for sh1106: "if you see garbage, check DC/CS wiring").

### `main/README.md`

Short. Says "this is the demo; libraries live in `../components/`; tree shows the demo's menu structure; explains the icon generator in `tools/gen_icons.py`".

## Task breakdown (preview for the plan)

Roughly 20 tasks, in dependency order:

1. Add `.editorconfig`
2. Add `.clang-format`
3. Run `clang-format -i` over all C/H — pure-format commit
4. Add `.clang-tidy`
5. Run `clang-tidy`, batch-fix the findings
6. Code quality pass: error returns from `sh1106_init`, NULL-guard `menu_init`, drop const-cast in render_list, add static_asserts, etc.
7. Create `components/font8x8/` and move font8x8 there
8. Create `components/sh1106/` and move sh1106 there
9. Create `components/buttons/` and move buttons there
10. Create `components/menu/` and move menu there (also fixes the extern font8x8 redeclaration by including font8x8.h properly)
11. Update `main/CMakeLists.txt` to REQUIRE all four components, leave icons.{h,c} and main.c in `main/`
12. Update `.clangd` for new include paths
13. Add root `README.md`
14. Add `components/font8x8/README.md`
15. Add `components/sh1106/README.md`
16. Add `components/buttons/README.md`
17. Add `components/menu/README.md`
18. Add `main/README.md`
19. Doxygen pass on `sh1106.h` + `font8x8.h`
20. Doxygen pass on `buttons.h` + `menu.h`

Each task ends with an `idf.py build` (and `flash monitor` for the four restructuring tasks that move runtime code) and a commit. Demo functionality must remain unchanged throughout.

## Non-goals (this iteration)

- No `idf_component.yml` manifests / publishing to ESP Component Registry.
- No `LICENSE` file at project root.
- No `CHANGELOG.md`.
- No CI pipeline (GitHub Actions / pre-commit).
- No Doxygen HTML output generation step.
- No `Kconfig` / menuconfig integration.
- No new runtime functionality.
- No tests / mocks / unit-test harness.

## Risks

- **Big aggregated diff.** Componentizing + formatter + linter + Doxygen + docs touches almost every file. The plan splits this into ~20 small tasks each with a build checkpoint to catch breakage early.
- **`menu.c`'s extern-without-include hack** works today because both files end up linked together. After moving to separate components, `menu` must `REQUIRES font8x8` and `#include "font8x8.h"` or the link breaks. Fixing this is part of the code-quality task (item #1) and reinforced when the menu component is created.
- **`.clangd` index** will be stale immediately after restructuring. Editor may show errors until the next build regenerates `compile_commands.json`. Not a blocker, just an annoyance.
- **ESP-IDF version pin.** Component auto-discovery and the `REQUIRES` syntax assume ESP-IDF v5+. The boot log earlier confirmed v5.5.4 — fine.
- **Doxygen verbosity.** Full `@brief` / `@param` / `@return` on every public function doubles the line count of each header. Trade-off accepted by the user explicitly.
