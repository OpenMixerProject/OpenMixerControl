# Proposal for OpenMixerControl — M32 UI "Show Setup + Fader Map"

## Summary

An **800×480, zero-dependency** UI prototype for Midas M32 / Behringer X32, built on the visual concepts of OpenX32 but with one specific idea: **define the show first, then let the console adapt**. The result is not another generic theme: it is an operational configuration that avoids showing channels and functions the show does not use.

It is not flashable firmware. It is a **navigable UI specification** plus a porting plan for LVGL 9.5 inside `OpenMixerControl`.

## Why it is worth trying

1. **Fewer operator mistakes.** With `HIDE UNUSED` enabled, disabled resources really disappear from OVERVIEW, AUX, MONITOR, FX, METERS and ROUTING. If the show has 8 inputs, the operator sees 8 faders, not 24.
2. **The surface is programmable.** 12 banks × 12 faders: 8 input banks (A01–A04 / B01–B04, 2 rows of 4) and 4 output banks (O01–O04, 1 row of 4). Straight by default; `FREE ALL` clears everything; then SEL bank → SEL slot → SEL source (INPUT / FX RET / GROUP / OUT) builds the bank however you want.
3. **Complete internal routing.** In addition to the existing paths, the matrix covers **GROUP→GROUP**, **GROUP→AUX** and **MATRIX→GROUP**, with rows/columns limited to active resources.
4. **Everything is stored in the scene.** SETUP and FADER MAP are independent SAFE groups: 32 slots in 4 banks, STORE/RECALL/COPY/PASTE/AUTO SAVE.
5. **Portable.** Every screen has a stable ID, every action has `data-act`/`data-i` that become `lv_event_get_user_data()`. No third-party assets, logos or trademarks.

## What is already there

- 16 pages: OVER, CHAN, EQ, DYN, AUX, MAP, MON, ROUTE, DLY, MET, FX, CARD, MACRO, SCENE, CFG, SYS.
- CFG sits next to SYS and can also be opened directly from SYS; MAP has a dedicated soft key.
- 44.1/48 kHz clock, AES50/CARD source, 24/25/30/50/60 fps video rate used by the frame calculation on DELAY.
- PAN on every individual strip, synchronized with the PAN knob; mono M / stereo L-R meters; monitor DIM/MONO/MUTE.
- Gate `GATE/EXPANDER/DUCKER`, comp `VCA/FET/OPTICAL/TUBE`, PRE/POST insert per channel and FX.
- Per-band EQ modes LOW CUT / LOW SHELF / PEQ / VEQ / HIGH SHELF / HIGH CUT, with the graph following the selected mode.
- Macros A1–A8 with REC/PLAY on clicks, faders, knobs, pan, EQ nodes and EQ mode changes.

## How to try it

```bash
cd openx32-quantum
xdg-open index.html          # works from file:// too
```

Suggested evaluation path:

1. Open `CFG` → press `MIN` → go back to `OVER`: only 8 inputs are visible.
2. Disable INPUT 01 in `CFG` → in `OVER` channel 1 disappears.
3. Open `MAP` → `FREE ALL` → go back to `OVER`: no faders.
4. Back in `MAP` → choose `OUT` → touch `OUT 01`: the fader comes back.
5. `AUX` → toggle the internal source map and an external destination; then check `ROUTE` to see the same patch.
6. `EQ` → change a band mode and watch the curve.
7. `ROUTE` → `GROUP → GROUP IN` and `MATRIX → GROUP IN`: try the patches.
8. `SCENE` → STORE, change the setup, RECALL: everything returns, except SAFE groups.

Screenshots are ready in `screenshots/` (800×480, generated from the prototype).

## What I am asking the project

- Feedback on the **show-first** approach: does it make sense to integrate `SHOW CONFIG` + `FADER MAP` before broader UI refactoring?
- If it is welcome, I will take on Phase 1 (EEZ Studio screens) and Phase 2 (LVGL bindings) on a fork of `OpenMixerControl`, following `docs/PORTING.md`.
- Any notes on which *mixer-parameter-engine* parameters should be exposed first (especially fader banks and internal routing).

## Constraints respected

- No logos, artwork, icons or trademarks of large-format console vendors; no "Quantum UI" wording.
- Same framebuffer resolution as the M32 (800×480).
- DSP, FPGA, boot loader and audio routing of OpenX32 remain unchanged.
- Upstream GPL license respected in any fork.

References: `README.md`, `docs/DESIGN.md`, `docs/PORTING.md`, `docs/FORUM_POST.md`, `docs/SUBMISSION.md`.
