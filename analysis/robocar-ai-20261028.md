# RoboCar AI (WCtrlRoboCar) and the WGNR route layer

Recovered from the executable. `0x4e4f10` is missing from the Ghidra export (its body is 13,278 bytes). It was decompiled from the local Ghidra project with a headless script, a 600 s timeout and the project opened read-only; the output stays in the scratchpad.

Native code:
- `src/port/race_robot.*`: the driver;
- `race_session.*`: opponents with the player's physics;
- `game_ground.*` / `race_track.*`: route nodes and route queries.

## The action

`WCtrlRoboCar` is an action on every `WackyAuto` template in `general.wgm`. Registration, object and routines:
- registered at `0x4e39b0` in the action list `0x541ef4`;
- factory `0x4e3a00`, size `0xe8`, vtable `0x516ae4`;
- update `0x4e4f10`, init `0x4e45e0`.

Its members, from the member table, are read through `+0x18` with a 4-byte base:

| Member | Offset | Serialized | Notes |
|---|---|---|---|
| `m_rDirModifier` | `+0x24` | 0.9 | |
| `m_aMinAngle` | `+0x2c` | 0 | |
| `m_aFWAdjustAngle` | `+0x34` | 2π | |
| `m_aFWAheadAgainAngle` | `+0x3c` | π/4 | |
| `m_aReversingAngle` | `+0x44` | 2.094 | |
| `m_iFollowRadius` | | 1000 | |
| `m_iFinishEscapeRadius` | | 1000 | |
| `m_iEnemyCloseRadius` | | 3000 | |
| `m_iRatherTarget` | | 20 | |
| `m_iChangeMind` | | 5 | |
| `m_iNewModule` | | 5 | |
| `m_iMinFrameFollowGoal` | | 10 | |
| `m_rWallCheckLen` | | 300 | |
| `m_aMinSlowAngle` | `+0x8c` | 0.349 | 0x4e45e0 forces 0.349 |
| `m_rMinSpeed` | `+0x94` | 0.3 | 0x4e45e0 forces 0.55 |
| `m_rSpeedReducer` | | 1 | |
| `m_nWeaponWait` | | 3000 | |
| `m_nFieldWait` | | 100 | |
| `m_iUsePowerUp` | | 1 (0 on two cars) | |
| `m_nModuleNum` | | −1 | |

**The robot flag.** The template's `+0xc0` is `m_isrobotauto` (corrected 20261029; first read as an input channel): `0x49d580` sets it to 1 on every racer but the player. The robot leaves its control in the car:
- `+0x2ac` is the throttle (0 none, 1 forward, 2 back, 3 slow);
- `+0x2ad` is the steering.

The car reads them in two places:
- `0x467886` copies `+0x2ad` into `+0x403`, then applies the 0.8 decay and deadband, but not the 40° clamp.
- `0x466e17` drives the speed:
  - 1 uses the keyboard's acceleration;
  - 2 subtracts `+0x933` = 0.023 down to −0.4, then adds 0.0096;
  - 3 subtracts 0.0096.

## The route layer (WGNR record 0, flag `0x80000000`)

**Query.** WCarFx's ground ray `0x47c180` queries with mask `0x818000ff`, from 403.7 above the car to 1000 below:
- A hit on a record with flag `0x80000000` is the secondary hit. `0x482900` writes it at `+0x5a…`, sets flag `0x100000`, and stores the route context (`+0x8e`), the resource (`+0x92`), the record (`+0x96`) and the **polygon** (`+0x9a`).
- The car keeps these at `+0x2b9…+0x2c5`. `+0x2c5` is the robot's *module*.

**Polygons** are addressed in 4-byte words from the geometry's polygon stream: the second half of a grid link, as for ground polygons. A route polygon is laid out as follows:

