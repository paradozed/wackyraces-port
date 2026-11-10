# WackyAuto wall contacts (`0x46a4a0`, 20261029)

`0x46a4a0` (WackyAutoNormal's update) is missing from the Ghidra export. It was decompiled from the local Ghidra project in headless mode, with the project opened read-only and no analysis; the output stays in the scratchpad.

Native code: `RaceSession::wall_contacts` and `RaceTrack::contact`.

## Why

The port's provisional response swept the two leading movement probes along the motion, slid the motion along the hit wall and scaled the speed by 0.6. When the slid motion was blocked again (a corner), it **zeroed the whole motion**. Cars stayed pinned in corners: a pile of robots in `sh_t1`'s first hairpin near (−7000, 725, −1500), and robots on `cc_t1`'s banks. Off the walls, the edge guard (still provisional) also zeroes the motion.

## The original response (`0x46bb..0x46c3f`)

1. **The move first.** The car takes its whole motion: position `+0x288` += motion. Then `0x468280` mode 1 places four **contact probes**:

   | Vehicle | Across | Up | Along |
   |---|---|---|---|
   | 0 | ±88 | 24 | ±190 |
   | 6 | ±68 | 24 | ±180 |
   | others | ±68 | 24 | ±122 |

   The along offset is stretched by 1 + 0.09 (speed − 0.4) above speed 0.4, as in mode 2. The probes go into `+0x40b` FL, `+0x417` FR, `+0x423` RL and `+0x42f` RR.
2. **The centre.** C is the car's position 30 along its up axis. Its height is the mean of the FL and RR probes (`+0x40f`, `+0x433`) plus 30 × up.y.
3. **The rays.** For each probe P, a ray (`0x4a4830`, mask `0x1e000ff`, record `0x6f12b0 + 0xa0·i`) runs from S = C + 0.2 (C − P) to P. A hit at H with normal N gives the correction c = N ((H − P)·N): the probe's penetration along the normal. The loop also keeps:
   - the per-axis largest positive and smallest negative components, starting from −1 and +1;
   - the mean hit point;
   - the hit flags.
4. **The aggregation** (`0x46c0..`):

   | Hits | Push |
   |---|---|
   | none | nothing |
   | one | c₀ |
   | two, N₀·N₁ ≥ 0.4 | the sum of the largest positive and smallest negative component, per axis |
   | two, N₀·N₁ < 0.4 (a corner) | the speed `+0x3ef` is set to 0; with e = (N₀·N₁) N₀ − N₁, the push is c₀ + (\|c₁\| / (N₁·e)) e, or the per-axis sum when N₁·e = 0 |
   | three or more | the per-axis sum; the speed is set to 0 when the hit flags include `0x1600001` |

5. **No downward push.** A negative vertical push is set to 0, and the speed is zeroed if it was negative too.
6. **Position.** `+0x288` += push.
7. **Turn.** The lever (mean hit − the last ray's start) × speed × 2·10⁻⁵ goes through `0x452ed0` with the push. The result's vertical component, clamped to ±0.07 (`0x512a54`/`0x512a50`), is added to the yaw `+0x26b`.
8. **Speed.** × 0.95.
9. **Other effects.** If the car is airborne (`+0x8a7` > 1), the slot rotation is cleared (`0x472680`). A mid-air hit with flags `0x600000` sets `+0x157` and stores the push direction. The 30-update wall sound timer (`+0x3e7`, slot `+0x3d7`) starts.

A wall therefore costs only 5% of the speed per update. The car slides along it as the probes are pushed back out, and the turn aligns it with the wall.

## Native

- **`wall_contacts`.** It runs after the move, before `settle`, for every physical car. It ports the probes, the rays, the corrections, the aggregation (the corner case included), the vertical clamp, the turn and the 0.95 factor.
- **`RaceTrack::contact`.** It queries with mask `0x1e000ff`. It accepts wall polygons (record flag 1) at any slope, and other surfaces only when steep (|normal.y| ≤ 0.6; flatter ones are the ground contact's). The normal faces the ray's start.
- **The old sweep and slide are gone.** The edge guard remains (provisional); the steep-rising-ground stop was removed on 2026-10-31 (see the glide below).

**Provisional:**
- **The normal is made horizontal.** With the full normal, steep banks (normal.y ≈ 0.64) lifted the test driver over `sh_t1`'s platform edges: 217 s and 9 respawns, against 147.9 s and 1.
- **The sign of the turn.** It is not recovered from `0x452ed0`'s argument order. It is chosen so that a car rubbing `sh_t1`'s wall near (−6400, −950) turns along it (−0.785 → −0.859 over 15 updates); the other sign turns it into the wall.
- **Not ported:** the three-hit flag stop (`0x1600001`; the hit flags are not passed), the airborne `+0x157` and the slot reset.

## Results (`native_drive_test`; robots always physical)

| Course | Every robot in 120 s (before) | Every robot in 180 s (before) | Test driver (before → after) |
|---|---|---|---|
| `cc_t1` | ≥ 2 laps | 3 laps (4 of 6) | 112.3 → 112.4 s |
| `sh_t1` | ≥ 2 laps (best 1) | 3 laps (none in the robot note's run) | 232.3 s, 8 respawns → 147.9 s, 1 respawn |
| `dt_t1` | ≥ 2 laps | 3 laps | 154.2 → 97.7 s |
| `ct_t1` | ≥ 2 laps | 3 laps | 120.5 → 102.7 s |

All 24 robots now finish 3 laps in 5400 updates. `native_drive_test` now requires every robot to do two laps in 120 s on each course. Not compared with Wine.

## Remainders (20261031)

- **Turn sign recovered.** `0x452ed0` writes `ecx = arg1 × arg2`
  (`ecx[0] = a1·b2 − a2·b1`, …). At `0x46c4bb` arg1 is the push (stack
  `0xc0`) and arg2 the lever (`0xec`: (mean hit − `0x1a8`) × `+0x3ef` ×
  2·10⁻⁵, `0x512a58`). The yaw takes the result's y, push.z·lever.x −
  push.x·lever.z, clamped to ±0.07. That is the sign the port had chosen.
- **Hit flags.** The loop ors each hit's polygon flags (the ray record's
  `+4`, `0x46bdd1`) into `0xfc(%esp)`. With three hits or more, any bit of
  `0x1600001` zeroes `+0x3ef` (`0x46c28d`). Ported (`TrackContact::flags`).
- **Airborne spin.** With `+0x8a7` > 1, `0x472680` on the slots `+0x48b`
  clears the running spin (`+0x873..+0x87b`) when it is non-zero. Ported.
- **TurboSpeed** (`0x46c5ad`, not in the first note): the push is
  normalised (`0x452e20`); when its y is below 0.92 (`0x512a48`) and
  `+0x385` = 8, `+0x3b3` = 0, the top-speed row comes back from `+0x3b7`,
  `+0x385` = −1 and the speed × 0.4 (`0x512a10`). Ported.
- **Full normals.** The push now uses the polygon's normal in full. Native
  results (`native_drive_test`, robots always physical / test driver):

  | Variant | cc_t1 worst robot | sh_t1 driver (1 lap) | dt_t1 driver | ct_t1 driver |
  |---|---|---|---|---|
  | before (horizontal, no flags/turbo) | 0 laps | 258.4 s, 12 respawns | 101.1 s | 106.3 s |
  | horizontal + flags/turbo/spin | 0 laps | 93.7 s, 0 respawns | 104.1 s | 104.9 s |
  | **full normal** (kept) | 2 laps | 150.3 s, 3 respawns | 101.4 s | 104.6 s |
  | full normal, no 0.6 slope filter | — | — | — | — |

  The last variant stops cc_t1's far robots (1 of 3 laps in 300 s), so
  the native slope filter (other surfaces only when |normal.y| ≤ 0.6)
  stays, provisional. The full normal is kept as the original's; the sh_t1
  driver's respawns are the test pilot's, not compared with Wine.
- **`+0x157` (not ported).** A wall hit with flags `0x600000` while
  `+0x8a7` > 4 sets `+0x157` = 1 and stores the unit push in `+0x23b`
  (`0x46c607`). Robots lose it in the same update: their ground ray
  (`0x46c858`, `+0xc0` = 1) clears it. For the player the four-probe
  contact (`0x46d7f4..0x46df27`) counts updates with `+0x157` or a probe on
  flags `0x1000001` in `+0x237` and keeps `+0x157` only past 3 (else 0;
  no such contact: `+0x237` = 0). Then the next update (`0x46a5a3`, unit
  push |x| or |z| > 0.1) skips the car update for the glide `0x4721f0`:
  position += `+0x23b`; a ray (mask `0x18000ff`) from 250 above to 200
  below; on a miss y −= 160, on a hit the car sits 3 off the polygon,
  oriented by its normal, and drivable flags (`0x8000fe`) count `+0x237`
  up to 5, which clears `+0x157`. It ends after 400 updates, or when the
  car rises more than 1 above its height at the fifth update: `+0x247` = 7
  (a cooldown counted down while `+0x237` = 0), `+0x237` = 0, speed 0.
  With `+0x237` = 0 and `+0x247` = 0 and a tilted up axis (|up.x| or |up.z|
  > 0.1), `+0x23b` is first scaled by (`+0x23f` + 1) × 20 with its y
  zeroed. (Ported the same day, below.)

## The glide, ported (20261031)

The fourth step of the count, read again in `0x46d0ef..0x46df27`: with
contacts, `+0x237` + 1; past 3, `+0x157` = 1 **and `+0x237` = 0**
(`0x46df1b` jumps to `0x46d0ef`, which writes edi = 0); otherwise
`+0x157` = 0 and the count stays (`0x46d0f5`). Without contacts
`+0x237` = 0. So the glide starts on the next update through the entry
branch (count 0: no cooldown, tilted car, push × (y + 1) × 20, count 1).
`ebx` is 1 in `0x46a4a0` (`0x46a4ab`), 0 in `0x4721f0` (`0x472213`). Every
end of the glide zeroes the speed (`0x472480`).

**Native** (`RaceSession::glide`, `Body::glide*`):
- `wall_contacts` sets `+0x157` on flags `0x600000` in the air (> 4).
- The player's four probes (`settle`, same rays as `0x46d7f4`: 80 above,
  30 below, mask `0x18000ff`) count `0x1000001` hits and keep `+0x23b` =
  the hit normal (facing up); robots clear `+0x157`.
- `move` starts with the cooldown and the glide; the glide ray is
  `RaceTrack::ground` from 250 above to 200 below; on a hit the car is
  set 3 along the normal, `up` = the normal, pitch and roll that put the
  heading and the side axis in the polygon's plane (the original builds
  the frame with `0x452ed0` and reads the angles back with `0x43c170`).
- The native steep-rising-ground stop is gone.

| Test driver | cc_t1 | sh_t1 (1 lap) | dt_t1 | ct_t1 |
|---|---|---|---|---|
| full normals, before the glide | 113.7 s | 150.3 s, 3 respawns | 101.4 s | 104.6 s |
| glide, steep stop kept | 114.1 s | 239.9 s, 5 respawns | 107.2 s | 111.4 s |
| **glide, steep stop removed** | 108.5 s, 0 glides | 239.9 s, 5 respawns, 5 glides | 97.4 s, 0 glides | 110.5 s, 4 glides |

Robots: every robot ≥ 2 laps in 120 s and every far robot 3 laps in
300 s on all four courses; the cc_t1 bend line −60911 (Wine −60849), no
falls. The sh_t1 glides begin near (−15500, 1180, −2730) in the air
(airtime 9, tilted 0.3/0.4), after four updates against `0x600000`
barriers, and drop the car about 300. **Not ported:** the speed update's
`+0x157` branches (`0x466e5f`, `0x466edf`), the engine sound one
(`0x467085`), and the calls closing the glide (`0x468620(1)`,
`0x469710`). Not compared with Wine.

## Wine comparison of the sh_t1 barrier (20261031)

Single race on `sh_t1`, vehicle 3 (the native test's), from the scratch
prefix. The player car was logged every 10 ms (`+0x288`, angles, `+0x3ef`,
`+0x8a7`; `+0x157`; `+0x237`, `+0x23b`, `+0x247`) and placed by writing
`+0x2c` (9 floats, reserved word, translation `+0x54`) and its copy `+0x98`,
`+0x288`, `+0x173`, the angles and the speed. Placement held only from
section 0 near the start; elsewhere the original put the car back.

- **Driven over the ramp** (placed at the native pilot's update 609,
  (−15156.5, 782.7, −2597.7), heading −2.8426, Up held): the original
  **does not take off**. The car follows the ramp and the platform
  (airtime 1 throughout), rubs the wall from (−14917, 895, −3295) and
  slides along it (heading −2.84 → −2.44, speed × ~0.96 an update).
  `+0x157` and `+0x237` stay 0. The native car leaves the ground at the
  ramp's top (airtime 2 at y 876) and meets the barrier in the air, which
  starts its glide: **the pilot's glide comes from the take-off, not from
  the glide code** (vertical-motion remainder, see TODO).
- **Falling onto the barrier** (placed at (−15100, 960, −2850), still for
  4 updates, then 0.442): at airtime 9 the car reaches (−14976.7, 946.7,
  −3245.2): `+0x237` = 1, `+0x23b` = (0.56, 0, 0.83), airtime 1 and the
  speed 0.411 → 0 in the same update. It then creeps along the barrier,
  `+0x237` going 1, 0, 1, 2, 3, 0… and never past 3, so `+0x157` stays 0:
  **no glide**. The native run from the same start (`air.cpp` in the
  scratchpad) reaches (−14965.6, 946.7, −3252.5) at airtime 10 with the
  same count and normal, lands on the next update, and keeps its speed
  (× 0.95, one wall ray on a `0x200000` barrier) instead of stopping.
  Which write zeroes the original's speed there was not caught: hooks on
  the five candidate writes (`0x46c29a` the 3-hit flag stop, `0x46c1e9`
  the corner, `0x46b632` the car-to-car clamp, `0x46a9b0`, `0x46c3ac`)
  were set afterwards, and the later placements did not reproduce the
  approach (the 3-hit stop fired twice, at placements).
- **Native fix found on the way:** with no ground under the car, `settle`
  returned before the probe count, so `+0x157` was never cleared and a car
  below the course chained glides without respawning. The miss now counts
  as the original's (`+0x157` alone counts; `count_glide`). The sh_t1 test
  driver: 239.9 s / 5 respawns → **139.5 s, 1 respawn, 2 glides**; ct_t1
  110.5 s, 4 glides; cc_t1 108.5 s and dt_t1 97.4 s, no glides.

## The take-off at sh_t1's ramp (20261031)

Native replays (scratchpad `ramp.cpp`, `replay.cpp`: the player placed at
a Wine sample, its yaw forced to each next Wine sample, Up held) against
the Wine logs of the same session:

- **From rest on the ramp** (−15139.4, −2589.6, heading −2.843, 0.452):
  native and Wine agree; no take-off, heights within 5 (804/817/821/824/
  837/854/871/888/900 against 798/818/822/827/832/849/866/881/895), the
  same roll (−0.41 then −0.13) and the same rub along the barrier.
- **Approaching from the flat** (Wine's own approach, from (−15637.6,
  −2100.4), heading −2.32 → −2.73): Wine's speed falls 0.436 → 0.415 →
  0.399 → 0.384 entering the ramp at (−15415, −2332), i.e. three updates of
  × 0.95 + the throttle: three wall contacts on the ramp face. The native
  car meets none (the ramp face, normal y ≈ 0.95, is dropped by the 0.6
  slope filter of `RaceTrack::contact`), keeps accelerating (0.441,
  0.446, …) and reaches the top faster. That is the take-off difference.
- **Without the filter** the three contacts match Wine exactly (0.415,
  0.399, 0.384), but the native push then lifts the car 30 an update (Wine
  11): at y 767.8 against 745.7 the rear ground probes (30 below) miss,
  the pitch stays 0 for six updates (Wine 0.26, 0.40), and the car keeps
  scraping (speed down to 0.345). On the courses robots stall: cc_t1's
  far robots finish 1 of 3 laps (one sits under a flat floor its wall
  rays hit from below, (1521, 692, −4349)); with the filter at |n.y| >
  0.98 instead, cc_t1 passes but one sh_t1 robot misses its two laps.
  The 0.6 filter stays. Why the native push lifts more than the
  original's is not found: probes (±68, 24 up, ±122 stretched), the ray
  from the centre 30 up + 20 %, and the order (walls, then the ground
  solve) are the same.

## Pushes compared step by step, and the tilt slot (20261031)

**Static re-read of `0x46bb72..0x46c2e0`.** Same as the port: the centre
C = (x, (FL.y + RR.y) × 0.5, z) + up × 30 (`0x51026c`, `0x512a60`); each
ray from C + 0.2 (C − P) (`0x51041c`) to the probe P; c = N ((H − P)·N);
per axis the largest positive (`0x114`, `0x118`, `0xdc`, from −1) and the
smallest negative (`0x110`, `0xd8`, `0x1c`, from +1); two hits with
N₀·N₁ ≥ 0.4 (`0x512a10`) push their sum (`0x46c2a4`), below it the corner.

**Wine hook** (`0x46c366`, the join of every path with hits; scratchpad
`hook3.py`): per contact update of the player, hits, flags, push, N₀, N₁,
c₀, c₁ and `+0x288` into a ring. Approach from the flat (heading −2.32 →
−2.77) onto sh_t1's ramp: **one** hit per update on the ramp face, normal
(0.133, 0.855, 0.500), flags `0x10`, pushes y 10.95, 16.46, 23.28, 14.51,
14.34; the pitch meanwhile rises by exactly 0.01745 an update (0.018,
0.035, 0.052).

**The tilt slot (`0x46ffb2`, new).** After the ground solve, the case of
the probe mask writes `+0x8ab`/`+0x8af` (FL 1, FR 2, RL 4, RR 8):

| mask | 1 | 2 | 3 | 4 | 5 | 8 | 10 | 12 | others |
|---|---|---|---|---|---|---|---|---|---|
| `+0x8ab` | 1 | 1 | 1 | −1 | 0 | −1 | 0 | −1 | 0 |
| `+0x8af` | 1 | −1 | 0 | 1 | 1 | −1 | −1 | 0 | 0 |

(`0x46fbc2`, `0x46fbde`, `0x46fc35`, `0x46fbf7`, `0x46fdeb`, `0x46fc17`,
`0x46fcc0`, `0x46fd50`; diagonals, three, four and no probes 0.) Values
1/−1/0 select ±1° (`0x511518`, `0x5129dc`) or 0 as the pitch and roll of a
type-2 slot request on `+0x48b` (`0x40400c` at `0x46ffe0`); the slots' sum
(`0x405c4f`) gives the next update's steps, added to the angles that
build the probe and move frame (`0x46ba57`) and stored (`0x46c500`).
Ported: `Body::tilt_pitch`/`tilt_roll` from the player's probe mask,
applied at the start of `move` (up rebuilt, `tilted_up`).

**Replay with the tilt and without the slope filter:** pushes y 10.48,
16.44, 23.80, 14.81, 14.70 (Wine 10.95, 16.46, 23.28, 14.51, 14.34), the
pitch steps 0.017, 0.035, 0.052 as Wine, heights within 4. Remaining
difference: at the seventh update Wine's ground solve already has a
left/right pair (roll −0.457), the port two updates later. Without the
filter cc_t1's far robots still stall (the robot under a flat floor at
(1521, 692, −4349), a robot-side problem), so the filter stays; with it
and the tilt the tests pass: test driver cc_t1 111.2 s, sh_t1 159.5 s /
2 respawns, dt_t1 104.5 s, ct_t1 119.9 s (108.5, 139.5 / 1, 97.4, 110.5
before the tilt).

