# Wii Classic Controller enhancements for Canoe

This project provides a small wrapper library for the SNES Classic Mini's
Canoe emulator. It makes the **left analog stick** of a Wii Classic Controller
behave as an additional D-pad input in Canoe games. **Press ZL to pause both
emulation and sound; press ZL again to resume.** Holding ZL triggers only once.
**Hold ZR for fast forward capped at approximately 2×; release it for normal
speed.** The cap is 120 emulated frames/s for the verified 60 Hz game loop;
heavier games can run below the cap. Audio cuts during fast forward are expected.

The library is loaded alongside Canoe when it starts and adds this input
behavior without replacing the emulator or changing the original controller
mapping. On Linux, this loading technique is called `LD_PRELOAD`.

The original D-pad continues to work. Two controllers are handled independently, so controller 1's stick does not move player 2.

ZL on either controller controls the shared game pause. The shortcut calls
Canoe's native pause routine on the input thread, from the same library as the
analog mod. The emulator executable stays intact. Pause currently supports the
specific Canoe build listed below; other builds keep analog support only.
ZR replaces only the measured CPU framebuffer refresh wait with a paced wait.
The same library implements left-stick-to-D-pad input, ZL pause and ZR fast
forward. The separate CRT module also reads SDL controller-axis events, but
uses only the right stick to adjust filter coefficients; it does not map a
stick to gameplay directions. Load both libraries to use the full feature set.

The separate framebuffer-worker module in `modules/crt-controls/` works with
the configured scanline path: the
right stick adjusts two coefficients, saves them automatically, and ZL+ZR
restores the defaults. It does not modify Canoe on disk.

## Motivation

The Wii Classic Controller is an excellent official Nintendo controller for retro play: it has a familiar, compact layout, a proper D-pad, and useful analog sticks. Canoe recognizes it, but its SNES games normally only read the D-pad. The sticks therefore sit unused, even in games where using a stick as a comfortable way to express the same four directions feels natural today.

This project keeps the original game input model intact. It does **not** turn an SNES game into an analog-aware game; it maps the left stick to the same digital directions that a D-pad produces. That makes it useful for games that were never designed around analog movement while preserving their original controls, timing, menus, and behavior.

## What it changes

The library observes SDL controller-axis events and intercepts Canoe's calls to `SDL_GameControllerGetButton`. For D-pad buttons, it returns:

```
physical D-pad pressed OR virtual D-pad direction from left stick
```

It uses separate hysteresis thresholds to avoid flicker near the stick centre:

| Transition | Absolute axis value |
| --- | ---: |
| Press direction | > 18,000 |
| Release direction | < 12,000 |

The mapping is SDL axis 0 = horizontal and axis 1 = vertical. Diagonals work because horizontal and vertical directions are tracked independently.

## Repository layout

- `modules/controller-controls/` contains left-stick-to-D-pad mapping, ZL
  pause, ZR fast-forward, their tests, module README and device install/upgrade
  scripts.
- `modules/crt-controls/` contains the independent CRT coefficient controls,
  settings persistence, reset chord and its tests.
- Each module owns its `src/`, `tests/`, `scripts/` and generated `dist/`.
- Root `scripts/build-all.sh` and `scripts/test-all.sh` run the corresponding
  operation for both modules.

Build and test both modules from the repository root:

```sh
ZIG=/absolute/path/to/zig ./scripts/build-all.sh
./scripts/test-all.sh
```

Each module's `scripts/build.sh` and `scripts/test.sh` can also be run alone.

The previously Mini-tested ZL/ZR library had SHA-256:

```
47cfaf7e0633e38c4ffb408f1843c2a23c553a88a6d1d80bd8651aafd12112dc
```

The original analog-only library has SHA-256
`faef005d9a1d8453f6df625b4456b93eba81bb88ad42ba535c16af0220aad098`.
The first ZL-only update had hash
`a1ed2cf5ef45a43631afa06a9aa70509f17a88c3817bb8e97430918a1793502d`.
The controller-controls module's Mini-tested library has SHA-256
`47cfaf7e0633e38c4ffb408f1843c2a23c553a88a6d1d80bd8651aafd12112dc` and
matches the build tested on the Mini. The CRT module has a separate binary,
built by its own module script. Root build commands compile both modules.
Rebuilds can change the hash because of toolchain metadata; verify the build and
checksum before installing.

## License

This project is licensed under the [MIT License](LICENSE). If you use or
redistribute the project, including a modified version, keep the original
copyright and license notices.

## Requirements

- SNES Classic Mini with hakchi/SSH access.
- Canoe launcher at `/usr/bin/clover-canoe-shvc`.
- Wii Classic Controller(s) recognized by Canoe through SDL.

For native pause and fast forward, `/usr/bin/canoe-shvc` must have SHA-256
`30d0768623e875dc8f3de334ccc7193807878c8ad9b50c38bec362deb4245a61`
(version `fc349ac43140141277d2d6f964c1ee361fcd20ca`). The upgrade script checks
the full file hash. Runtime additionally checks file identity, ELF load layout,
instruction signatures and the active game object before calling a private
routine. Analog input still works if pause is unavailable on another build.
Fast forward additionally requires the verified CPU framebuffer path on the
main thread. Other rendering paths retain their original pacing.

This does not remap controls system-wide and does not affect RetroArch. RetroArch already has its own analog input handling.

## Download