| Offset | Content |
|---|---|
| `+0` | polygon flags (`0x08000000` quad) |
| `+4` | plane |
| `+6` | 3 or 4 vertex indices, padded to 4 bytes |
| `+0x10` | route flags (quads) |
| `+0x16` | counts: bits 4–7 forward links, bits 0–3 backward links |
| `+0x18` | 12-byte links {u16 target polygon, u16 weight, s16 x, y, z, u16}, forward links first |

The route flags:

| Bits | Use |
|---|---|
| bit 0 | turn-round check |
| bit 2 | ignore the second goal |
| bit 4 | not a default branch |
| bits 16–23 | blocked |
| byte 1 (`+0x11`) | the remaining fraction of the section in 1/255; `(0xff − b) × the section length (+0x34) >> 8` is the car's distance into the section, `+0x2b5` |
| byte 3 | a speed class: × 1.0 … 0.0 from table `0x52e600` |

Byte 1 is the WModManager route cursor that ranks racers (see TODO).

**The graph.** Each course section has one route record (record 0, matrix 0). Its graph is connected from polygon 0:

| Course | Nodes per section |
|---|---|
| `cc_t1` | 100, 62, 84 |
| `sh_t1` | 82, 36, 87 |
| `dt_t1` | 101, 47, 24, 70 |
| `ct_t1` | 113, 89, 71 |

The forward link points chain along the road, with byte 1 decreasing. For example, `cc_t1` polygon 33 has byte 1 = 0xc9, its forward link goes to 45 (0xbc), and its back link to 0.

## Goals (`0x4e9750`, `0x4e9610`)

