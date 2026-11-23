# Review package — sent to OpenX32

The proposal was published on 22 November 2026 and then transferred by the maintainers to [Ticketsystem issue #11](https://github.com/OpenMixerProject/Ticketsystem/issues/11) (the former OpenX32 issue #142). It is open, assigned to `jonniavid`, type Feature.

Maintainer feedback:
- "I really like the accent color!"
- Asked whether EEZ Studio screens and LVGL bindings can be prepared on a fork of `OpenMixerControl`: "Yes, you can prepare that. In Future we want to have OMC as a real client as a separate process on the X32."

Because GitHub issues do not accept ZIP attachments and this account has read-only access to `OpenMixerProject/OpenX32`, the complete package is hosted as a release asset under the author's account:

- Source repository: https://github.com/jonniavid/openx32-quantum-review
- Release v19: https://github.com/jonniavid/openx32-quantum-review/releases/tag/v19
- Direct ZIP: https://github.com/jonniavid/openx32-quantum-review/releases/download/v19/openx32-quantum-review.zip

The forum draft was submitted manually by the author in the `Behringer X32 and M32` category; it was still awaiting moderation at the last check. Nothing was posted to `OpenMixerControl`.

## What was sent

- `index.html`, `styles.css`, `app.js` — the 800×480 prototype, opens from `file://` with zero dependencies.
- `README.md` — quick start and feature list.
- `docs/DESIGN.md` — UI specification.
- `docs/PORTING.md` — OpenMixerControl / EEZ Studio / LVGL 9.5 porting plan.
- `docs/PROPOSAL.md`, `docs/FORUM_POST.md`, `docs/SUBMISSION.md` — proposal and message drafts.
- `screenshots/` — 800×480 PNGs: `overview`, `setup`, `assign`, `routing`, `scenes`, `monitor`, `eq`, `aux`.

## Message used in the issue

> Hello, I prepared a small, dependency-free 800×480 UI prototype for the M32/X32 form factor. The idea is to define the show first (SHOW CONFIG) and then make fader banks, routing, AUX maps and scenes follow only the resources that actually exist. It is not firmware: it is a navigable specification plus a porting plan. If the direction is useful, I would be glad to adapt it inside OpenMixerControl, and maintainers are welcome to change or discard anything. Thank you for the feedback.

## Review path (30 seconds)

1. Open `index.html` directly in a browser.
2. `CFG` → `MIN` → `OVER`: only the active inputs appear.
3. `MAP` → `FREE ALL` → `OVER`: no faders; then assign one from the OUT list.
4. `AUX` → toggle internal sources and an external destination; check `ROUTE` for the same patch.
5. `EQ` → change a band mode; the curve follows.
6. `ROUTE` → try `GROUP → GROUP IN` and `MATRIX → GROUP IN`.
7. `SCENE` → STORE / RECALL: SETUP and FADER MAP return, except SAFE groups.

## Checks already performed locally

- `node --check app.js` passes.
- All 16 pages load with no JavaScript errors in headless Chromium.
- Every page keeps `#screen` at `744×412` with no overflow at the 800×480 framebuffer.
- Screenshots were regenerated at `800×480` from version tag `v=19`.
- No DiGiCo logos, artwork, icons or protected assets; wording is English and "Quantum UI" is not shown.
