# Design system — M32 UI concept

## Palette

| Token | Hex | Use |
|---|---|---|
| `--bg` | `#0d0e10` | console background |
| `--panel` | `#191b1e` | panels |
| `--panel-2` | `#22252a` | secondary panels |
| `--line` | `#353a41` | borders |
| `--txt` | `#d9dde1` | text |
| `--dim` | `#7d858d` | secondary labels |
| `--amber` | `#ff9d00` | selection, faders, curves, active tab |
| `--amber-soft` | `#ffc45e` | numeric readouts |
| `--green` | `#65d472` | signal / healthy meter |
| `--yellow` | `#ffd23f` | meter +6 dB |
| `--red` | `#ff5142` | clip / mute |
| `--cyan` | `#4aa8d8` | filters, secondary curve |
| `--violet` | `#b98cff` | pan / spatialization |

Selection is always **amber**: it is the dominant visual signal, as on large-format consoles. Red is reserved for mute/clip, green for signal.

## Geometry (800×480)

```
┌──────────────────────────────────────────────┐
│ topbar                    34 px              │
├──────┬───────────────────────────────────────┤
│ soft │ screen                               │
│ 54px │ 746 × 414 px                         │
│ keys │                                      │
├──────┴───────────────────────────────────────┤
│ bottom bar                32 px              │
└──────────────────────────────────────────────┘
```

- **Topbar**: brand + 16 tabs + status LED, clock and sample rate (44.1k/48k). `CFG` sits next to `SYS`.
- **Soft keys**: 14 vertical keys (SEL, MUTE, SOLO, ◀, ▶, EQ, DYN, AUX, MAP, MON, MAC, DLY, CFG, HOME).
- **Screen**: page area with `.page-head` + panels.
- **Bottom bar**: selected-channel context and status messages.

## Components

### Channel strip
~56 px wide, up to 12 strips per bank. Layout: number, name, source, **draggable L/C/R mini-pan**, M/S, fader + meter, dB readout, GT/EQ/CP indicators. Selection = amber border + outer glow. The mini-pan is synchronized with the PAN knob in QUICK/CHANNEL views: `L6 … C … R6`, 0.5 steps, center dead-zone.

### Stereo master
In OVERVIEW the master is neither a strip nor a fader: it has a **large 68 px knob** with 270° travel, dB readout, two adjacent vertical meters (L and R) and M/S. The L/R meters are derived from the active input channels; the knob writes a dedicated master level, so SHOW CONFIG never leaves a fixed channel 23/24 behind.

### Fader
- 9 px track (16 px in the enlarged view), fader cap with metallic gradients.
- Amber fill from the base to the cap.
- Adjacent 7 px meter with a 90 ms transition (updated at ~10 Hz).
- `dB = (fader - 80) × 0.35`, with `-∞` at 0.

### Knob
34 px diameter, 270° travel (`-135° … +135°`), amber indicator (cyan/violet for filters and pan). Vertical drag: 90 px = full scale.

### Routing
Dynamic matrix (rows = active sources, columns = active destinations, max 16) navigated in blocks. Source blocks: INPUT, AUX, AES50 A/B, ULTRANET, CARD 1/2, USB, GROUP, FX RET, **MATRIX**. Destination blocks: XLR OUT, AUX OUT, AES50 A/B, ULTRANET, CARD 1/2, USB OUT, GROUP IN, **MATRIX IN**, DSP. This enables GROUP→GROUP, GROUP→AUX and MATRIX→GROUP. The patch count is shown in the header.

### Show config / Fader map
`SHOW CONFIG` defines which resources exist in the show (INPUT, AUX, FX, GROUP, MATRIX, XLR OUT) with counters, presets (DEFAULT/BAND/CHOIR/THEATRE/MIN), MONO/STEREO format and group/FX pre-post destinations. With **HIDE UNUSED** ON, disabled resources disappear from OVERVIEW, AUX, MONITOR, FX, METERS and ROUTING: this is what prevents operator mistakes.

`FADER MAP` handles **12 banks × 12 faders: 8 input banks on 2 rows of 4 (A01–A04, B01–B04) and 4 output banks on 1 row of 4 (O01–O04)**. By default the mapping is "straight": INPUT 1–24 → FX RET → GROUP → XLR OUT. **FREE ALL** clears everything; then SEL bank → SEL slot → SEL item from the INPUT/FX RET/GROUP/OUT list assigns the channel to the fader and advances to the next slot. **STRAIGHT** restores the default. The mapping is part of the scene snapshot (SAFE group `assign`).