When the module changes, `0x4e9750` does four things:
- **First link (`+0x50`).** If the previous module's next link (`+0x54`) belongs to this polygon, it is kept. Otherwise `0x4e9610` picks one at random: walking the links, it adds their weights and returns the first whose target's flags have no `0xff0000` bits once the sum exceeds `rand() × 2 − 1`. A weight of 65535 is always taken.
- **Second link (`+0x54`).** The same choice is made in the link's target polygon (`+0x2c9`).
- **Goals.** `+0x58` and `+0x64` are the two links' points, through the record matrix and the section frame.
- **Section exit (corrected 20261029).** `0x4e9610` returns a first link whose target is `0xffff` as is (a section's exit polygon, for example `dt_t1` section 0 polygon 1266). Its weighted walk also starts from the first link. `0x4e9750` then clears the second goal (`+0x54`, `+0x2c9`) but still takes goal 1 from that link's point. It returns with the previous goals kept only when there is no link at all. The port had kept the previous goal at exits too. With the slide's yaw kick, the robots then orbited the point beyond it.
- **Edge point (`+0x70`).** The midpoint of the module's two vertices that the next polygon does not share.

## Driving near the player (states in `+0x2b1`)

Default control: throttle 1, steering 0.

- `a` = heading − the direction to goal 1 (`fpatan` of the forward axis and the X/Z difference, wrapped to ±π).
- **Route flag 1:**
  - if `|a|` > `m_aReversingAngle` and this module is not the one last turned on, state 4;
  - otherwise state 2;
  - without flag 1, state 4 becomes 8.
- **States 1 and 2:**
  - `b` is the angle to goal 2. Without goal 2, or with route flag 4, `b` = `a`; at a dead end it aims through goal 1, away from the edge.
  - Same sign, `s` = `b`:
    - when `|b|` < `|a|`;
    - or when goal 1 is within 1000 units (integer squares) and the speed is above 0.4.

    Otherwise `s` = `a`. Its sign flips when reversing. Then steering = −0.9 × `s` × (80 + rand() % 30) / 100.
  - Opposite signs: steering = −0.9 × `b` when near and fast, otherwise none.
  - **Stuck:** an update counts as slow when the car moved under 50 units on both axes since the start of its own update (`+0x173`, `0x46a572`). After 100 slow updates the robot goes to state `0x10`.
- **State 4:**
  - throttle 2;
  - when the speed is negative, steering = (±π − `a`) × 0.9 / 2.
- **State 8:** the same about the edge point, then throttle 1 and state 2 once `|a|` ≤ π/4.
- **State `0x10` (unstick):**
  1. 30 updates in reverse;
  2. wait to stop, then pick a random side;
  3. 30 updates forward at ±5°;
  4. then state 2.
- **Target speed:** the top speed, or its route class.
  - Above `m_aMinSlowAngle`, `f` = 0.9 × 40 / `|s|` in degrees, with `s` capped at 40°.
  - When `f` < 1, the target is `d` × `f` / `+0x4c`, at most the top speed, where `d` is the distance to goal 1. Within `+0x4c` × top / 2 it keeps the current speed.
  - It is halved in states 4 and 8.
  - It is at least `m_rMinSpeed`, or half that when reversing.
  - `+0x4c` is the distance between the front and rear movement probes, 204 at rest.
- **Throttle:** 2 above the target, 1 below minus the target. It is 0 on surfaces `0x1000001`.

## Far from the player (state `0x1000`)

**Switch.** When the car is 5000 units from the player on an axis, or `dx² + dz²` > 24,999,999, and no item effect runs (`+0x385` = −1), `+0x12a` becomes 1 and the state `0x1000`. Both switches set the speed to top × 0.8.

**Motion:** the car does not use its physics. Each update it:
1. moves by top × `+0x163` × `+0x4c` × 0.56 when ahead of the player (`+0x1af` lower), or 0.48 otherwise;
2. turns by half the smaller same-signed goal angle (`0x4530a0`);
3. rests on the route contact.

So away from the player the robots go at about half their top speed.

## Native

- `RaceSession` holds the six opponents in grid order: each a `RaceCar`, its `Body` (history, drowning, laps) and a `RoboDriver`.
- The player and the robots share `move`/`settle`/`respawn`/`crossings`: walls, edge guard, ground, drowning, the loop seam and laps.
- Robots draw from the main `rand()` stream (`0x430f60` = `rand() % n`), as the power-ups do.
- In the far mode the car slides along at half speed and is put on the route layer (on the ground without one), then settled. See the far-mode laps section.
- `rank()`, the progress strip and the finish standings use the robots' progress and finishing order.
- After the finish a robot brakes to a stop (states `0x10000000`/`0x20000000`).
- **Provisional:** off the route layer the original only drives straight. The port keeps the last goals instead, because its provisional wall and edge responses push cars off the layer, at `dt_t1`'s first bend for example.

## Not ported

- Avoiding other cars: ported 2026-10-30, see `robot-avoidance-20261030.md`.
- The `+0x1c7` effect states (7–10).
- Battle mode.
- Car-to-car contacts: robots pass through each other and the player.

## Verification

- **`native_drive_test`:** on each Championship I course, every grid slot stands on the route layer. With robots always physical and the player idle, a robot laps within 120 s and every robot holds a goal.
- **Headless runs with robots always physical, 5400 updates:**

  | Course | Robots that finished 3 laps |
  |---|---|
  | `cc_t1` | 5 |
  | `ct_t1` | 5 |
  | `dt_t1` | 3 |
  | `sh_t1` | none; they loop through unsticks at the hairpin near (22500, −900), about 0.4 of a lap |

  A `cc_t1` lap takes about 1150 updates (38 s), close to the test driver's 40 s.
- The player's results are unchanged: `cc_t1` 3 laps in 118.8 s.
- In the game, the robots leave the `cc_t1` grid as a pack and follow the road.
- **Update 20261029:** with the original wall contacts (`wall-contacts-20261029.md`), every robot finishes 3 laps in 5400 updates on all four courses, including `sh_t1`.
- No Wine comparison of the robots' lines or lap times has been made.

## Rank-based speed scaling (`0x4e4d30`, 20261030)

Decompiled headlessly from the local Ghidra project, opened read-only (`0x4e4d30`, `0x4e45e0`, `0x4e4f10`); the output stays in the scratchpad. Not compared with Wine.

- **Course row.** `0x483e06` copies `0x52b910[course + environment × 4]` (6 words) to `0x6f2cc8`:
  - `0x6f2cc8` lead, `0x6f2ccc` gap and `0x6f2cd0` close (integers);
  - `0x6f2cd4` boost, `0x6f2cd8` far catch-up and `0x6f2cdc` far hold (floats).

  It also copies `0x52b8bc[i]` to `0x6f2c48`, read at `0x4e5973` (not traced), and logs the row with `SCDATA` (`0x52bc10`). Environments are cc, sh, dt, ct and dr, and the course is the variant (`_t1` = 0). Championship I's row is (2000, 6000, 10000, 0.2, 1.5, 0.9), with a far catch-up of 1.7 on `ct_t1`.
- **The factor.** At the race setup (`0x49f51d`), each robot's `+0xb4` = 1 + c + `rand()` × 2⁻¹⁵ × c / 4, with c = `0x6f2cd4`. That gives 1.2 to 1.25 on `_t1` courses.
- **Progress `+0x1a7`** (`0x4a3d20`): (`+0x187` − 1) × the lap length `+0xf38`, plus the lengths of the sections passed (`0x401ef1`), plus `+0x2b5`. Before the start it is a negative grid order (−10000 − n).
- **The decision `0x4e4d30`.** It reads the robot's rank `+0x1af`, its progress P, the progress of the first player in the racer list (`[0x6f2d00+0x48]`), and that of the racer ranked just ahead. The search goes through `0x6f2ce0` by vehicle and skips the player (`+0x38`). The defaults are P + gap + 1 in the main rule and P + close + 1 in the hold rule.
  - **Hold rule.** It applies to ranks above 4 (above 5 when `0x6f41dc` = 2, never when it is 3) outside environment 4. The result is −1 when the car ahead is within close.
  - **Main rule.** The result is +1 when P < player + lead and P < ahead − gap. It is −1 when the car is within close of the player and ahead − gap < P; that test comes second.
  - **First lap.** A negative result becomes 0 while `+0x187` < 2.
- **Application** (`0x4e4f6b`, game modes `app+0x1a60` 0 and 3, every robot update).
  - **Catch-up** (+1, with `+0x30` = 0): `+0x30` = 1. The top speed (row[0]), `+0x933` and the gravity `+0x98` are multiplied by `+0xb4`, row[4] by `0x52e670[v]` = 0.3, and `m_rDirModifier` becomes `0x52e630[v]` = 0.4. `+0x365` keeps the old gravity.
  - **Return** (< 1, with `+0x30` = 1 and `+0x19f` > 1): the values come back from `+0x21b`, `+0x21f`, `+0x223` and `+0x365`, and `m_rDirModifier` becomes `0x52e62c[v]` = 0.25.
  - **Far mode** (`0x4e5538`): the speed is × `0x6f2cd8` at +1 and × `0x6f2cdc` at −1.
- **Init `0x4e45e0`.**
  - It doubles the car's gravity `+0x98` (9 → 18 for robots), and sets `+0x94` = 0.55, `+0x9c` = 1 and `+0xac` = 900.
  - When it runs mid-race (manager phase `+0xf30` 4..7) or in mode 1, it also sets `m_rDirModifier` = 0.25 and row[4] × `0x52e66c[v]` = 1.0. In mode 1 the robot's `+0xb0` also becomes 100 + `rand()` % 100.

**Native.**
- `rank_row`, `rank_scale` and `course_indices` are in `race_robot`.
- `RaceSession` provides `set_course`, `race_distance` and `race_rank`. `scale_by_rank` runs before each controlled robot's update. The far step takes the factors.
- `VehicleTuning::reverse_step` makes `+0x933` per car.

**Provisional:**
- `race_distance` uses the native section lengths (the entry node's distance field) for `+0x34`;
- ties in rank go to the lower index.

**Gravity:** the doubling and the × `+0xb4` now apply (2026-11-01 check). The vertical motion `0x46a4a0` was ported on 2026-10-30.

**Results** (`native_drive_test`): the four courses unchanged for the robots' laps; the `dt_t1` test driver now finishes 2nd (1st before).

## Robot items (20261030)

From the same decompiles and `0x4679d0`/`0x46a4a0` (Ghidra export and headless run); not compared with Wine.

- **Pickups.** Every WackyAuto's object query (`0x46a768`, mask `0x79600`) takes an idle `WPowerUp` (`0x46aad5`): `+0x142` = 1, charge `+0x399` + 1; node `+0xa8` 0 starts the gadget slot `+0x3a0` (1), 1 the weapon slot `+0x39f`, 2 empties both. Only the player hears it (`+0xc0` ≠ 1).
- **Slots for robots** (`0x4679d0` with `m_isrobotauto` = 1). No roulette and no rank favouring:
  - gadget: state 0 holds the vehicle's own gadget (`0x2ab078c[v]`) and waits (`0xff`); 1 draws `(rand() & 3) + 7`; 5 spends a charge and clones the action (`0x4d2970`); 4 waits for `+0x385` = −1.
  - weapon: state 0 the vehicle's own (`v + 0xb`, or `rand() % 9 + 0x11` for vehicle 6); 1 draws `rand() % 9 + 0x11`; 5 and 4 as for the gadget, with `+0x389`.
  - The charge is capped at 3.
- **Decisions** (WCtrlRoboCar, near mode, `app+0x1a60` ≠ 1, `+0xb0` = 0):
  - every 300 + `rand()` % 300 updates (`+0xe0`), `+0xe4` = `rand_unit()` < `0x6f2c48` (`0x52b8bc[course + env × 4]`: 0.4 on `cc_t1` … 0.7): aim at the player only;
  - TurboSpeed (8) when the top-speed row is not raised; MonsterTires (7) on surface flags `0x80007c` or `+0x1c7` = `0xb`; FireSiren (9) at a car in view (`0x4a71e0`, template `+0x100`) farther than 700 (`0x516b54`); Shield (10) with guard 1, no hit and any car within 5000 (2π); the own gadget on `+0x1c7` 1 or `0xb`;
  - weapons with a charge, the gadget not being used and no hit: the dropped ones (`0x11`, `0x12`, `0x13`, `0x15`, `0x17`) at once; the others at a car in view farther than 700, the player or any car when `+0xe4` = 0, and when `+0xac` has run out or the target is the player; then `+0xac` = 100 + `rand()` % 100.
- **The power-up list `+0xd0`.** Every 31 updates (`+0xcc`), the four nearest objects with flag `0x2000000` within 6000 on each axis (`0x516b3c`) in the robot's section cell, by integer squared distance.
- **`0x4e4a10`.** With the speed ≥ 0, the steering angle s turns to the best untaken box (`+0x142` = 0): within 3000, within ±20° of the heading, and with |wrap(α − s)| below 80° within 500, 60° within 1000, 40° within 2000 or 30°, the nearest to s. It runs after s is chosen in states 1/2 (all three branches).

**Native.**
- `Opponent::Items`, `robot_slots`, `robot_items`, `robot_boxes`, `pick_up` for robots, `pickup_turn` in `RoboDriver::drive`.
- Gadget/weapon effects are per car (`Body::turbo`, `shield`, `weapon`); `use_gadget`/`use_weapon` take the car.
- In the four Championship I races of `native_drive_test`, robots took 8, 4, 47 and 5 power-ups and used 5, 3, 44 and 5 items.
- The `sh_t1` test driver now takes 178.8 s with 5 respawns (147.9 s, 1 before), because robots hit it.

**Provisional:**
- the weapons' query uses TNT's 60° and 5000 (template `+0x100` not read);
- the list covers every course power-up, not the section cell's;
- far robots take nothing.

## The `sh_t1` hairpin under Wine (20261030)

The original ran a single race on `sh_t1` under Wine, with the player parked at (−9250, 714, −913) so that the robots near the first hairpin stay physical. The `wmem.exe` probe (scratchpad) logged each robot (`0x6f2ce0[vehicle]`) every 100 ms for 60 s. Robots spent the following time slow (speed < 0.2) in the hairpin box (x −9500…−6000, z −2500…0):

| Robot | Time in the box | Time slow |
|---|---|---|
| 1 | 17 s | 13 s |
| 5 | 7.6 s | 4.9 s |
| 0 | 5.4 s | 3.3 s |
| 2, 4, 6 | ≤ 2 s | ≤ 0.8 s |

**Robot 1:**
1. It stopped against the wall at (−6880, 730, −1800), heading ≈ −0.19, still pushing at speed ~0.16.
2. It unstuck by reversing past z −2300 (speed −0.28), then came back to the same wall.
3. This repeated three times; on the fourth try it started from (−7772, −2313), took a different line and left after ~25 s.

So the original robots meet the same trap and leave only through the unstick's random side. The native robots differ in two ways:
- they drop to speed ~0 at the wall (the corner case of `wall_contacts`);
- one trap, at (−6880, 734, −650) against the cliff, never released in 60 s.

Placing a robot through `+0x288/+0x54/+0xc0` misplaced it by ~7000 units, and the reason was not found.

## Far-mode laps against Wine (20261030)

**Original.** `cc_t1` under Wine (scratch prefix, audio off), the player idle on the grid so every robot runs in state `0x1000`. The `wmem.exe` probe logged each robot's lap (`+0x19f`) every 100 ms; times are from the start of the log, so only laps 2 and 3 compare.

**Native fixes.** Before them no native far robot ever lapped (the tests had used `set_robots_always_near`):
- `cc_t1`: section 2's ground runs ~200 units past its last route polygon (z 17800..18000 at x −70100). There the route exists only through the loop transform (section 0), so the robot kept its exit goal (link `0xffff`) and circled it. Now a far robot off the route crosses the seam when the route lies on the other side (`RaceSession::cross_seam`, shared with `settle`). Provisional: how the original's `+0x2c5` lookup reaches the next section is not traced.
- `sh_t1`: putting the far robot on the ground dropped it 500 units off the jump at (22000, −19700) onto the floor below and off the route. The far step now rests on the route layer (`RaceTrack::Route::y`), as the original rests on the route contact (`+0x2bd`, `+0x2d5`), and falls back to the ground without one.

**Lap durations, `cc_t1`** (seconds):

| Vehicle | Original lap 2 / 3 | Native lap 2 / 3 |
|---|---|---|
| 0 | 66.1 / 65.6 | 62.1 (lap 3) |
| 1 | 75.6 / 71.1 | 66.8 |
| 2 | 66.1 / 66.1 | 62.2 |
| 4 | 71.1 / 71.1 | 69.0 |
| 5 | 79.0 / 80.6 | 75.3 |
| 6 | 62.6 / 63.6 | 58.8 |

The vehicle order matches (6 fastest, 5 slowest). The native laps are about 5% quicker; the route-layer rest shortened them by ~1.5 s from the ground rest, and the cause of the rest of the gap is not traced. The native far robots finish three laps in 300 s on all four Championship I courses (`native_drive_test`). Lines were not compared, only lap times.

## Robots outside the player's rows (20261031)

The race manager's update `0x4a1c8b` walks the robots (`manager+0x60`,
12 bytes each: car, section, row) after moving the player's row slots
(`+0x830` previous, `+0x8c4` current, `+0x958` next; see
[trams](trams-20261030.md), "Section enabling").

- **Hide** (`0x4a2759..`): a robot not hidden whose row matches none of
  the player's slots gets state `+0x2b1` = `0x10000`, `+0x1c7` = 0 and flag
  `0x100` on its node and children (not drawn, out of the queries). Its
  position stays where it was. `+0x2b5` keeps the far step's distance into
  the row, (0xff − module `+0x11`) × row `+0x34` >> 8 (`0x4e545f`).
- **Run** (`0x4e52f1`): the robot's own update only adds `+0x4c` (204,
  the probe span the far step also uses) to `+0x2b5` and returns (not in
  app mode `+0x1a60` = 1). The manager (`0x4a1fbd`) starts the next row
  (section) when `+0x2b5` exceeds the row's `+0x34` and sets `+0x104`.
