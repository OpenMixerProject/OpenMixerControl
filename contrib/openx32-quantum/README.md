# OpenX32 · M32 UI

Interactive UI prototype for **Midas M32 / Behringer X32**, built on the visual concepts of [OpenX32](https://openx32.com/screenshots/) with a layout inspired by large-format digital consoles.

> ⚠️ **This is not flashable firmware yet.** It is a navigable UI specification (800×480, the same framebuffer size as the M32) plus a porting plan for LVGL inside `OpenMixerControl`. DSP, FPGA and boot loader remain those of OpenX32.

## Getting started

Open `index.html` in a browser (works from `file://`, zero dependencies).

```bash
xdg-open index.html
# or: chromium index.html
```

Shortcuts:

- `1`–`9`: first nine pages · `0` = SYSTEM · `-` = MACRO · `=` = SCENE · `V` = MONITOR
- `P` = SHOW CONFIG · `G` = FADER MAP · `D` = DELAY
- `←` / `→`: change the selected channel
- `M` / `S`: mute / solo the selected channel
- Drag faders, knobs, EQ nodes and the **PAN bar on every strip** with mouse or touch
- Deep links: `index.html#eq`, `#routing`, `#macro`, `#scenes`, …

## What is inside

| Screen | Content |
|---|---|
| `OVERVIEW` | Up to 12 strips, banks grouped as **input 2×4 + output 1×4**, master LR, quick control of the selected channel |
| `CHANNEL` | Input / Filter / Gate / Comp blocks, large fader and channel bank |
| `EQ` | 4-band curve with draggable nodes, RTA and per-band mode (LOW CUT / LOW SHELF / PEQ / VEQ / HIGH SHELF / HIGH CUT) |
| `DYNAMICS` | Gate and compressor curves with threshold, attack, release, ratio |
| `AUX SENDS` | Active AUX buses with send faders, PRE/POST, send mute, bus master, internal source map and external map that writes the routing matrix |
| `SHOW CONFIG` (`CFG`) | Defines **what exists in the show**: counts of INPUT / AUX / FX / GROUP / MATRIX / OUT, MONO/STEREO format, group destinations, FX PRE/POST, **HIDE UNUSED**; also reachable from SYSTEM |
| `FADER MAP` (`MAP`) | **12 banks × 12 faders: 8 input banks (2 rows × 4) + 4 output banks (1 row × 4)**: FREE ALL → SEL bank → SEL slot → SEL source from INPUT/FX RET/GROUP/OUT; STRAIGHT restores the default mapping |
| `MONITOR AUDIO` | Control room / headphones source, DIM, MONO, MUTE, level and L/R meters |
| `ROUTING` | Matrix with rows/columns limited to active resources: INPUT, AUX, GROUP, FX RET, MATRIX → XLR/AES50/CARD/USB/GROUP IN/**MATRIX IN**/DSP — includes GROUP→GROUP, GROUP→AUX, MATRIX→GROUP |
| `DELAY` | Input/output delay in meters, ms and frames at the selected video frame rate |
| `METERS` | Horizontal 16-channel meterbridge + master L/R |
| `EFFECTS` | 4 FX slots (reverb, chorus, delay, overdrive) with bypass |
| `CARD` | SD/USB, track list, transport, counter |
| `MACRO / RECALL` | 8 assignable keys, REC / PLAY / CLEAR / SAVE with action sequences |
| `SCENE MANAGER` | SHOW → 4 BANK × 8 SCENE structure, STORE / RECALL / COPY / PASTE / DELETE, per-group SAFE |
| `SYSTEM` | 44.1/48 kHz clock, source, 24/25/30/50/60 fps frame rate, info and M32 mapping |

### Cross-cutting features

- **Show setup**: INPUT 1–24, AUX 1–8, FX 1–4, GROUP 1–8, MATRIX 1–6, XLR OUT 1–16. With **HIDE UNUSED** enabled, disabled resources disappear from strips, AUX sends, monitor, FX, meters and the routing matrix.
- **Inputs**: 24 channels + 4 FX RET as input channels + 8 MIXBUS + 16 XLR OUT assignable to faders; each channel can be **MONO** or **STEREO** and can be **linked** to the next one.
- **Fader map**: default "straight" mapping (INPUT → FX RET → GROUP → OUT); FREE ALL empties every bank, then each fader can be assigned individually. The mapping is stored in scenes (SAFE group `FADER MAP`).
- **PAN per channel**: L/C/R mini bar on every OVERVIEW strip, synchronized with the PAN knob in CHANNEL/QUICK views; horizontal drag, recordable in macros.
- **Meters**: MONO shows **M**, STEREO/linked shows **L/R**; the MONITOR page has L/R meters with MONO/DIM/MUTE.
- **Dynamics**: gate `GATE / EXPANDER / DUCKER`, compressor `VCA / FET / OPTICAL / TUBE`.
- **Insert**: per channel and per FX slot with `PRE EQ / POST EQ / PRE FADER / POST FADER` position.
- **FX outputs**: XLR, **USB**, **CARD 1**, **CARD 2**.
- **Clock**: 44.1/48 kHz, source `INTERNAL / EXTERNAL / AES50 A/B / CARD 1/2`, video frame rate **24/25/30/50/60 fps**.
- **Delay/alignment**: input/output delay in **meters, ms and frames**; frames use the fps set in SYSTEM (343 m/s).
- **Macros**: 8 banks A1–A8 record clicks, faders, knobs, pan and EQ nodes; PLAY replays them, CLEAR/SAVE persist in the browser.
- **Scenes**: 32 slots in 4 banks, full state storage including SETUP and FADER MAP, selective recall with SAFE for SETUP / FADER MAP / INPUT / AUX / ROUTING / OUTPUT / MONITOR / MACRO, COPY/PASTE and AUTO SAVE.

## The real constraint

- OpenX32 runs on **i.MX25 / ARM926EJ-S** with an **800×480** framebuffer and LVGL 9.5 at ~30 fps.
- The large-format console referenced for inspiration has a 17" high-resolution touchscreen, a dedicated meterbridge and 18 TFT displays: the similarity can only be **conceptual** (palette, hierarchy, strips, selection colors, panel layout), not pixel-perfect.
- The M32 has 25 physical faders: the prototype uses **12 logical faders × 12 banks** (input 2×4 + output 1×4), mapped onto the existing physical faders.

## Structure

```
index.html     800×480 shell (topbar, soft keys, screen, bottom bar)
styles.css     design tokens, LVGL-equivalent components
app.js         state, 16 screens, interactions, show setup, fader map, macros, scenes, meters
docs/
  DESIGN.md     palette, grid, components, mapping to M32
  PORTING.md    concrete steps to integrate into OpenMixerControl
  PROPOSAL.md   proposal to send to the project
  FORUM_POST.md ready-to-paste forum post
screenshots/   6 × 800×480 screenshots (overview, setup, assign, routing, scenes, monitor)
```

## Roadmap

1. **Phase 0 — UI spec** ✅ this prototype.
2. **Phase 1 — EEZ Studio**: recreate the 16 screens in `OpenX32Gui.eez-project`.
3. **Phase 2 — LVGL bindings**: connect widgets to the *mixer parameter engine* and the M32 surface.
4. **Phase 3 — Build & test**: fork `OpenMixerControl`, build via Docker, boot from USB on the M32.
5. **Phase 4 — DSP/FPGA parity**: only after the UI is stable (EQ, dynamics, FX, AES50).

See [`docs/PORTING.md`](docs/PORTING.md) for details.

## References

- OpenX32 screenshots: https://openx32.com/screenshots/
- Firmware / kernel: https://github.com/OpenMixerProject/OpenX32
- UI, LVGL and EEZ Studio: https://github.com/OpenMixerProject/OpenMixerControl
- Community: https://discourse.openmixerproject.de/
- Large-format console documentation (public): https://digico.biz/consoles/quantum112

## Legal note

Hobbyist and educational project. The UI is **inspired** by large-format digital consoles: it contains no logos, artwork, icons or trademarks of DiGiCo and no protected assets, and does not use the name "Quantum UI". The upstream OpenX32/OpenMixerControl code is covered by its own GPL license: respect it in any fork.