GitHub Releases do not have a published build yet. The current library in
`modules/controller-controls/dist/` includes analog input, ZL pause and capped
ZR fast forward. The separately built CRT worker module is documented under
`modules/crt-controls/`. The unverified OpenGL scanline prototype is kept on
the separate `codex/experimental-opengl-scanlines` branch.

## Install

1. Use `modules/controller-controls/dist/libcanoe_analog_state.so` for analog support, ZL pause and capped ZR fast forward. The CRT controls are a separate module under `modules/crt-controls/`.
2. Copy it to `/var/lib/hakchi/libcanoe_analog_state.so` on the Mini.
3. Copy `modules/controller-controls/scripts/install.sh` to the Mini and execute it as root.
4. Reboot the Mini.
5. Test one controller, then two controllers in a simultaneous two-player Canoe game.

`install.sh` makes a timestamped backup of the Canoe wrapper before changing it. It preserves any existing options or prior fixes in that wrapper. It recognizes both `exec canoe-shvc ...` and `exec /usr/bin/canoe-shvc ...` launcher formats.

Before installing, you can safely validate the detected Canoe launch line without
changing any files. This also validates an already-configured launcher, but does
not reinstall it:

```sh
sh /tmp/install.sh --dry-run
```

If the analog mod is already installed, exit to the Mini's menu, stage the new
library in `/tmp`, and use the checked upgrade instead of overwriting a library
that a running game may have mapped:

```sh
sh upgrade-controls.sh /tmp/libcanoe_controls.new.so 47cfaf7e0633e38c4ffb408f1843c2a23c553a88a6d1d80bd8651aafd12112dc
```

The script prints the backup directory and preserves the launcher. Open a game
again to load the updated library. No extra resident program is needed.

## Verify on the Mini

```sh
sha256sum /var/lib/hakchi/libcanoe_analog_state.so
tail -n 8 /usr/bin/clover-canoe-shvc
```

The wrapper must export the library before it executes `canoe-shvc`:

```sh
if [ -r /var/lib/hakchi/libcanoe_analog_state.so ]; then
    export LD_PRELOAD=/var/lib/hakchi/libcanoe_analog_state.so
fi
exec canoe-shvc $options $log
```

## Reversal

To undo only the controls upgrade and keep the previous analog mod, exit the
game and run `sh restore-controls.sh BACKUP_DIRECTORY`, using the backup path
printed by the upgrade script.

Run `modules/controller-controls/scripts/uninstall.sh` on the Mini. It restores the most recent backup created by the installer and removes the library.

## Development notes

The [controller input map](docs/CONTROLLER-MAPPING.md) records the 2026-10-24
button and stick capture, including how ZL/ZR appear as SDL trigger axes.
The [Mini validation guide](docs/MINI-VALIDATION.md) documents how to repeat
hardware tests and preserve useful findings without committing raw logs.
The [future modular input design](docs/FUTURE-MODULAR-INPUT.md) records a
possible shared controller core and optional feature modules.
The [ZL pause implementation notes](docs/ZL-PAUSE.md) describe the native call,
version checks, diagnostics and validation.
The [fast-forward investigation](docs/FAST-FORWARD.md) demonstrated the native
pacing constraint. The [ZR implementation](docs/ZR-FAST-FORWARD.md) adds a
120 fps ceiling rather than unlimited speed.
The separate CRT worker controls and settings tests are in
[`modules/crt-controls/`](modules/crt-controls/).

The work began by observing Canoe's SDL input path with an ARM `LD_PRELOAD` probe. The probe established that the Wii Classic Controller uses SDL axis 0 for horizontal movement and axis 1 for vertical movement, while Canoe requests D-pad buttons 11–14 (up, down, left, right).

The implementation originally tried synthesizing SDL events, but Canoe polls `SDL_GameControllerGetButton` instead. The final version tracks stick state in `SDL_PollEvent` and merges it at the button-query boundary.

SDL assigns a joystick instance ID to each controller. State is keyed on this ID, not globally, so each controller's analog stick remains associated with its own player.

## Tested outcome

The original analog-only release was tested as follows:

- Wii Classic Controller: all four analog directions, D-pad, face buttons, and diagonals work.
- Two identical Wii Classic Controllers: each analog stick controls only its own player.
- Original SNES controller: remains working.
- Persistence: works after an SNES Mini reboot.

For the first ZL update, input-state tests covered press/hold/release, ZR remaining free,
independent controller IDs, reconnection, analog hysteresis/diagonals and
unchanged physical-button/event handling. On the Mini, 43 ZL pause/resume pairs
were recorded in Mario with matching emulation frame counters and both pause
flags changing together. The user confirmed image/audio pause, repeated presses
and combinations with other buttons. A later combined build retained pause and
fast-forward behavior after a Mini reboot. The combined build has not been
revalidated with two controllers at once.

For the ZR update, input tests additionally cover either controller holding ZR,
release, hysteresis and disconnect. Pacing tests exercise 10,000 fast frames,
slow frames, interrupted waits, clock/sleep failures and no catch-up bursts.
On the Mini, the real-clock pacing test with zero emulation workload measured
119.003 fps, below the 120 fps ceiling. Mario measured approximately 118.9 fps
(1.98×) while holding ZR, returned to 60 fps after release, and retained working
ZL pause/resume. The user confirmed the behavior. See the ZR implementation
notes for the evidence and remaining validation scope.

The CRT worker module was tested on the Mini with held-stick repeats,
automatic settings persistence and the ZL+ZR reset chord.
Host tests cover configuration validation, loading, delayed writes and reset
writes; they do not emulate Canoe's rendering path.
