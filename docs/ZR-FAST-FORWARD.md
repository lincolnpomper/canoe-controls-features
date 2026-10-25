# ZR fast forward with a 2× ceiling

## Behavior

Hold ZR to accelerate and release it to restore native pacing. The verified
game loop normally runs at 60 emulated frames/s; fast forward is capped at
120 frames/s. A heavy game can run below that ceiling. This is a paced mode,
not an unlimited-speed option. No game speed or timer data is rewritten.

ZL still toggles native emulation/audio pause. While paused, the original screen
wait is used even if ZR remains held. After resuming, a still-held ZR can
accelerate again. Either controller can request fast forward; one player's
release does not cancel another player's held ZR. Disconnect clears that
controller's request. A new game process begins with no buttons held.

Audio keeps Canoe's original callback and can have cuts during acceleration,
as the user observed and accepted in the earlier proof.

## Implementation

The [initial investigation](FAST-FORWARD.md) established that skipping the CPU
framebuffer `FBIO_WAITFORVSYNC` call allows approximately 2.06× in Mario. The
production implementation replaces only that wait with a capped frame interval:

- `canoe_analog_state.c`: SDL axis 5, hysteresis, per-controller ZR latch and
  aggregate held state.
- `canoe_fast_forward.c`: forwards every other `ioctl` unchanged. It requires
  request `0x40044620`, exact caller `0x1626B0`, the main thread, verified
  instructions, the known game object/state, CPU rendering, low-latency mode
  and an unpaused game before using accelerated pacing.
- `frame_pacer.c`: enforces at least **8,333,334 ns** between completed fast
  presentation waits, using `CLOCK_MONOTONIC` and `nanosleep`. Interrupted waits
  are rechecked against the deadline. Every interval starts from the actual
  completion time; slow or delayed frames never build up catch-up debt.
- `canoe_pause.c`: provides the existing executable/object checks and log.

Failure of the profile, game-state, clock or wait checks returns to the original
framebuffer wait. Releasing the last ZR or taking the paused/non-game path resets
the pacing timestamp. The initial fast frame establishes a fresh timestamp,
rather than waiting against a deadline left over from a previous hold.

All four source files compile into the same `libcanoe_analog_state.so`. No
resident helper, temporary observer, code patch or new launcher entry is needed.
The executable hash/profile is the same one documented for ZL.

## Validation

The native input tests passed with address/undefined-behavior sanitizers:
existing analog hysteresis/diagonals and physical buttons, ZL press/hold/release,
separate controller IDs, ZR release/hysteresis, either controller still holding
ZR, and disconnection/reconnection.

The pacing tests passed over **10,000 fast frames**, including alternating
workloads faster than the cap, an overloaded frame, no catch-up bursts,
interrupted sleep, clock/sleep failures and timestamp reset.

On the Mini, a separate real-clock executable exercised the exact production
pacer with **zero emulation workload**. 360 intervals took 3,025,127,805 ns,
yielding **119.003 fps**, below the 120 fps ceiling. This checks the cap in the
case where workload would otherwise permit much higher speed; the helper is
not required by the installed mod.

The updated library was verified in Mario (PID 9093), without the temporary
diagnostic probe:

| ZR hold | Duration | Emulated frames | Effective rate |
| --- | ---: | ---: | ---: |
| Short hold | 1716 ms | 204 | 118.881 fps / 1.981× |
| Longer hold | 4928 ms | 586 | 118.912 fps / 1.982× |

Read-only samples after release showed 60 frames advancing per second again.
Two ZL pause/resume pairs (1017 ms and 767 ms) preserved their frame counters
and correctly changed both emulation/audio flags. The user confirmed:
“sim, funcionou. pareceu 2x”.

The repeatable hardware-testing procedure is in
[`MINI-VALIDATION.md`](MINI-VALIDATION.md). The summarized measurements above
are retained; raw event logs and per-run installation records are not required
to understand or reproduce the test. Host tests can be rerun with
`bash scripts/test.sh`.

No additional games were tested, in accordance with the user's preference.
Two-controller hardware and reboot tests of this version are not yet performed;
input-state tests cover the two-controller logic.

## Installation and rollback

Installed on 2026-10-24 after verifying Canoe, the uploaded library, and its
runtime dependencies, with no game running. The launcher and Canoe remained
unchanged.

- Current library: `47cfaf7e0633e38c4ffb408f1843c2a23c553a88a6d1d80bd8651aafd12112dc`.
- Previous analog/ZL library: `a1ed2cf5ef45a43631afa06a9aa70509f17a88c3817bb8e97430918a1793502d`.
- Console backup: `/var/lib/hakchi/canoe-controls-backups/19700101-020922`.
- Local backup: the pause-only library preserved before the fast-forward update.
- The test procedure and retained result are documented in
  [`MINI-VALIDATION.md`](MINI-VALIDATION.md) and this file.

The Mini's clock is unset, hence the 1970 backup name. To undo only this
upgrade, exit to the Mini menu and run on the Mini:

```sh
sh /var/lib/hakchi/canoe-controls-backups/19700101-020922/restore-controls.sh /var/lib/hakchi/canoe-controls-backups/19700101-020922
```

This restores analog/ZL without ZR.

Actions are logged in `/tmp/canoe-controls.log`, including `zr_held`,
`zr_released` and `fast_forward_ready_120fps`. The log is diagnostic and optional
for operation. A button request in the log records held state; the native wait
is still used whenever the compatibility/game/pause conditions disallow fast
forward.
