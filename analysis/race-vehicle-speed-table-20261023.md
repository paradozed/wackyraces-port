# WackyAuto per-vehicle speed target

## Evidence

The installed executable's `FUN_00483a20` copies 24 four-byte values for
each player vehicle from `DAT_0052b558` into `DAT_006f2990`, with a `0x60`
byte stride. The destination row index is the vehicle object's `+0x88` value.
The WackyAuto constructor `FUN_00469bf0` then reads the row's first float into
`WackyAuto+0x21b`. `FUN_00466db0` uses `+0x21b` as the target against which the
smoothed forward-speed state at `+0x3ef` is compared and clamped. This makes
the first row float the executable-backed per-vehicle speed target.

The seven IEEE-754 values from the executable data table are:

| WPC vehicle ID | Float word | Value |
|---:|---:|---:|
| 0 | `0x3f333333` | 0.70 |
| 1 | `0x3f266666` | 0.65 |
| 2 | `0x3f333333` | 0.70 |
| 3 | `0x3f147ae1` | 0.58 |
| 4 | `0x3f266666` | 0.65 |
| 5 | `0x3f147ae1` | 0.58 |
| 6 | `0x3f3d70a4` | 0.74 |

The second float in each row is also used by `FUN_00466db0`: for positive
speed below the target, WPC adds `(target - speed) * row[1]` each update. The
second words decode to convergence factors `[0.014, 0.017, 0.014, 0.017,
0.017, 0.017, 0.019]`. The same updater uses `+0x933` (initialized to 0.023)
for acceleration from reverse and braking, and 0.0096 for coasting/release;
the deadband is 0.015. The native loop now follows these normalized per-frame
updates at the recovered 60 Hz cadence, keyed by the selected WPC vehicle ID.

The movement integrator still maps normalized speed to world motion using the
provisional scale `760 / 0.70` world units per second. This preserves the
previous reference vehicle's approximate native speed while applying WPC's
relative top speeds and convergence rates. Absolute world scale and
speedometer mapping still need a synchronized runtime capture.

## Source locations

- `analysis/decompilation/wpc/functions-000.c`: `FUN_00483a20` player-object setup and the
  24-word row copy from `0x0052b558` to `0x006f2990`.
- `analysis/decompilation/wpc/functions-028.c`: WackyAuto constructor `FUN_00469bf0` loads
  row word zero into `+0x21b`; `FUN_00466db0` consumes it as a speed target.
- Radare2 inspection of `wpc.exe` at `0x0052b558` confirms each listed row
  word in the installed binary.