- **Show** (`0x4a20aa`): once the row is one of the player's, flag `0x100`
  clears, `+0x8a7` = 1, `+0x2b1` = 1, `+0x2b5` = 0, the slow counter
  `+0xa8` = 0, and the car takes the frame of the row's `+0x48` object (the
  entry WPortal) moved 200 along its z axis, 50 up and (k − robots / 2) ×
  200 along its x axis (constants `0x512a44` = 200, `0x512a5c` = 50; k
  counts the robots placed at that slot in this pass). In section 0 with
  `+0x93f` set another frame is used (`0x4a2108`, not ported).
- `0x4a3a86`: a hidden robot's distance left in its row is `+0x34` −
  `+0x2b5` (rank and progress).
- Row `+0x34` on Championship I (one row per section): cc_t1 37725,
  25763, 75716; sh_t1 59521, 25316, 43354; dt_t1 52197, 20220, 16815,
  25889; ct_t1 60000, 48484, 38180. Only dt_t1 has more than three
  sections, so only there can a robot leave the player's rows.

Under Wine (2026-10-31, dt_t1, player in sections 0/1) far robots stood
~3.2 s at section 2's entry (16182, 19987; portal `0x35e` at 16272,
19812) and appeared at (1048, 23084) near section 3's portal `0x3df`
(1457, 23559). The code gives 16815 / 204 ≈ 83 updates (2.8 s at 30 Hz)
and a show at (1257, 22961) for k = 0; Wine sampled the robot some
updates after it.

