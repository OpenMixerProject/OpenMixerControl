# Porting to OpenMixerControl / LVGL 9.5

Goal: turn the prototype into a real theme for the `omc` program that runs on the M32.

## 1. Understand where to work

- `OpenX32` = boot (U-Boot/barebox), kernel 6.18, initramfs, toolchain. **The UI is not touched here.**
- `OpenMixerControl` = the `omc` program, LVGL 9.5 GUI, EEZ Studio project `OpenX32Gui.eez-project`, sources in `src/`, libraries in `lib/`, build with `Makefile` / `compile-docker.sh`.

```bash
git clone https://github.com/OpenMixerProject/OpenMixerControl.git
cd OpenMixerControl
git checkout -b m32-ui
```

## 2. Recreate the screens in EEZ Studio

Open `OpenX32Gui.eez-project` with EEZ Studio and add 16 screens with the same IDs as the prototype:

```
overview, channel, eq, dyn, aux, setup, assign, monitor, routing,
delay, meter, fx, card, macro, scenes, about
```

`setup` (SHOW CONFIG) goes next to `about` (SYSTEM) in the tab bar; SYSTEM has two buttons that open SETUP and FADER MAP.

Use `docs/DESIGN.md` as the color and geometry specification. Export the project by regenerating the generated files the way the upstream Makefile does.

## 3. HTML widget → LVGL translation

| Prototype | LVGL 9.5 |
|---|---|
| `.tab` / `#tabs` | `lv_tabview` or `lv_buttonmatrix` with 16 buttons |
| `.soft` | vertical `lv_buttonmatrix`, 14 buttons |
| `.strip` | `lv_obj` container + `lv_label` (number/name) + `lv_button` M/S |
| `.pan-mini` | horizontal `lv_bar` with indicator and `lv_label` L/C/R, drag via `lv_indev` |
| `.ftrack` + `.fcap` | vertical `lv_slider`, or `lv_obj` with `lv_indev` drag |
| `.ffill` | `lv_bar` bound to the fader value |
| `.fmeter .lvl` | `lv_bar` / `lv_meter`, updated by a timer |
| `.vum-group .vum` | two L/R `lv_bar`s or a single M one, based on `CH_MODE`/`CH_LINK` |
| `.knob` | `lv_arc` or `lv_meter` with a 270° indicator |
| `.scene-row`, `.macro-key` | `lv_list` / `lv_buttonmatrix` with slot IDs in user data |
| `.bank-picker .bank-btn` | 4-column `lv_buttonmatrix`: 8 input banks (2 rows) + 4 output banks (1 row) |
| `.slot-cell`, `.assign-item` | `lv_list` with slot/fader IDs in user data |
| `.count-row`, `.ich`, `.fmt-cell` | `lv_buttonmatrix` / `lv_switch` for counts and MONO/STEREO format |
| `<svg>` EQ/dyn | `lv_chart` (line series) or `lv_canvas` for the computed curve |
| `.matrix` | `lv_table` with clickable cells (`LV_EVENT_CLICKED` + row/column) |
| `.track` / `.fx-slot` | `lv_list` / `lv_obj` with flex layout |

Suggestion: keep a single **event handler** per screen and pass the parameter ID with `lv_event_get_user_data()`, exactly like `data-act`/`data-i` in `app.js`.

## 4. Connect the mixer parameter engine

The prototype has local state (`state.channels`). In firmware it must **not** be duplicated:

- every widget writes/reads a **parameter ID** of the existing engine (gain, HPF, EQ band, gate, comp, fader, mute, solo, routing);
- the engine only notifies changed values: subscribe to those notifications and call `lv_label_set_text()` / `lv_bar_set_value()` only on the affected object;
- avoid `lv_obj_clean()` + full rebuild on every change: that is the difference between stable 30 fps and a visible refresh.

Recommended mapping (to be added to the binding code):

