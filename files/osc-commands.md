# OSC scene and existing recorder commands

Send to UDP 10032; replies go to the sender IPv4 address on port 10033.

`/scene/save/<slot>` and `/scene/load/<slot>` take no arguments, slot 0..9999.
They operate on the server process's sceneN.json, not the GUI client's local
filesystem. Slot 0 is also used by the existing startup/autosave mechanism.
Replies `/result/scene/<save|load>/<slot> ,s {"success":bool,"message":string}`
report actual serialization/file/parse/array validation outcomes. A failed
parse/validation does not partially apply a scene. Saves use temporary-file
replacement so write failure does not truncate an existing scene. Loading is
controller state restoration, not external DSP acknowledgement.

Supported recorder paths are `/recorder/stop/1`, `/recorder/play_pause/1`,
`/recorder/record/1` (no arguments), `/recorder/seek/1 ,i <sample index>`
(nonnegative int32 samples, not seconds). Replies use
`/result/recorder/<command>/1 ,s {"success":bool,"message":string}`.
These call the existing Card::XLIVE_* methods only with a detected X-LIVE card,
not on bodyless/raspi/WING. Others get unsupported_no_xlive_card.

The result truthfully reports the legacy backend return, not guaranteed
device acknowledgement (Stop in particular can return true without one).
Successful messages explicitly say backend_returned_true_not_audio_confirmation.
No format/delete, transport state engine, generic stereo USB/file recorder,
TOC/session UI or fabricated two-track engine is added. Existing card flags
and position feedback retain their known limitations.

Capability names: scene_save, scene_load; recorder_xlive_transport only with
the supported detected card. Config::Save now returns bool; existing callers
may continue ignoring its result.
