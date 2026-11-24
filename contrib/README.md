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
- Left/Right arrows: previous/next tab in the overlay (normal OMC page
  navigation when the overlay is hidden).
- Up/Down arrows: move the highlighted item; on FADER MAP they move the
  selected slot.
- Encoders 1-6: turn the first six on-screen knobs of the current page. On
  FADER MAP, encoders 1/2 still select bank/slot. Simulator keys: `Q`/`A`,
  `W`/`S`, `E`/`D`, `R`/`F`, `T`/`G`, `Z`/`H`.
- Button 2 (`X`): apply the slot on FADER MAP, otherwise activate the
  focused item. Button 6 (`N`): reset the map or clear the focus.

## Page notes

- SETUP has a NETWORK / IP ADDRESS block: select an octet and use `-` / `+`
  to edit it, or toggle DHCP. This is a UI-only value for now; connecting it
  to the mixer's network configuration is a later step.
- The DELAY / ALIGNMENT page uses an enlarged two-column grid. The buttons in
  DELAY EDIT switch between INPUT 1-14, INPUT 15+, OUTPUT 1-14, OUTPUT 15+
  and AUX 1-8, so every channel stays reachable without shrinking the text.

## Build

```bash
./compile.sh TARGET_PC_SDL2
./build/pc_sdl2/omc -b
```
