# Native Canoe pause on ZL

## Behavior

Press and release ZL to pause the game and its audio. Press it again to resume.
Holding the button does not repeat the operation. ZR now provides
[capped fast forward](ZR-FAST-FORWARD.md). Either
controller can toggle the shared emulation pause; the held-button latch belongs
to each joystick instance independently.

The existing left-stick D-pad behavior is preserved. A controller removal clears
its stored directions and ZL latch, freeing the slot for future connections.

## Implementation

`modules/controller-controls/src/canoe_analog_state.c` observes SDL controller
axis 4 (`LEFTTRIGGER`), which the capture confirmed as physical ZL. The
recorded values are 0 released and
32767 pressed. The same 18000/12000 hysteresis used for the analog input arms
one action per press. On that edge, it calls `canoe_pause_toggle(instance_id)`
from `modules/controller-controls/src/canoe_pause.c` on the SDL polling thread.

The integration calls Canoe's original ARM routine at `0x2CAC4` with `!PAUSED`.
It reads Canoe's own current flag at `0x1E210C` instead of maintaining a separate
notion of pause. The native routine also invokes the active object's audio pause
method at `0xE3F04`, which updates the audio callback's flag at `0x1DFFC4`.

No code trampoline, instruction replacement, timer, process suspension or
second resident binary is involved. The pause, input and newer fast-forward
source files compile into the same
`libcanoe_analog_state.so`, loaded through the existing launcher.

## Supported executable

- SHA-256: `30d0768623e875dc8f3de334ccc7193807878c8ad9b50c38bec362deb4245a61`.
- Version: `fc349ac43140141277d2d6f964c1ee361fcd20ca`.
- Fixed-address ARM ELF, 1,836,012 bytes.

The upgrade script checks the complete executable hash. The runtime guard
checks `/proc/self/exe`, file size, the main executable's two mapped load
segments, and instruction signatures for both pause routines before accessing
private data. Each toggle additionally requires game-loop state 3, the known
active object/vtable and audio method, and valid boolean flags. An unrecognized
runtime profile disables the native pause feature while retaining analog input.
Runtime signatures are a compatibility check, not a replacement for the full
installer hash check.

This profile is deliberately narrow. Additional Canoe builds need their own
verified addresses and checks before native pause can be enabled on them.

## Build and tests

```sh
ZIG=/absolute/path/to/zig bash scripts/build-all.sh
bash scripts/test-all.sh
```

The production target is ARM hard-float Linux with glibc 2.17, Cortex-A7, ARM
instruction mode. Input tests run natively with address/undefined-behavior
sanitizers and mocked SDL calls. They check held/jittering ZL, release/repress,
ZR isolation, multiple controller IDs, repeated reconnects, existing analog
hysteresis and diagonals, physical buttons and event preservation.

The controller-controls Windows build script is at
`modules/controller-controls/scripts/build.ps1`; it has not been run on
Windows during this Mac session.

## Installation and rollback on an SNES Classic Mini

The initial ZL-only build was installed on 2026-10-24 through the
controller-controls module's upgrade script, while the Mini was in its menu.
The replacement was staged, checked, smoke-loaded and atomically moved into
place. The launcher was preserved.

- Updated library SHA-256: `a1ed2cf5ef45a43631afa06a9aa70509f17a88c3817bb8e97430918a1793502d`.
- Previous analog-only library SHA-256: `faef005d9a1d8453f6df625b4456b93eba81bb88ad42ba535c16af0220aad098`.
- Console backup: `/var/lib/hakchi/canoe-controls-backups/19700101-014333`.
- Local backup: the analog-only library preserved before the pause update.

The later ZR build supersedes this library and preserves it in a separate
backup; see the ZR implementation notes for the current binary hash and rollback.

The console clock was unset, so its backup directory name shows 1970; the
installation date above comes from the Mac session. To restore the previous
library, exit the game and run on the Mini:

```sh
sh /var/lib/hakchi/canoe-controls-backups/19700101-014333/restore-controls.sh /var/lib/hakchi/canoe-controls-backups/19700101-014333
```

## Runtime evidence

The updated library loaded in Mario as PID 32696, with `supported=1`. **43
complete ZL pause/resume pairs** were recorded on the main/input thread (TID
32696), with matching frame counters at each pause and its corresponding resume.
Their measured durations ranged from 116 to 2483 ms. Both global and audio pause
flags changed to 1 on pause and 0 on resume in every pair. The only mod library
mapped was the updated analog/controls library; the input probe and earlier
pause observer were absent. The game subsequently exited normally to state 5.

The user confirmed that the feature worked perfectly, including many repeated
ZL presses and using ZL while holding other buttons. The user also paused, held
Right, and resumed: Mario then moved Right. This agrees with the intended native
pause behavior: controller polling remains active while emulation stops, and
gameplay resumes using current input state. It does not queue gameplay frames
while paused.

The repeatable hardware-testing procedure is in
[`MINI-VALIDATION.md`](MINI-VALIDATION.md). The summarized results above are
kept; raw event logs and per-run installation records are not needed to
understand or reproduce the test.

The running library records startup and requested actions in
`/tmp/canoe-controls.log`, capped at approximately 64 KiB across game launches
until reboot. The log is optional for operation: inability to open/write it
does not prevent the shortcut from working. The hardware-testing procedure is
documented in `MINI-VALIDATION.md`; temporary diagnostics are not required by
the installed mod.

The read-only status helper used during the test sampled the frame counter and
flags through `/proc/PID/mem`. It was not loaded into Canoe and is not required
by the installed mod.

This update has not yet been verified on hardware with two controllers or after
a reboot. Input-state tests cover two-controller independence, and the earlier
analog-only release already had hardware validation for both cases.
