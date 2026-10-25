# Persistent CRT worker controls

This is a separate Canoe preload module for adjusting the two signed worker
coefficients used by the Mini's configured scanline path. It hooks the
framebuffer worker at `0x00163084`; it does not modify `canoe-shvc` on disk.
The hook checks Canoe's executable size and the first two instructions before
patching, so unsupported builds are left untouched.

The module reads SDL controller-axis events so the right stick can adjust the
CRT coefficients and ZL+ZR can reset them. This is separate from the main
library's left-stick-to-D-pad mapping and its ZL pause/ZR fast-forward actions.
The CRT module does not make the left stick control gameplay directions. Load
both shared libraries to use all features together; the two event hooks chain
through SDL's normal `RTLD_NEXT` lookup and this combined setup has been
validated on the Mini.

On the Wii Classic Controller's right stick, left/right changes the first
coefficient and down/up changes the second. Each adjustment is 10 points, with
a 400 ms initial key delay and 100 ms repeat interval. Values are clamped to
0–199. The coefficients are written to
`/var/lib/hakchi/canoe-filter-worker-probe/settings.conf` one second after
adjustments stop. Holding ZL+ZR together for 1.5 seconds restores defaults
114/85 and saves them immediately. ZL and ZR keep their pause and fast-forward
actions from the controller library.

The settings file format is `v1 <horizontal> <vertical>`. The build uses a
second shared library, `libcanoe_filter_shadow_test.so`, separate from the
analog/pause/fast-forward library. Install scripts and recovery backups remain
local to the Mini setup; this source directory contains no personalized
settings or device backups.

Build with `modules/crt-controls/scripts/build.sh` and an ARM-targeting Zig
compiler. The output is written to `modules/crt-controls/dist/`.
`modules/crt-controls/scripts/test.sh` runs host-side checks for loading valid
settings, rejecting invalid values, delayed persistence and the reset write.
The adjustment loop and framebuffer hook have been verified on the Mini; the
host tests do not emulate Canoe's renderer.

The Mini-tested persistent-settings build has SHA-256
`3be7f6e94a48665a66704c873b092c70cfa8eef6896ff493e54b759840f25001`.
