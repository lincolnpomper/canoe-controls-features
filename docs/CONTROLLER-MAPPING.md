# Wii Classic Controller input capture

Captured on an SNES Classic Mini on 2026-10-24, with the existing analog mod loaded alongside a temporary SDL input logger. A player exercised the controller in port 1 while Mario was open. SDL identified it as `Nintendo Clovercon`, controller index 0, joystick instance ID 0. Instance IDs are assigned at runtime and must not be hardcoded.

## Buttons

These entries combine the user's physical input sequence with the recorded press/release events and SDL's controller mapping. The first round included extra Right presses; all four D-pad directions were present. Minus and Plus were confirmed in a separate two-button round.

| Physical input | Raw joystick input | SDL controller input |
| --- | --- | --- |
| D-pad Up | Button 13 | Button 11, `DPAD_UP` |
| D-pad Down | Button 14 | Button 12, `DPAD_DOWN` |
| D-pad Left | Button 11 | Button 13, `DPAD_LEFT` |
| D-pad Right | Button 12 | Button 14, `DPAD_RIGHT` |
| A | Button 0 | Button 0, `A` |
| B | Button 1 | Button 1, `B` |
| X | Button 2 | Button 2, `X` |
| Y | Button 3 | Button 3, `Y` |
| Minus | Button 8 | Button 4, `BACK` |
| Plus | Button 9 | Button 6, `START` |
| L | Button 4; analog axis 2 | Button 9, `LEFTSHOULDER` |
| R | Button 5; analog axis 5 | Button 10, `RIGHTSHOULDER` |
| ZL | Button 6 | **Axis 4, `LEFTTRIGGER`** |
| ZR | Button 7 | **Axis 5, `RIGHTTRIGGER`** |
| HOME | Button 10 | Button 5, `GUIDE` |

ZL and ZR are physical digital buttons exposed as standardized SDL trigger axes: **0 released, 32767 pressed**. A shortcut in `SDL_PollEvent` should therefore observe `SDL_CONTROLLERAXISMOTION` (`0x650`), axis 4 for ZL, rather than assume a controller-button event. Handle the rising edge once and rearm after release, with state tracked per instance ID.

L/R produce both raw analog-axis changes and digital button events. The installed SDL mapping binds each shoulder twice, once from a button and once from an axis. The capture contains repeated controller shoulder events and state changes during travel, so those controller events alone should not be treated as a clean single-edge input.

## Sticks

| Physical input | Raw joystick axis | SDL controller axis | Observed values |
| --- | --- | --- | --- |
| Left stick horizontal | 0 | 0, `LEFTX` | Left −32768; right 32767; center 0 |
| Left stick vertical | 1 | 1, `LEFTY` | Up −32768; down 32767; center 0 |
| Right stick horizontal | 3 | 2, `RIGHTX` | Left −32768; right 32767; center 0 |
| Right stick vertical | 4 | 3, `RIGHTY` | Up −32768; down 32767; center 0 |

Each stick was captured in a separate right/left/up/down round, returning to center between movements. All four axes reached both endpoints and returned to zero.

HOME was captured last. Its press generated raw button 10 and SDL controller button 5 (`GUIDE`), followed by the logger unloading as the game exited. The user confirmed that the Mini returned to its home menu. The process ended before a HOME release event was recorded.

## SDL mapping reported by the console

```text
180000004e696e74656e646f20436c00,Nintendo Clovercon,leftx:a0,lefty:a1,rightx:a3,righty:a4,dpup:b13,dpdown:b14,dpleft:b11,dpright:b12,a:b0,b:b1,y:b3,x:b2,back:b8,start:b9,guide:b10,leftshoulder:b4,leftshoulder:a2,rightshoulder:b5,rightshoulder:a5,lefttrigger:b6,righttrigger:b7,platform:Linux,
```

Raw joystick axis numbers and standardized SDL controller axis numbers are different namespaces. In particular, raw axis 5 belongs to R's analog travel, while SDL controller axis 5 represents ZR.

## Capture method

The controller was captured with a passive SDL logger while one control at a
time was pressed and released. Both sticks were tested in separate
right/left/up/down rounds, returning to center between movements. HOME was
tested last because it exits the game and ends capture. The logger did not
alter input events. The observer and temporary launcher change were removed
after capture, and the original launcher, Canoe executable, and installed
analog library were verified unchanged.

See [the Mini validation guide](MINI-VALIDATION.md) for how to repeat a capture.

This document records the tested console/controller combination. It does not establish that every controller model or SDL build uses the same mapping.
