# Controller controls

This module handles Wii Classic Controller input for Canoe: left-stick to
D-pad, ZL pause and ZR fast forward capped near 2×. Its sources, host tests,
build/install scripts and current ARM library are kept together here.

From the repository root, build and test this module on its own with:

```sh
ZIG=/absolute/path/to/zig bash modules/controller-controls/scripts/build.sh
bash modules/controller-controls/scripts/test.sh
```

The binary is written to `modules/controller-controls/dist/`. The root-level
`scripts/build-all.sh` and `scripts/test-all.sh` also run the CRT module.

On the Mini, install or upgrade the library with the scripts in this module's
`scripts/` directory. Native pause and fast forward require the exact Canoe
build and runtime checks documented in the
[project README](../../README.md). For the separate CRT controls, see
[`../crt-controls/README.md`](../crt-controls/README.md).

Implementation and device-test notes are in
[`../../docs/ZL-PAUSE.md`](../../docs/ZL-PAUSE.md),
[`../../docs/ZR-FAST-FORWARD.md`](../../docs/ZR-FAST-FORWARD.md), and
[`../../docs/MINI-VALIDATION.md`](../../docs/MINI-VALIDATION.md).
