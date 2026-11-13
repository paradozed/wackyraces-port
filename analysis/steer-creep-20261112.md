# Steering from rest: the 0.05 creep (2026-11-12)

## How it was found

The closed-loop driver (`analysis/driven-race-20261112.md`) behaved differently on the `cc_t1` grid in the
two builds. A controlled test settled it: on the grid, 25 s after the start, Left alone (no Up) for 2 s.
- **Wine** (memory polled every 10 ms: the forward axis `+0x44`/`+0x4c`, the position `+0x54`/`+0x5c`,
  the speed `+0x3ef`): the speed reads 0.0500 at once and stays there while Left is held. The car
  circles, its heading falling about 2.1 rad a second (about 27 frames a second, so ≈ 0.078 a frame).
  (The first test's 0.0136 after the release came from an overlapping Up tap; see Checks.)
- **Native, before:** nothing. The yaw stayed at 0.147 and the car did not move (`yaw_step` is 0 at
  speed 0).

## The original

The steering update `0x467640` runs right after the speed update `0x466db0` in the car update (calls at
`0x46a719` and `0x46a720`).
- **Joystick axis:** an axis value other than 0 takes the analog branch (`0x4676b1..0x467782`), which
  ends at `0x467810` without the rule below.
- **Keyboard keys:** with the axis at 0 the keyboard branch tests the two keys' bindings
  (`0x52aac4` at `0x46779a`, `0x52aac0` at `0x467847`). Either key moves the steer `+0x403` by `+0x937`
  within ±0.698 (`0x5129e0`/`0x5129d8`). Both then reach `0x4677da..0x467806`: when the speed (the
  value read at entry) is at least 0 (`0x510124`) and below 0.1 (`0x511624`), the speed `+0x3ef`
  becomes 0.05 (`0x3d4ccccd`) and `+0x257` (the brake state) becomes 1.
- **Not taken by:** robot-driven or finished cars (`[+0x18]+0xc0` = 1 or 2, `0x467886`: the steer is
  copied from `+0x2ad`) and a car with `+0x3ff` ≥ 2 (`0x467897`, counted down). That never happens
  in the shipped game: the only other writer is the car init (`0x466159`, 1), and no member registration
  names the field (static search, 2026-11-12).

## Native

`steer_creep(SpeedState&, const DriveInput&)` in `game_drive` applies the rule. `RaceSession::drive`
(player 1) and the human opponent's update (player 2) call it right after `update_speed`, as the
original orders them.

The keyboard-only condition: `DriveInput::axis_steer` is set while a joystick's x axis byte is not 0
(the original's test at `0x46768e`), and `steer_creep` skips then. Not ported: the analog branch itself.
The port still maps the axis to digital Left/Right past ±64. (The `+0x3ff` lock is never set, see
above.)

## Checks

- `native_drive_test` (synthetic):
  - Left from rest gives 0.05 with brake state 1;
  - no change at 0.1, in reverse, or without a key.
- **The same test, port against Wine.** Both start the circle at the same point (Wine (3629.6, −6853.1)
  heading −0.176; native (3629.3, −6853.2) heading −0.176) and follow the same circle, e.g. Wine
  (3404.0, −6907.4) heading 2.725, native (3402.3, −6907.8) heading 2.888, a sample apart.
  - The port turns 0.0815 rad an update.
- **A clean rerun under Wine** (Left alone, no Up tap; one log line per frame) settles both points:
  - the heading turns 0.0815 rad a frame, the port's value exactly;
  - after the release the speed falls 0.0500 → 0.0404 → 0.0308 → 0.0212 → 0, with the brake state 1 → 2.
    That is the no-key branch `0x4673c8..0x467441` (coast 0.0096 `0x5129d4`, 0 below 0.015
    `0x5129c8`), as natively.
  - The 0.0136 of the first test came from an Up tap that overlapped the release.

## The whole steering update, ported (2026-11-12, later)

`update_steering` now follows `0x467640` for a human car. `a` is the joystick's x axis byte, now in
`DriveInput::axis`, and `s` the steer `+0x403`:
- **a < −85:** `s` = max(`s`, 0.1396), then +0.01745 an update while below 0.349 (`0x5129f8`, `0x5129f4`).
  **a > 85:** the mirror image.
- **0 < |a| ≤ 85:** `s` = a × −0.011364 × 0.1396 (`0x5129e4`, `0x5129f8`). The stick's left (a negative
  byte) steers positive, as the left key does.
- **a = 0:** the right key (binding `0x52aac4`) is tested first and takes `+0x937` off while `s` > −0.698;
  otherwise the left key (`0x52aac0`) adds it while `s` < 0.698. With both held, right wins (before:
  none). The limit is tested before the step, not clamped after (the keys alone settle at
  0.8 × step / 0.2, far below it).
- **Then:** `s` × 0.8, and 0 when |`s`| < 0.017453 (the deadband was 0.017 natively).
- **Joysticks in the port:** the x axis now feeds `axis`, no longer digital Left/Right past ±64.
  `steer_creep` skips while it is off centre.
- **Checks:** synthetic checks in `native_drive_test` for the full and partial stick, holding past
  85, and both keys. Not tested with a real joystick (none here).