## The robots' ground ray, and the slope filter dropped (20261031)

**The stall.** Without the slope filter the cc_t1 robot stuck at (1521,
692, −4349) was not a far-mode problem. It switched to the near mode at
the foot of the jump ramp (z −11860, 5000 from the idle player), took off
at the lip (z −10150) with its up axis tilted 15° (0, 0.97, −0.26), and
landed beyond the ravine 31 above the flat floor. There it hovered for
150 updates: its wall rays (24 up in its tilted frame) hit the floor and
pushed it up, while the native robot ray (vertical, 30 × up.y below)
never reached the floor. Its up axis also stayed tilted, since the native
robot frame only changed on a hit. Once it landed it drove off a ledge onto
a slope (up y 0.6) and stopped there.

**The original's robot ray** (`0x46c7f2..0x46c97e`, re-read):
- the axis is row 1 of the mode-2 frame (`0x468280(2)`, `0x43bf00`, +0xc..+0x14), the up axis, × 1.3 (`0x512a40`);
- the ray runs from position + 2 × 80 (`0x512a3c`) × that axis to position − `+0x93b` (30) × that axis: 208 above to 39 below along up, mask `0x18000ff`;
- on a hit (`0x46cf8a`) the position is the hit point + its normal × 3 (`0x511894`);
- the frame is rebuilt from the normal (two cross products `0x40266c`) and its angles go to `+0x267`/`+0x26b`/`+0x26f` (`0x4035e9` at `0x46d0c6`);
- the case is 20 (`0x46d0de`), past the jump table, straight to the tilt slot at `0x46ff2f`.

