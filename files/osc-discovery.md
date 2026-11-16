# OSC discovery and state

Send to UDP 10032; listen on UDP 10033 on the same IPv4 address.

* `/get/topology/1` (no arguments): `/set/topology/1 ,s <JSON>`.
* `/get/state/1` (no arguments): paced current-value snapshot as ordinary
  `/set/<OSC-name>/<one-based-instance> ,f|s` messages, ending with
  `/state/complete/1 ,f 1`. Up to 32 values per control-loop iteration.
* `/subscribe/state/1 ,f 1|0`: enable/disable changes, same-path float ack;
  first enable also requests a snapshot. Renew at least every five seconds.
  The lease lasts ten seconds; at most sixteen IPs may subscribe/request a
  snapshot. Requests/metadata/snapshot cursors are bounded per client.
* `/get/metadata/1`: `/set/metadata/<name>/1 ,s <JSON>` per exposed parameter,
  ending `/metadata/complete/1 ,f 1`. Fields: name, type (`number`/`string`),
  instances, min/max (null for strings), readonly, audio_status.

UDP snapshots are neither reliable nor atomic; completion is not a transaction
or a DSP acknowledgement. Re-request to repair packet loss. Listen before
requesting. Renewing a subscription does not restart its initial snapshot.
State queries have no arguments, not a dummy float.
Explicit state queries restart the snapshot cursor; do not send them on each
lease renewal. A UI may renew every four seconds and wait sixty seconds before
retrying an incomplete snapshot rather than starving a slow transfer.
Metadata and state use independent cursors: requesting both preserves both
completion markers, with metadata sent first under the same pacing budget.

Topology describes the controller catalog, not attached/working audio hardware.
`strips` entries are `{kind,name,start,count,eq_bands}`. Starts are zero-based
virtual strip indices; input=0/32, aux=32/8, fx=40/8, bus=48/16, matrix=64/6,
mono/sub=71/1, dca=72/8, main LR=80/1. Index 70 is reserved. Four EQ bands
(`1`..`4`) are advertised only for input/aux. Counts: fx_slots=8, sends=16,
matrices=6, dcas=8, mute_groups=6. Top-level sources/outputs describe zero-based
port groups; these are NOT native DSP routing selectors or strip IDs.
`colors` advertises the CHANNEL_COLOR enum palette in order: black, red,
green, yellow, blue, magenta, cyan, white (hex RGB strings). These are eight
OMC colors, not twelve WING colors. Send the palette index without translation
only when rendering from this advertised palette.

`routing` describes authoritative native index spaces. Each of channels,
outputs, effects, sockets has `{osc,sources:[{name,start,count}],dests:[...]}`.
Sources use actual selector values (Off=0 is excluded), destinations use
zero-based OSC instances (add one to construct a path). channels route_dsp_in
destinations are 0..39; outputs route_dsp_out are 0..39 (delay capable);
effects route_dsp_out are 40..63 (not delay capable). All three use native DSP
sources 1..92, with separate main L/R, FX inserts and DSP2 aux sources, NOT
virtual strip IDs. sockets route_fpga sources are 1..160; destinations 0..207.
The stored destination catalog includes AES50 B but the source catalog does
not. Require `osc` in capabilities before writing: descriptors alone do not
create OSC registrations absent from the build. Source value 0 disconnects.
Top-level `fanout:true` explicitly permits repeated sources across destinations;
each destination still selects one source. Writing a route never unpatches other
destinations using that source.

Capabilities list actual exposed OSC names plus implemented API feature names.
Other modules may register API capabilities in Config::oscCapabilities.
The compatibility aliases ch_sub_level, ch_eq_1type..4type, dcaN_member,
mute_groupN_member, mute_groupN_mute are added only if their underlying
parameter already has an OSC registration. Existing aliases are preserved;
snapshots/changes send both spellings. This complements existing expansion
PRs rather than adding their parameter registrations again.

Metadata `state_only_no_dispatch` explicitly flags polarity and gate,
compressor, lowcut enable flags: their DSP dispatch is absent in this baseline.
Monitor volume has a sender with no caller; monitor tap has no consumer.
Those are controller-side omissions, not merely uncertain DSP firmware.
`hardware_dependent` means controller state only: no audible promise, no
per-strip guarantee (EQ is input/aux only; DCA mute/solo are not implemented;
sub independent solo is incomplete). External DSP/FPGA correctness, FX audio,
send mute/tap semantics and physical ports still require hardware validation.