```
CH_GAIN, CH_PAN, CH_HPF_FREQ, CH_PHASE, CH_FADER, CH_MUTE, CH_SOLO
CH_EQ_F[0..3], CH_EQ_G[0..3], CH_EQ_Q[0..3], CH_EQ_MODE[0..3] (LOW CUT / LOW SHELF / PEQ / VEQ / HIGH SHELF / HIGH CUT)
CH_GATE_TYPE (GATE/EXPANDER/DUCKER), CH_GATE_THR/ATK/REL/ON
CH_COMP_TYPE (VCA/FET/OPTICAL/TUBE), CH_COMP_THR/RATIO/ATK/REL/ON
CH_AUX_SEND[i][bus], CH_AUX_MUTE[i][bus], AUX_LEVEL[bus], AUX_MUTE[bus], AUX_MODE[bus], AUX_MASTER[bus], AUX_INTERNAL_MAP[i][bus], AUX_EXTERNAL_MAP[bus][dst] (shares ROUTE_SRC/DST patch)
CH_MODE[i] (MONO/STEREO), CH_LINK[i], CH_INSERT_SLOT[i], CH_INSERT_POS[i]
INPUT_DELAY[i], OUTPUT_DELAY[n], ROUTE_SRC[group][i]_DST[group][j]
FX_SLOT[n]_PARAM[m], FX_SLOT[n]_POS, FX_SLOT[n]_OUT (XLR/USB/CARD1/CARD2), CARD_STATE
CLOCK_RATE (44.1/48), CLOCK_SOURCE, VIDEO_FPS (24/25/30/50/60)
MON_SOURCE, MON_LEVEL, MON_DIM/MONO/MUTE, MON_DEST
SHOW_SETUP{inputOn[],auxOn[],fxOn[],groupOn[],matrixOn[],outOn[],hideUnused,formats,dests}
FADER_BANK[b]{name,slot[12],type} (8 input banks 2×4 + 4 output banks 1×4), FADER_BANK_ENABLED
ROUTE_SRC[group][i]_DST[group][j] (also GROUP→GROUP, GROUP→AUX, MATRIX→GROUP)
MACRO_STEP[button][n], SCENE_SLOT[n]{name,snapshot,safe}, SCENE_CURRENT, SCENE_SAFE[group]
```

## 5. Performance (800×480, ~30 fps)

- Enable partial draw buffering; do not invalidate the whole screen every frame.
- Separate LVGL timers:
  - 100 ms `lv_timer` → meters and spectrum (10 Hz);
  - 300 ms `lv_timer` → clock and secondary text.
- Recompute the EQ curve only when a band changes, not every frame.
- Use preloaded fonts and fixed sizes: avoid `lv_label_set_text_fmt()` in loops.
- If more speed is needed, draw the curve on `lv_canvas` and call `lv_obj_invalidate()` only on that object.

## 6. M32 physical surface

- The `producer_button_map.txt` file in `OpenX32` describes the surface keys: add a second map for SEL/MUTE/SOLO/HOME/EQ/DYN/AUX.
- 25 physical faders → 12 logical faders + master: use the first 12 for the active bank and the last ones for master/groups; the active bank comes from the selected `FADER_BANK`, not from a fixed layer list.
- Encoders + wheel: channel selection and EQ band; soft keys 1–8 for pages.
- Always verify that LEDs follow the parameter engine state, not the widget state.

## 7. Build and test

```bash
cd OpenMixerControl
./compile-docker.sh          # or make, see upstream README
```

The generated artifact must replace/be included in the `OpenX32/compile.sh` process (which packages `omc` into the initramfs). To test without writing to the SD card:

1. copy the boot file to a USB stick as described in the OpenX32 README;
2. boot the M32 with the USB stick inserted;
3. verify boot, display, surface, audio in/out **before** judging the UI;
4. only then evaluate the theme.

Minimum checklist:

- [ ] USB boot and DHCP/SSH network
- [ ] 16 pages navigable by touch and soft keys
- [ ] M32 faders/encoders move the correct parameters
- [ ] per-channel mini-pan and PAN knob synchronized with `CH_PAN`
- [ ] mono M / stereo L-R meters and MONITOR meters (DIM/MONO/MUTE) consistent
- [ ] mute/solo and selection consistent between display and LEDs
- [ ] EQ/dynamics (including per-band EQ mode and gate/comp types) write to DSP1 parameters
- [ ] AUX sends PRE/POST and mute follow mixbus parameters
- [ ] routing AES50 A/B, ULTRANET, CARD 1/2, USB, GROUP→GROUP/AUX and MATRIX→GROUP consistent with the FPGA
- [ ] HIDE UNUSED really removes disabled resources from strips, sends, meters and the matrix
- [ ] 12 fader banks (input 2×4 + output 1×4): FREE ALL, per-slot assignment and return to straight mapping
- [ ] mono/stereo and link really write to DSP1 channels
- [ ] per-channel/FX insert with PRE/POST position written to DSP
- [ ] clock rate/source and video frame rate applied to the audio engine
- [ ] input/output delay expressed in m/ms/frames and applied to the DSP buffer
- [ ] macro REC/PLAY trigger actions on the surface and parameters
- [ ] scene STORE/RECALL with SAFE respected and persistence on storage
- [ ] routing and card unchanged from upstream
- [ ] no third-party logos or assets in the project

## 8. After the UI

Only when Phase 2 is stable:

- DSP2: FX already present in OpenX32 (reverb/chorus/delay/overdrive), to be exposed on the `EFFECTS` page;
- DSP1: dynamic EQ and multiband compressor are WIP upstream;
- FPGA: AES50 and routing stay as in OpenX32 so audio is not broken;
- document differences from the original firmware before any public release.