Airborne past 10 updates, `+0x267`/`+0x26f` ease by 0.85 for every car, so a robot's frame levels out as well.

**Ported.**
- `RaceTrack::ray(from, to)`: the ground mask along any segment, nearest hit.
- In `settle`, robots cast that ray along `car.up`, sit at the hit + normal × 3, and take pitch and roll from the normal (atan(−f·n / n.y), atan(r·n / n.y)).
- In the air, robots rebuild their up axis from the eased angles (`tilted_up`).
- `RaceTrack::contact` no longer drops flatter non-wall surfaces.

**First attempt (the longer ray alone).** It made things worse: two robots
slid down the jump's 58° back face into the ravine, then hovered there
100 above the floor, wedged on their wall rays with a fixed tilt. Easing
the angles in the air fixed both.

**Results** (`native_drive_test`, no filter, all checks pass):

| | Test driver | Near robots, worst in 120 s |
|---|---|---|
| `cc_t1` | 114.0 s, 0 respawns | 2 laps |
| `sh_t1` | 196.1 s, 6 respawns (160.7 / 3 with the filter) | 1 lap |
| `dt_t1` | 103.8 s, 0 respawns | 0 laps |
| `ct_t1` | 122.6 s, 0 respawns | 2 laps |

Far robots finish three laps on all four courses. The `dt_t1` robot with
no lap is trapped on a pavement at (−2450, 58, 27534): a flag-1 wall at z
27644 holds it, and a pole's `0x46a7d7` push of 25 + 270 × speed throws it
back each time it moves forward. The `sh_t1` respawns are falls off the
course in several places; none was compared with Wine. With the filter
and the new robot ray the tests passed too (sh_t1 160.7 s / 3 respawns,
dt_t1 near robots worst 3). The filter was dropped because the
original's ray takes every surface of its mask.

