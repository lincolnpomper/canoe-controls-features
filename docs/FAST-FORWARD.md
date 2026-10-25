# Fast-forward investigation for ZR

This document preserves the initial unlimited proof. The subsequent
[production ZR implementation](ZR-FAST-FORWARD.md) is installed and validated
with a 120 fps ceiling (approximately 2×), as requested by the user.

## Result

On 2026-10-24, a temporary module demonstrated real **approximately 2.06× fast
forward in Mario**, loaded from a saved state on an SNES Classic Mini. It omitted
one framebuffer vertical-refresh wait for three seconds and then restored the
original wait automatically. The production analog/ZL library was unchanged.

| Trial | Duration | Emulated frames | Effective rate | Relative to 60 fps | After test |
| --- | ---: | ---: | ---: | ---: | ---: |
| First | 3017 ms | 372 | 123.30 fps | 2.055× | 60 fps |
| Repeated for user observation | 3000 ms | 371 | 123.67 fps | 2.061× | 60 fps |

The steady intervals during acceleration reached 124 emulated frames per second.
The user confirmed visible acceleration and reported audio cuts, familiar from
fast forward in other emulators and acceptable for this use. The user also
reported many video frames; no separate measurement of tearing or dropped
display frames was performed. These rates measure Canoe's emulation-step
counter, not the number of distinct images the television displayed.

## What actually limits the tested configuration

Runtime measurements confirmed `cpu_render=1` and `lowlatency=1`, with the main
thread using the framebuffer renderer. Its presentation routine at `0x16233C`
issues an `ioctl(fd, 0x40044620, ...)` at `0x1626AC`, returning to `0x1626B0`.
This request is `FBIO_WAITFORVSYNC`, as defined in the Linux `linux/fb.h` header
bundled with the local Zig toolchain. In the normal run, that call occurred 60
times per second and spent approximately 0.51 seconds per wall-clock second
waiting. Skipping this wait removed a pacing constraint.

The test retained buffer updates, rendering work and all semaphore waits. The
semaphores synchronize worker tasks and cannot be treated as equivalent to the
vertical-refresh wait. No changes to Canoe instructions, private function
replacement or alteration of its clock sources were needed.

OpenGL swap interval is configured elsewhere in Canoe, but disabling it would
not address the framebuffer wait active in this test. Other display paths have
not been dynamically tested.

## Temporary proof

The initial probe was kept in a separate local workspace. Its shared library
interposed `ioctl`, SDL semaphore/presentation functions, and `SDL_PollEvent`,
alongside the existing analog/ZL mod. Timing was aggregated by thread, caller
and request or synchronization object.

The experiment only returned success without the real call when all of these
conditions held: request `0x40044620`, exact caller `0x1626B0`, game state 3,
not paused, and the three-second monotonic deadline still in the future. Every
other call was forwarded unchanged. The deadline was also checked inside the
`ioctl` wrapper, so skipping could not continue merely because input polling
was delayed. Logs confirmed skipped calls during the experiment and native
waits again afterward.

The supported Canoe SHA-256 remains
`30d0768623e875dc8f3de334ccc7193807878c8ad9b50c38bec362deb4245a61`.
Library file identity, mapped layout and instruction signatures were checked
before private-state reads. The probe library SHA-256 was
`6d85ed4ab0425aa29571192f8a27f4b6881319cec4e2e51b86cffc772ef1500f`.

## ZR integration following the proof

The earlier [controller capture](CONTROLLER-MAPPING.md) identified ZR as SDL
controller axis 5 (`RIGHTTRIGGER`), with values 0 released and 32767 pressed.
The implemented behavior is **hold ZR to fast forward, release it to return to
native pacing**. See the production notes for the later capped implementation.

The production implementation puts the narrowly guarded `ioctl` wrapper and ZR
held state into the same `.so` that handles analog input and ZL pause, with
per-controller held state, disconnect handling, ZL precedence and native pacing
whenever the game state or compatibility checks fail. Acceleration is requested
while either controller holds ZR.

Leaving the original audio callback in place preserves the cuts heard and
accepted during the proof. Audio is produced faster than its normal output
cadence; any later muting, discard or resampling option requires separate
buffer-handling work. This experiment did not implement such an option.

The initial proof's measured maximum was not a fixed 2× mode: it depended on the
game, rendering filter and remaining CPU work. The later production build adds
a clock-based 120 fps ceiling. Heavy games can run below that cap; the mode is
not allowed to use all available speed in lighter games. Longer use, home
interactions and two-controller hardware behavior remain outside the current
hardware validation scope.

## Evidence and cleanup

The repeatable hardware-testing procedure is in
[`MINI-VALIDATION.md`](MINI-VALIDATION.md). The summarized results above are
retained; raw logs and per-run captures are not required to understand or
reproduce the production behavior. The launcher was restored to its previous
hash, and the production analog/ZL library still has SHA-256
`a1ed2cf5ef45a43631afa06a9aa70509f17a88c3817bb8e97430918a1793502d`.

No further experiments are scheduled. The temporary module will not load into
new games. A game already running with it keeps the passive diagnostic code
until it exits; the temporary files disappear on reboot.
