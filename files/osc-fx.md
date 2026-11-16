# OSC FX and oscillator

All registered FX parameters have the lowercase spelling of their canonical
MP_ID enum name as their OSC path, including `fx_type`. Examples:
`/set/fx_type/1 ,f 1`, `/set/fx_reverb_rt60/1 ,f 3`.
Indices are slots 1–8. Native enum values, units, ranges and the upstream
`multibandcompresor` spelling are preserved. The enum sentinel is not a type.
Defeedback/upmixer type selection does not establish a working parameterized
algorithm. No new audio processing, bypass, mode, presets or inserts are added.

`osc_frequency` (20–20000 Hz, default 1000) and `osc_level` (-120–0 dB,
default -120) have two instances and use ordinary set/change/reset OSC commands.
They are persisted by the existing scene catalog and Mixer::Sync dispatches
them to the existing DSP2_SetOscillator method. The DSP sender's peer values
start initialized. No waveform, noise, enable or destination control is claimed.
-120 dB is attenuation, not an exact digital mute. DSP firmware/audio behavior
requires hardware validation; controller dispatch alone is not that validation.

Run `omc --test` to exercise the canonical FX catalog and both oscillator
values through Mixer::Sync in bodyless mode.