### Clock
The SYSTEM panel exposes sample rate (44.1/48 kHz), source (INTERNAL, EXTERNAL, AES50 A/B, CARD 1/2) and video frame rate (24/25/30/50/60 fps). The rate updates the status bar; the fps recalculates the frames on the DELAY page.

### Delay / alignment
Two side-by-side lists (INPUT / OUTPUT) with 16 rows each: meters, milliseconds and frames. The bottom panel has a 0–100 ms knob and 0/1/5/10 ms presets. Conversions: `m = ms × 343 / 1000`, `frame = ms / (1000 / fps)`, with fps selected in SYSTEM (24/25/30/50/60).

### Mono / stereo / link and insert
In CHANNEL STRIP: `MONO`/`STEREO`, `LINK +1` (pairs the next channel and forces both to stereo), insert slot choice (OFF or one of the 4 FX) and `PRE EQ / POST EQ / PRE FADER / POST FADER` position. In FX: insert position and output `XLR / USB / CARD 1 / CARD 2`.

### Monitor audio
Selectable sources: LR, AUX 1–8, BUS 1–8, CARD 1/2, USB, FX RET 1–4. The LISTEN block has a level knob, L/R meters (or mono sum), DIM −20 dB, MONO, MUTE and PHONES 1/2 / CR SPK destinations.

### Dynamics and insert
On DYN you choose the gate type (`GATE / EXPANDER / DUCKER`) and compressor type (`VCA / FET / OPTICAL / TUBE`): the type shapes the drawn curve. The per-channel insert is a wide panel with MODE, FX slot and position, not an icon on the edges.

### Macro
Eight keys A1–A8. REC captures clicks, faders, knobs, pan and EQ nodes (macro/scene/clock management commands are excluded); PLAY replays the sequence at 130 ms per step with highlighting; CLEAR empties; SAVE persists in `localStorage`.

### Scene structure
Hierarchy **SHOW → BANK A/B/C/D → 8 slots = 32 scenes**. Each scene stores name, time, full snapshot (including SETUP and FADER MAP) and a SAFE map. STORE saves the current state; RECALL applies only groups not in SAFE; COPY/PASTE, PREV/NEXT, AUTO SAVE and DELETE complete the management. Persistence uses `localStorage` with graceful degradation on `file://`.

### AUX sends
- Only the buses enabled in SHOW CONFIG appear at the top; each has send faders for the active bank, PRE/POST, send mute and a bus mute/level control.
- A dedicated BUS MASTER fader and meter; the internal map toggles the sources that feed the bus.
- The external map selects destinations for the AUX bus and writes the same `state.patch` entries used by ROUTING, so the two pages cannot disagree.
- Each send has its own mute and `PRE`/`POST` mode; the individual send meter follows the channel level weighted by the send.
- The bus meter sums the 16 channels of the bank, applies the bus level and mute.
- Faders use the same dB scale as the main faders (`faderDb`).

### EQ / Dynamics
SVG with a fixed `viewBox` and curves generated in JS:
- logarithmic X axis 20 Hz – 20 kHz;
- 4 draggable nodes (frequency/gain), Q in knobs;
- per-band mode buttons: LOW CUT, LOW SHELF, PEQ (parametric), VEQ, HIGH SHELF, HIGH CUT; the drawn curve follows the selected mode;
- approximated low-cut response;
- gate/compressor curves with a dashed threshold line.

### Routing matrix
CSS grid `92px + N equal columns` (N = active destinations), clickable cells with an amber dot when patched; rows = active sources.

## Mapping large-format concepts → M32

| Large-format concept | Counterpart on M32 / OpenX32 |
|---|---|
| 17" multitouch screen | 800×480 display (drag + soft keys) |
| meterbridge + quick soft select | METERS page + soft-key column |
| 12 faders × layer/bank | 25 physical faders → 12 logical faders × 12 mappable banks (input 2×4 + output 1×4) + master |
| strip with integrated processor | DSP1 gate / EQ / comp already implemented |
| rack of plug-in slots | FX slots on DSP2 (reverb, chorus, delay, overdrive) |
| dedicated per-band encoders | M32 encoders + wheel, mapped in `producer_button_map.txt` |
| matrix routing | FPGA routing, 160 sources × 160 destinations |

## Accepted trade-offs

- No pixel-perfect copy: resolution and hardware are different.
- Meters at 10 Hz instead of the 30 fps UI: values change only when the parameter engine notifies a change.
- 16 pages instead of one giant screen: navigation via tabs/soft keys.
- Original colors and shapes: no copied assets or trademarks.
