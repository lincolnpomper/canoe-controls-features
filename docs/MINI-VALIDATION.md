# Testing changes on an SNES Classic Mini

This guide records the hardware-testing workflow used for the controller and
Canoe changes. It is intended for maintainers and coding agents continuing the
work. Raw event captures and per-run reports are deliberately not kept in the
repository; keep concise results, relevant hashes, and unresolved limitations.

## Before changing the Mini

1. Read the feature notes and inspect the current branch, build scripts, and
   tests. Run `bash scripts/test.sh` before a device session.
2. Identify the exact Canoe executable and installed library. Native pause and
   fast forward are guarded for the Canoe SHA-256 documented in `README.md`;
   do not infer compatibility from a version label alone.
3. Confirm the Mini is at its Home menu with no game running before replacing a
   library or launcher. Make a dated Mac-side backup of the current library and
   launcher, record their hashes, and use the checked upgrade/restore scripts.
4. Build the default library, record its SHA-256, and compare it with the
   intended build before copying it to the Mini. Keep experimental hooks out of
   the default build unless their behavior has been verified on hardware.
5. Keep the physical controller available. Do not leave a virtual-input helper
   enabled between tests: it can take ownership of input and prevent the
   physical controller from working. If a temporary observer or helper is
   installed, record how it is disabled and verify the original launcher is
   restored before ending the session.

## Capturing controller mappings

Use a passive SDL observer that records device mappings and button/axis events
without consuming or synthesizing input. Start with the game open, ask the
player to press one control at a time, and let it return to neutral between
presses. Record the physical control, SDL control name, raw button/axis number,
and representative released/pressed values. Capture analog directions
separately and return the stick to center between directions. Record the device
name, but treat controller indexes and instance IDs as runtime values; never
hardcode them.

Compare observations with the existing
[controller map](CONTROLLER-MAPPING.md). After capture, disable the observer,
restore any changed launcher, and verify ordinary physical controls still
work. Put the distilled mapping in the documentation; raw event streams are
temporary diagnostics.

## Testing pause on ZL

Use a game with visible motion or continuous audio. Record the installed
library hash and the starting Canoe frame counter and pause flags. Press and
release ZL once, then leave the controller untouched long enough to distinguish
a real pause from a short delay. Confirm video and audio stop, the frame
counter stays fixed, and both Canoe pause flags indicate paused. Press and
release ZL again; confirm both flags clear, audio/video resume, and the frame
counter advances. Repeat with a held press and with another gameplay direction
held while paused. A held press must toggle only once, and current gameplay
input should apply after resume.

These checks establish observable behavior and state consistency. An event log
alone does not prove the player saw or heard the pause.

## Testing fast forward on ZR

Use a game scene with continuous motion or a known-running frame counter. First
measure normal emulation for several seconds; the verified game loop is about
60 emulated frames per second. Hold ZR for around five seconds and measure
again. The target is near 120 emulated frames per second (about 2×), not the
maximum the hardware can produce. Release ZR and confirm normal pacing returns.
Also test ZL while ZR is held: pause must take precedence, and capped pacing may
resume after unpausing if ZR is still held.

Keep the first hardware trial short. If the game hangs, audio/video fail to
recover, or the measured rate exceeds the intended cap, release the button and
return to Home. Use the documented rollback procedure before further testing.
Do not use an unlimited-speed experiment as a substitute for the capped test.

## Recording results

For each hardware-validated change, keep a short summary in the relevant
feature note: tested binary and Canoe hashes, game/scene, test steps, measured
result, observed audio/video behavior, rollback availability, and what was not
tested. Keep exact commands or test data only when another maintainer needs
them to reproduce a measurement. Avoid committing raw logs, process IDs,
timestamps, personal machine paths, or controller-session transcripts when a
brief result preserves the useful finding.

Run `bash scripts/test.sh` after code changes and before each device session.
It covers host-side input, pacing, and CRT-settings behavior; it does not
replace the Canoe hardware checks above.
