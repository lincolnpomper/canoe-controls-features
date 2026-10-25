# Future direction: shared controller input core

This is a design note for a later iteration. It does not describe the current
implementation and does not change the binaries.

## Goal

Separate controller reading from features that use controller input. A shared
input core should read Canoe/SDL controller events once, normalize them, and
make the full Wii Classic Controller state available to optional feature
modules. A user could then install only the features they want while keeping
one consistent source of input state.

The core would identify each connected controller independently and expose
normalized D-pad directions, face/shoulder buttons, Plus/Minus, HOME, both
sticks, and trigger values. It should preserve physical events and pass them on
to Canoe unchanged unless a feature explicitly requests a supported action.
Controller indexes and instance IDs remain runtime data, not configuration.

## Feature modules

Feature modules would subscribe to normalized events or query a shared state
snapshot. Examples include:

- map the left stick to gameplay D-pad directions;
- use ZL for pause or ZR for capped fast forward;
- use the right stick to adjust CRT coefficients;
- use the right stick to adjust the left-stick sensitivity while the left stick
  continues controlling the game.

The last example is compatible with a broadcast model: both the game-input
feature and the sensitivity feature can observe the same left-stick values, and
the right stick can adjust sensitivity independently. The input core reports
input; feature modules own their actions and settings.

## Avoiding conflicts

Modules should not each install competing SDL hooks or independently reinterpret
the same raw event stream. The core should dispatch each normalized event once
to registered modules, in a documented order. Observation should be shared by
default so one feature does not hide input from another. When a feature needs
exclusive use of a control, such as a menu chord, it should declare that claim
and a clear priority or context. Conflicting claims should be detected and
reported rather than silently depending on preload order.

The module interface should have an explicit ABI/version, lifecycle callbacks
for controller connection and disconnection, and defined behavior when a
module fails to load. The core and each feature should be independently
buildable and testable, with compatibility checks around any Canoe-specific
hooks. This avoids making every feature a new, uncoordinated interposer while
keeping the emulator executable unchanged.

## Current implementation and next design work

Today, the main controls library handles left-stick-to-D-pad, pause, and fast
forward. The separate CRT module reads SDL events for right-stick adjustment
and the reset chord. They chain their `SDL_PollEvent` hooks through
`RTLD_NEXT`; this combined setup has been tested, but it is not yet a general
plugin API or conflict manager.

Before building a shared core, define the normalized event/state API, module
registration and ordering, control-claim rules, and failure behavior. Then
extract the existing input tracking behind that API and migrate the CRT and
gameplay features one at a time, preserving hardware tests for the current
behavior.
