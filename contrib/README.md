# OpenX32/M32 show-first UI prototype in OpenMixerControl

This branch (`ui/show-first-prototype`) ports the static 800x480 browser
prototype into the real `omc` GUI as a native LVGL 9.5 page.

## What is here

- `openx32-quantum/` — the original dependency-free `file://` prototype,
  kept verbatim as a design reference (screenshots, DESIGN.md, PORTING.md).

## Native LVGL page

A new page `SHOW_CONFIG` was added without touching the EEZ Studio project:

| File | Change |
|------|--------|
| `src/page-showconfig.h` | New header-only page: 12x12 fader bank matrix, amber `#ff9d00` accent, encoder bindings. |
| `src/enum.h` | Added `SHOW_CONFIG` to `X32_PAGE`. |
| `src/ctrl-client.cpp` | Included/registered the page; bound simulator key `7` to jump to it. |
| `src/page-prototypegui.h` | Wired `nextPage` to `SHOW_CONFIG`. |

The page reuses the existing empty "Prototype GUI" tab (Setup > Prototype GUI),
so no EEZ Studio regeneration is needed. All widgets are created
programmatically in `OnInit()`.

## Navigation

- Press `7` in the SDL2 simulator to open the page directly.
- Or navigate: Setup > Prototype GUI > (next page) SHOW CONFIG.
- Encoder 1: select bank (1-12). Encoder 2: select slot (1-12).
- Button 2: apply selected slot to the first input bank. Button 6: reset.

## Build

```bash
./compile.sh TARGET_PC_SDL2
./build/pc_sdl2/omc -b
```