## A sh_t1 fall compared under Wine: hanging off a ledge (20261101)

**Native falls.** The test driver's first fall on sh_t1 is at the
tunnel hairpin. Reversing slowly off a ledge at (−4043, 858, −3494),
the car glided (+0x157 through a 0x600000 wall hit in the air). Off the
WGNR the glide drops 160 an update for its 400 updates, down to
y −64000, before the out-of-area return. The ledge is the course's
edge: the WGNR ends at x ≈ −4055 (flat ground at 862.8 inside), with no
surface below.

**Wine** (scratch prefix, single race on sh_t1, player placed at
(−4054, 862, −3520), yaw 3.039, speed 0, no keys):
- the original settles at (−4027.7, 850, −3514.6), outside the WGNR, and hangs there with x and z fixed;
- one probe keeps touching: the airtime cycles 1..5;
- each contact tips pitch and roll by 1° (the tilt slot), from 0.66 to 1.38 rad;
- the car sinks ~1 unit an update over ~140 updates;
- then it falls straight down (x, z fixed; the angles ease × 0.85), with +0x157 at 0 throughout;
- after ~41 updates in the air it returns to (−16687, 722, −1160).

**Native before the fix.** `settle` returned early whenever no WGNR lay
under the car's centre, so no probe was cast. The car never touched,
fell at once and respawned every 40 updates.

