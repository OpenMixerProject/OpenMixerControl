Hello everyone,

I would like to propose, with great humility and simplicity, a navigable **800×480 UI prototype** for the M32/X32 form factor. It is not firmware: it does not touch DSP, FPGA, boot or audio routing. It is a zero-dependency UI specification plus a porting plan for `OpenMixerControl`. Maintainers are welcome to modify, reduce or discard anything.

### The idea

Instead of always showing the whole console, the UI starts from **SHOW CONFIG**, where the operator defines what exists in the show: number of INPUTs, AUX, FX, GROUPs, MATRIX and OUTPUTs, mono/stereo format, group destinations and FX pre/post. With `HIDE UNUSED`, unused resources really disappear from OVERVIEW, AUX, MONITOR, FX, METERS and ROUTING. If the show has 8 inputs, the operator sees 8 faders, not 24.

Next to it is **FADER MAP**: 12 banks × 12 faders — 8 input banks on two rows of 4 (A01–A04 / B01–B04) and 4 output banks on one row of 4 (O01–O04). They are straight by default, `FREE ALL` clears everything and each fader can then be assigned from INPUT / FX RET / GROUP / OUT. The mapping is stored in the scene with a dedicated SAFE group.

The AUX page now has a bus master, an internal source map and an external destination map that writes the **same** routing patches used by the ROUTING matrix. The matrix also covers the missing internal paths **GROUP → GROUP**, **GROUP → AUX** and **MATRIX → GROUP**, with rows and columns limited to active resources.

The EQ page offers per-band filter modes, as on the surface: **LOW CUT / LOW SHELF / PEQ / VEQ / HIGH SHELF / HIGH CUT**. The curve and the draggable nodes follow the selected mode.

### Files

- Complete review package: [openx32-quantum-review v19](https://github.com/jonniavid/openx32-quantum-review/releases/tag/v19) — [direct ZIP download](https://github.com/jonniavid/openx32-quantum-review/releases/download/v19/openx32-quantum-review.zip). GitHub issues cannot attach ZIP files, so it is hosted as a release asset under my account.
- `index.html`, `styles.css`, `app.js` — open directly from `file://`, no dependencies.
- `README.md` — quick start and feature list.
- `docs/DESIGN.md` — UI specification.
- `docs/PORTING.md` — EEZ Studio / LVGL 9.5 porting plan.
- `docs/PROPOSAL.md` and `docs/SUBMISSION.md` — proposal and local review notes.

No logos, artwork, icons, trademarks or protected console assets are included. All wording is in English.

### Screenshots (800×480)

![Overview](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/overview.png)

![Show config](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/setup.png)

![Fader map](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/assign.png)

![Routing](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/routing.png)

![EQ filter modes](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/eq.png)

![AUX bus and maps](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/aux.png)

![Monitor](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/monitor.png)

![Scenes](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/scenes.png)

### Try it in 30 seconds

1. `CFG` → `MIN` → `OVER`: only 8 inputs appear.
2. In `CFG` disable INPUT 01 → in `OVER` channel 1 disappears.
3. `MAP` → `FREE ALL` → `OVER`: no faders.
4. `MAP` → `OUT` list → touch `OUT 01`: the fader comes back.
5. `AUX` → toggle internal sources and one external destination; then check `ROUTE`.
6. `EQ` → change a band mode and watch the curve.
7. `SCENE` → STORE / RECALL: setup and fader map return, except SAFE groups.

### Checks already performed

- `node --check app.js` passes.
- All 16 pages load without JavaScript errors in headless Chromium.
- Every page stays inside the 800×480 framebuffer with no overflow.
- Screenshots regenerated from version tag `v=19`.

### Questions

- Does a show-first approach make sense before other UI work?
- Which `mixer-parameter-engine` parameters should be connected first for fader banks, EQ modes and internal routing?
- If the direction is welcome, may I proceed with the EEZ Studio screens and LVGL bindings on a fork of `OpenMixerControl`?

Thank you for the feedback, and thank you for the work already done on OpenX32.