**Native** (`RaceSession::virtual_robot`, `Body::hidden`): the same hide,
run and show; hidden robots are skipped by drawing, car contacts, object
probes, weapons, power-up reappearance, car effects and the robots'
avoidance. Their race progress uses `+0x2b5`. Provisional: the lap counts
where the hidden run passes the start line's distance into section 0.
`native_drive_test` (dt_t1, player on the grid): 18 hides and shows, the
longest 83 updates, shows within 648 of the entry portal.

## The route cursor (2026-11-01)

**The original.**
- `0x4a3880` (the distance left in a car's row) returns `+0x34` − `+0x2b5` for a hidden car (`+0x2b1` & `0xfff0000`).
- Otherwise, with a route polygon in `+0x2c5`, it returns (word `+0x16` >> 8) × `+0x34` >> 8. That is the polygon's byte `+0x17` on a quad: the high byte of the link-count word, whose low byte holds the link counts.
- Without a polygon (`0x4a394e`) it falls back to the distance from the car to a frame of the row (not traced).
- The far step's `+0x2b5` (`0x4e5f90`) instead reads byte `+0x11`, the route flags' byte 1, as (0xff − b) × `+0x34` >> 8.

**The data.** Walking the route graphs of cc_t1 and sh_t1:
- the two bytes agree within a few units on most polygons;
- the largest difference is 93 (sh_t1 section 1);
- both grow along the forward links from the exit polygon, which has 0.

**Native.**
- `RaceSession::update` queries the route polygon under the player after the step, takes each visible robot's from its driver (`RoboDriver::module`, `+0x2c5`), and keeps the last hit (`Body::cursor`/`cursor_section`).
- `fraction()` uses 1 − b / 256 while the cursor is in the car's section, otherwise the geodesic field. The player's `rank()` goes through the same path.
- `native_drive_test` passes. The test driver's times: cc_t1 139.6 s (222.7 before), dt_t1 105.9 s, ct_t1 105.3 s. The gadget check now waits for the effect to end. The rank change altered the roulette's draw, and state 4 waits for the effect.
