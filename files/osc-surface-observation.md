# Non-owning OSC surface observation

`/subscribe/surface/1 ,f 1|0` enables/disables observing all defined surface
elements. A same-path float acknowledgement reports whether it is enabled.
Send UDP to 10032 and listen on 10033 at the same IPv4 address. Renew every
five seconds; subscriptions expire after ten seconds and are capped at sixteen
IPs. The capability name is `surface_subscription`.

Events: `/press/<element>/1 ,f 1|0` for press/release,
`/turn/<element>/1 ,f <signed steps>`, `/move/<element>/1 ,f <dB>`.
The element name is its lowercase surface label, replacing runs of other
characters with `_` (no leading/trailing underscore), matching the separate
local claim extension's naming convention.

The observer runs before normal processing and does not change bindings,
consume events, claim controls or drive LEDs. Faders/encoders are observed even
if unbound. Receiving an event is not an instruction to echo a parameter write:
OMC still handles the action. No-GUI/claim/LED ownership are separate features.
Only elements defined for the current model can generate notifications.
