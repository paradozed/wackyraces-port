# Wheel spin and steer (2026-11-09)

Static reading of the car update `0x46a4a0`; native captures and a `native_drive_test` check; **no
Wine comparison**.

## Recovered

- **The wheels** are the car model root's first four children (`[car +0x14] +0x60`, then `+0x14`),
  kept in `+0x273`, `+0x277`, `+0x27b`, `+0x27f` (`0x469ebc`).
- **The roll angle `+0x2a4`** (`0x46b0d2..0x46b157`): the length of the update's probe step (front
  probe `+0x40b` − rear `+0x423`, × the speed, accumulated at `0x46ac23` while the motion counter
  `+0x3eb` > 0) × 0.05 (`0x512a6c`) is added when the speed copy `[esp+0x44]` is below 0 (wrapped
  above 2π, `0x512618`), else subtracted (wrapped below −2π, `0x512a18`). It is set to 0 at the car
  init (`0x469f0a`) and by `0x46642f`.
- **The wheel frames** (`0x46b15b..`):
  - wheel 0: `0x43c250(+0x2a4, +0x403, 0)` (the steer) at its own translation;
  - wheel 1: a copy of wheel 0's 13 words, then its own translation;
  - wheel 2: the identity turned about x by `+0x2a4` (`0x405e8e`) at its translation; wheel 3 the
    same way (copy).
- **`0x43c250(a, b, c)`** (traced): rows x = (cc cb − sc sb sa, −sc ca, sc cb sa + cc sb),
  y = (sc cb + cc sb sa, cc ca, sc sb − cc cb sa), z = (−sb ca, sa, cb ca).
- MonsterTires' tyres turn by `+0x2a4` × 2.5 (`0x51629c`).

## Native

- `RaceSession` keeps `wheel_spin` from the forward step (`forward_step`, the probes' pitch left out,
  provisional) and the speed's sign; `steer()` exposes `+0x403`.
- `RaceView::submit_car` draws the wheels' batches apart: each wheel's batches with
  model × (the frame above at the wheel node's translation) × the inverse of the wheel node's
  transform; the rest of the model without them. MonsterTires' tyres take the same roll × 2.5.
- `native_drive_test` (dt_t1): "wheel spin turned 178 rad down in 72 updates" (about 2.5 rad an
  update at top speed, a 20-unit radius); with the step zeroed the check fails.
- Captures (`cc_t1`, vehicle 0): the wheels stay in place; with Left held the front pair turns.

## Not verified

- No Wine comparison (the spin is too fast to read on captures from behind).
- The sign test's slot `[esp+0x44]` is taken to be the speed (its write at `0x46ae50` comes from
  `+0x3ef`); the motion block's own copy is not traced.