**Fix.** Nothing below the centre no longer skips the contact rays; the
out-of-area return (provisional) needs no contact as well as no WGNR
below. Native replay of the same placement: the car hangs at x, z fixed,
tilting 1° a contact and sinking slowly, then falls straight. It holds
~310 updates against Wine's ~140, so its probe geometry (vertical rays,
not along the car's down axis) is still provisional. Not compared: the
return point (Wine's is near the start, ~12700 away; the native one is a
recorded safe contact).

**Effect on the test driver** (sh_t1, one lap): 247.3 s and 10 respawns
(196.1 s / 6 before). Eight are drownings in section 2's water (y 2450)
around (19700..24500, −29100..−20300), where the autopilot was already
reversing about; none of them was compared with Wine. The other three
courses are unchanged (cc 113.7 s, dt 102.4 s, ct 126.1 s, no respawns).

## A sh_t1 drowning compared under Wine: air steering and the return point (20261101)

**Native case.** The test driver's drownings in section 2 come from a
loop. The provisional return put the car on a recent safe contact, the
platform at (22010, 3200, −28349). The driver then reversed off that
platform's edge (21154, 3188, −28265) at −0.4 with full lock, turning
0.11 an update all through a 33-update fall, and drowned in the water at
y 2450.

**Wine** (scratch prefix, sh_t1 single race):
- the car is moved to section 2 by a chain of placements along a robot's path (`wmem hops`, ~1500 apart, 300 ms each; the tracking follows);
- it is then placed at (21580, 3215, −28406), yaw −1.502, speed −0.398, with Down+Left held.

