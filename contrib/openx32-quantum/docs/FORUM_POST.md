## [Proposal] M32 UI 800×480 — show configuration + programmable fader banks

Hello everyone,

I have prepared a navigable UI prototype for M32/X32 and would like to ask for feedback before porting it into `OpenMixerControl`.

**Folder:** `openx32-quantum` (`index.html`, `styles.css`, `app.js`, documentation). It opens directly from `file://`, with no dependencies.

**Complete package:** [release v19](https://github.com/jonniavid/openx32-quantum-review/releases/tag/v19) · [direct ZIP](https://github.com/jonniavid/openx32-quantum-review/releases/download/v19/openx32-quantum-review.zip) · [source repository](https://github.com/jonniavid/openx32-quantum-review).

### The idea

Instead of always showing the whole console, the UI starts from a **SHOW CONFIG** page where you define what exists in the show: number of INPUTs, AUX, FX, GROUPs, MATRIX and OUTPUTs, mono/stereo format, group destinations and FX pre/post. With `HIDE UNUSED`, unused resources **really disappear** from overview, sends, meters, monitor and the routing matrix.

Next to it is **FADER MAP**: 12 banks, 12 faders each: 8 input banks on two rows of 4 and 4 output banks on one row of 4. They are "straight" by default (INPUT → FX RET → GROUP → OUT), but `FREE ALL` clears them all and then each fader can be assigned from the INPUT / FX RET / GROUP / OUT list. All of this is saved in the scene, with a dedicated SAFE group.

I also added the internal paths that were missing: **GROUP → GROUP**, **GROUP → AUX** and **MATRIX → GROUP**, in the same routing matrix. The AUX page has a bus master, an internal source map and an external destination map that writes the very same routing patches. On EQ, each of the four bands can now select LOW CUT / LOW SHELF / PEQ / VEQ / HIGH SHELF / HIGH CUT, and the curve follows the mode.

### Why I am proposing it here

Because it is already a navigable and portable UI specification: 16 screens with stable IDs, actions with parameter IDs, no protected assets, and the same framebuffer resolution (800×480). The plan for EEZ Studio / LVGL 9.5 is in `docs/PORTING.md`.

### Try it in 30 seconds

1. `CFG` → `MIN` preset → `OVER`: only 8 inputs appear.
2. In `CFG` disable INPUT 01 → in `OVER` channel 1 disappears.
3. `MAP` → `FREE ALL` → `OVER`: no faders.
4. `MAP` → `OUT` list → touch `OUT 01`: the fader comes back.
5. `ROUTE` → `GROUP → GROUP IN` / `MATRIX → GROUP IN`.
6. `SCENE` → STORE / RECALL: the setup and the bank map return identically.

![Overview](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/overview.png)
![Setup](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/setup.png)
![Assign](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/assign.png)
![Routing](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/routing.png)
![Eq](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/eq.png)
![Aux](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/aux.png)
![Monitor](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/monitor.png)
![Scenes](https://raw.githubusercontent.com/jonniavid/openx32-quantum-review/main/screenshots/scenes.png)

### Questions

- Does this "show-first" approach make sense before other UI work?
- Which mixer-parameter-engine parameters should I connect first for fader banks and internal routing?
- If the theme is accepted, may I proceed with the EEZ Studio screens and LVGL bindings on a fork?

I have not touched DSP, FPGA, boot or audio routing of OpenX32. No logo or visual element of other consoles has been reproduced.

Thanks for the feedback!