The original also falls off and drowns:
- **Lacet** : 0.108..0.111 per update on the ground, 0.033 from airtime 5 on, which is × 0.30;
- **Speed** : −0.4 kept in the air;
- **Descent** : 4.2, 4.4, 4.7, 5.2, 5.7 a update over the first updates, as the native fall;
- **Landing** : yaw −3.36, about 340 from the native landing in x, from a one-update offset in the steering onset;
- **Return** : after ~1.7 s, at (22599.5, 3000.1, −11006.6), yaw −π.

**Static.**
- `0x46ad3b`: when `+0x8a7` > 4 and `+0x385` ≠ 1, the player's turn term is multiplied by 0.3 (`0x512a04`); robots skip this branch.
- `0x4a6730` (the return frame):
  - the frame is the node `+0x48` of the row entry `+0x3c` + `+0x1bf` × 3312 of the car's row `+0x1b7`;
  - for row 0 with entry 0, it is the start line node (manager `+0x30`);
  - the frame is copied, then moved 200 (`0x512a44`) along its z axis; the lift of 50 (`0x512a5c`) follows;
  - `+0x1bf` is the entry the car came in by (`+0x8cc` of the slot, up to the row's `+0x38` − 1), not a progress index.
- Both Wine returns fit:
  - section 2's entry portal (22599.5, 2950.1, −10806.6), z axis (0, 0, −1), gives the drowning's return exactly;
  - the start line (−16887, 722, −1155) + 200 gives (−16687, 722, −1160), the earlier ledge fall's return.

**Ported** (`RaceSession::move`, `respawn`):
- the × 0.3 air turn for the player;
- the return at the frame of the car's section entry (the start line in section 0) + 200 along z + 50 up, for every car;
- the recent-safe-contact history and its back-off are removed;
- a returned car's update skips the start-line test, since the return can put it past the line.

Native replay of the Wine placement: the same yaw steps on the ground and in the air, the same descent, and the return at (22599.46, 3000.12, −11006.58), yaw −π, as Wine.

**Effect on the tests.**
- The test driver relied on steering in the air:
  - `cc_t1`: 184.0 s, 4th. It misses a jump in section 2 near (−51186, 1071, 2960) and wanders in the basin below for ~66 s;
  - `sh_t1`: it no longer clears the pyramids within 400 s (18 returns to section 2's entry).
- The checks changed:
  - the sh_t1 drive now checks that the driver reaches the last section;
  - the championship check takes the points of the place the driver gets;
  - the TurboSpeed check starts after 100 updates, before the cc_t1 ravine;
  - up to two near robots may be trapped on sh_t1.
- **Remaining.** On sh_t1, a robot reaches the first pyramid ramp (−18000) at 0.62. The ramp face's wall contacts (× 0.95) slow it to 0.36 at take-off, it drowns, and it then loops from section 2's entry. Under Wine, robots clear it.

## The wall-contact remainders (20261101)

- **Edge guard removed.** It was a native rule: a move that left the car's centre over no WGNR was blocked like a wall. The original does not do this. Under Wine on sh_t1, a car rests at (−4028, 850, −3515) past the WGNR edge, held by a probe. Since the contact rays now run without ground under the centre, the guard only got in the way. Without it, all checks pass (`native_drive_test`). The test driver:

  | Course | Before | Now |
  |---|---|---|
  | `cc_t1` | 184.0 s, 0 returns | 140.2 s, 1 return |
  | `sh_t1` | no lap in 400 s | one lap in 105.3 s, no return |
  | `dt_t1` | 110.4 s | 100.2 s |
  | `ct_t1` | 128.9 s | 116.2 s |

  The dt_t1 robot that was trapped on a pavement is free too. Two near robots can still loop at sh_t1's first pyramid jump.
- **`+0x157` in the speed update** (`0x466db0`, called at `0x46a719`). The branches at `0x466e5f` (brake) and `0x466edf` (accelerate) are the robots' (`+0xc0` ≠ 0):
  - with `+0x157` set, a braking robot gains `0x5129d4` up to its limit instead of slowing;
  - an accelerating one goes straight to its top speed.
  
  They are never reached: a robot's `+0x157` is set by the wall contacts (`0x46c607`) and cleared by its ground contact (`0x46c858`) later in the same update. Not ported, on purpose.
- **`0x467085` is the force feedback, not the engine sound.**
  - `edi` is the player's input device (`0x54190c[...]`), with force feedback when `+0x36f` & 0x100 and the option `0x6f4568[i]` (RETOUR DE FORCE) are set.
  - With `+0x157` set, `0x417140` sends `SendForceFeedbackCommand(DISFFC_PAUSE)` (vtable `+0x58`, argument 4).
  - Otherwise `0x416dd0` sets the effect from the speed (× 9) and the steering, with magnitudes 1000/2000/5000 by surface (`+0x8bb`).
  - The port has no force feedback.
- **The glide's closing calls are the normal update's.**
  - `0x468620(arg)` rebuilds the frame `+0x2c` from the angles and the position, then the camera target `+0x8eb`: the whole frame with argument 1, the translation alone with 0. The yaw is taken from `+0x283` when `+0x287` = 1, and the offset `+0x923` is added when `+0x91f` = 1.
  - `0x469710` updates the shown model: the LOD switch `+0x12a`/`+0x136` and the shown pitch `+0x13e` eased × 0.3.
  - The update ends with the same pair (`0x470544` with `+0x3eb` > 0 ? 1 : 0, then `0x470651`), so the glide adds nothing of its own.
  - Open (camera): the native follow frame keeps the lagged yaw and a level up, where the original's target carries pitch and roll (`0x478070` not ported).

## Robots on sh_t1's pyramid ramp under Wine (20261101)

**Setup.**
- The player is parked in section 2 by chained placements, at (21808, 2950, −16166).
- Six robots are logged every 10 ms (`wmem logv V`).
- A first try left the player at section 2's entry (−11162), so the robots passed the ramp in far mode (`+0x2b1` = 0x1000): constant speed and yaw, no pitch, airtime 1. Those rows say nothing about the physics.

**Near robots in the original** (left ramp, x ≈ 21900, z −18000..−20400):
- **On the ramp face**: pitch 0.325, airtime 1. The speed dips once by ~4 % (0.606 → 0.583), then rises again.
- **In the air**: the speed keeps rising for the whole jump (0.604 → 0.638 over airtime 2..15). The robots turn at full rate (0.1..0.14 an update): the × 0.3 is the player's only.
- **Robot 6**: it leaves the ground at the ramp's foot and flies into the face, losing speed (0.709 → 0.538) as the native robot did. It then lands on the ramp (pitch 0.324), speeds up and clears the jump. The peaks reach 3405..3496.

**Static.**
- `0x466e0b`: robots (`+0xc0` ≠ 0) go to their branch `0x466e17` without any airtime test. The `+0x8a7` > 6 skip (`0x466f91`) is the player's only.
- The port gated both on airtime ≤ 6. It also used "grounded" in place of the branch's `+0x157` tests, which never see the flag set for robots. The `+0x89b` tests are the car-follow lock (set to 10 at `0x46b64c`), ported 2026-11-01 (vertical-motion note).

**Ported.**
- `update_robot_speed` runs every update. In the brake case, the speed comes back up by the coast step to the −0.4 × `+0x8c3` limit (`0x466e88`).
- The surface modifiers (`update_grip`, `0x470218` from `0x4701f5`) now follow the last contact's flags at every update's end, in the air too, for the player and the robots.
  - Before, they were only refreshed on the ground, so `update_boost`'s doubling (`0x466df6`, while `+0x8db` runs) compounded in the air.
  - Once the speed also updated in the air, this blew a dt_t1 robot's `+0x8bf` up to 4096 and its speed to 7000 (coordinates past the integer range).

**Native replay** (near robots, sh_t1): the robot hits the face in the air (0.69 → 0.52), lands with up (0, 0.95, 0.32), speeds up and clears the jump. Its peak is 3405, as Wine's robot 4 (3404.9); its line runs at x ≈ 21875 (Wine 21866..21873). Every near robot does two laps in 120 s on sh_t1, and the test's sh_t1 allowance of two trapped robots is gone.

**`native_drive_test`** (all checks pass):

| Course | Test driver | Worst near robot in 120 s |
|---|---|---|
| `cc_t1` | 114.9 s, 1st | 2 laps |
| `sh_t1` | one lap in 248.4 s, 1 return | 2 laps |
| `dt_t1` | 108.1 s | 2 laps |
| `ct_t1` | 108.6 s | 1 lap |

Not compared: the right-hand ramp lane (x ≈ 23300) in near mode.

## The barrier settled against Wine (20261106)

Corrects the two sections of 20261031 above. Harness: sh_t1, vehicle 3, the
player written in one process (`hops`: 2 to 3 lines at speed 0, 30 ms apart, then
the speed; pitch and roll 0), logged every 10 ms, with jump hooks on all 20
writes that can zero `+0x3ef` appending to a ring. The native run holds the same
state (position, yaw, speed 0, pitch and roll 0) for the same updates.

**Three causes, all in the original's code:**

1. **Wall rays' centre.** `0x46bab5` takes C from `0x404def` (the car's world
   frame). At that point the update's motion has only reached `+0x288`
   (`0x46ba3f`), so C.x and C.z are the **last update's** position. The rays run
   from that centre + 20 % to the new probes. Native: `wall_contacts(.., before)`.
   Contact turn on the first hit: Wine -2.8400 -> -2.8155; old native -2.8370; now
   -2.8157. This probably also explains the "native push lifts more than the
   original's" mystery above (not re-measured).
2. **Player's ground rays.** `0x46c7f6` scales the up axis by 1.3 (`0x512a40`)
   **before** the robot/player branch at `0x46c82a`, so the player's rays
   (`0x46d7f4`) also run from 104 above to 39 below, not 80 and 30. Landing on the
   barrier: Wine airtime 1 on the hit's update; old native one update later and
   4.6 lower for the rest of the slide; now the same update and height.
3. **Slot tilt.** The step uses the last update's probes, so the slots' pitch and
   roll (`0x46b9a0`, stored at `0x46c500`) reach it one update later. Native
   applied them before the step. Wine y 947.35, 947.31, 948.44; native now the same.

**Fall** (-15100, 960, -2850, yaw -2.84, 0.442, no throttle), update: Wine / native

| Update | x, y, z | yaw | speed | airtime |
|---|---|---|---|---|
| 5 | -14935.36, 947.35, -3273.49 / -14935.66, 947.35, -3273.39 | -2.8155 / -2.8157 | 0.3925 both | 1 both |
| 7 | -14844.32, 948.44, -3340.19 / -14844.65, 948.44, -3340.15 | -2.7741 both | 0.3365 both | 1 both |
| 12 | -14667.45, 965.83, -3471.24 / -14667.29, 965.47, -3472.92 | -2.7207 / -2.7204 | 0.2191 both | 2 both |
| 19 | -14520.32, 982.04, -3573.36 / -14518.54, 981.14, -3576.19 | -2.6992 / -2.6987 | 0.1158 both | 9, `+0x157` = 1 both |
| 23 | -14474.62, 915.72, -3504.61 / -14472.85, 916.23, -3507.44 | -2.6921 / -2.6987 | 0 both | 9 both, `+0x247` = 7 both |

**Corrections to the earlier sections:**

- **A glide does occur under Wine**, here and in all 11 traced falls, and ends at
  `0x472490` as in the native code.
- **The dead stop (0.411 -> 0) did not reproduce** in 11 traced falls with 3 to 12
  holds, with and without the throttle. Its likely source is the placement, not
  proven: a car placed 16 inside the ramp (-15156.5, 782.7, -2597.7 with pitch 0;
  ground 798.4) was thrown 56 up and stopped.
- **The take-off is gone.** Run from the flat (-15637.6, 725, -2100.4, yaw -2.6,
  0.44, Up held): neither build takes off, both keep airtime 1 and rub the barrier
  from about update 21. Speeds are identical to 4 digits through update 13 (0.4449
  ... 0.3884); Wine then takes one wall hit more at update 14 (0.3743, native
  0.3940), and the runs drift apart by tens of units.

**Second course, cc_t4** (16339, 137.5, -10533.8, yaw -1.026, 0.563, Up held; a
wall slope in section 0, where the test driver landed after the seam). Both builds
throw the car 80 up and stop it dead, then count `+0x237` and set `+0x157` on the
same updates, within about 1 unit at first and 7 by update 25; one airtime differs
(update 7: Wine 2, native 1). Not compared: arriving there with the seam's own
pitch and speed (`hops` writes pitch 0 and speed 0 first).

**Course results:** after these fixes the test driver alone fell to 8 of 20 (was 9).
Its failures were not physics: on `cc_t4` it crossed the seam 1800 off the robots'
line (Wine robots cross near (-21600, 32780)), landed on that slope and glided
below the course, returning past the start line, so laps went uncounted; on `sh_t4`
the geodesic field stalled it. With the test driver's changes in `TODO.md`: 16 of 20.
