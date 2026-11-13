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

**Item aims (2026-11-08).**
- **The query:** `0x4e5b0a` (weapons 11–16, 20, 22, 24, 25 by the table `0x4e82f8`/`0x4e82f0`;
  17, 18, 19, 21, 23 drop at once, `0x4e5f33`), the FireSiren at `0x4e5b61` and the battle path
  `0x4e5d77`. Each calls the object query `0x403d69` with range ftol(`+0xf8`) and angle `+0x100` of
  the record of `car+0x375`.
  - `car+0x375` is the item's entry in the HUD table `0x2ab078c[icon]`, a `WPowerUpSprite`. The
    two members are `m_range` (`0x284`) and `m_visangle` (`0x285`).
- **Read under Wine (`cc_t1`):** the 27 entries are runtime nodes `0x36b..0x385`. Their values equal
  those of `general.wgm`'s first 27 `WPowerUpSprite`s in tree order:

  | Icons | Range | Angle |
  |---|---|---|
  | 0–3, 5–8, 26 | 0 | 2π |
  | 4 | 300 | 2π |
  | 9 (FireSiren), 10 (Shield) | 5000 | 2π |
  | 11, 12 (kicker, flipper) | 2000 | 40° |
  | 13 (paint) | 3000 | 30° |
  | 14 (breath), 16 (tank gun), 25 (missile) | 10000 | 20° |
  | 15 (boomerang) | 8000 | 70° |
  | 17–19 (dropped) | 2000 | −120° |
  | 20, 22, 23 | 5000 | 60° |
  | 21 (storm) | 10000 | 60° |
  | 24 (TNT) | 3000 | 20° |

- **The power-up actions share them.** The use `0x4d2970` (the icon's sprite as `this`) clones
  `0x6f2c50[icon]`, then writes into its first action `+0x28` = the car, `+0x2c` = the sprite
  record's `+0xf8` (`m_range`) and `+0x30` = its `+0x100` (`m_visangle`), and activates it. So the
  actions' "range `+0x2c`, angle `+0x30`" are their icon's values:
  - the FireSiren's query `0x405051` (5000, with `0x4a7e70`'s box test, not the whole section);
  - the falling tree's and brick wall's target (`0x4e23fc`: 5000 at 60°, before a provisional
    10000);
  - the detour's query (5000, then its strict 150 test, so the outcome is unchanged).
- **Native:** `ItemAims` (`default_item_aims`, `load_sprite_aims` from `general.wgm`, set by
  `linux_main`). It is also used for those three actions. `robot_items` aims every weapon and the FireSiren with its icon's values; before,
  TNT's 60° and 5000 were used for all.

**Provisional:**
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

## Far robots and hits (2026-11-08)

- **The trace (`dr_t2`, player idle on the grid, the race distance in the rows' `+0x34`):**
  - Robot 0, far, was hit around 160 s (its speed stayed 0 from then on in far mode).
  - The far motion does not run `move()`, so the hit's 4π spin (`request_spin`) and the −5 motion
    counter stayed pending.
  - At 241 s it crossed the seam into near mode next to the parked player. Then the stale spin
    (1.40 rad an update, decaying by 0.889) turned it round in place while the hold kept it still.
  - It drove +x, +z off the road, fell to y −1931, and was respawned 200 past the start line without
    crossing it: a lap was lost.
- **The original:** the car update `0x46a4a0` processes the slots (`0x4727b0`, `0x472890`, called at
  `0x46b962`) with no test of the far state `+0x2b1`. The far/near switch (`0x4e523a`) writes only
  the state, the angles, the position, `+0x173` and the speed.
- **Native (provisional):** the far branch counts the motion counter up, decays the spin and the
  pushes, and drops the spent ones, without moving the car. Robot 0 now ends its third lap at 241.4 s
  (329 s before).
- **Not checked under Wine:** that the car update runs for far robots (their angles stay frozen
  under Wine).

## A far robot's hit under Wine (2026-11-08)

`cc_t1`, the player on the grid and robot 0 about 40000 away (far mode). A one-shot jmp hook on
`0x4a1c40` wrote type `0x80` into robot 0's WackyAutoBomba (`car+0x36d`, `+0x2c`) and called
`0x41ba60`. `wmem logm` then read the bomba every 30 ms:

- the counter `+0x82` rose by one a frame, 1 to 80 in 3.5 s (about 22 frames a second);
- `+0x36` (done) was set at counter 80 and the type was cleared at 81;
- the robot meanwhile stayed far, moving from (−37090, 563, 15206) to (−57037, 553, 5298).

So the hit runs out while the robot is far, as the native far branch now lets it (`bomb_step`).
Not checked: whether the 25-update hold stops a far robot (natively it slides on).

The end differs:
- Wine sets `+0x36` at counter 80.
- `update_bomb`, with vehicle 0's model colour range (lo (0.23, 0.05, 0), hi (1, 1, 1), as
  `RaceView::model_colours` computes it), sets it at 73, as for every vehicle.

The cause (found the same day): Wine runs renderer 1 (`0x6f5958` = 1), whose fade steps packed
bytes with truncation, 33 updates a fade instead of 30. The native tint now follows that path, and
its mode changes match the log (`analysis/weapons-bomba-20261029.md`).

## A far robot's hold (2026-11-08)

### Wine

`cc_t1`, robot 0 far away, hit with 0x80 through the hook. `wmem logv 0` every 20 ms:

- before the hit: 79 units a frame at speed 0.56 (the far step);
- 0.33 s to 1.5 s: still, speed 0;
- then the speed rose by 0.0216 a frame (4, 7, 11 … 79 units a frame), up to 0.406, then fell to
  0.237;
- from 3.55 s, when the hit ended (counter 80): 79 a frame again, speed frozen at 0.233.

### Code

In the far state (`+0x2b1` = 0x1000), `0x4e53d4` jumps to `0x4e82e3`, skipping the far step,
when the bomba's type (`[car+0x36d] +0x2c`) is set. It also skips it when `+0x1c7` is 5. The car
update then moves the robot with its last control. The far flag itself (`+0x12a`, `0x4e50ce` /
`0x4e5200`) depends on the distance and `+0x385` = −1 only.

### Native

`drive_opponent` takes the far step only without a running hit. A far robot with a hit goes
through the near path, which keeps its last control while the bomba runs. `native_drive_test` hits
a far robot on every course:

- during the hold it moved 0 to 115 (a creep on a slope after the hop), against about 1600 for a
  glide;
- by the hit's end it had moved 145 to 5262.

## The robots' own gadget (2026-11-08, static reading)

- **`0x4e59a4`.** With `+0x1c7` = 2, the controller puts the robot's own gadget
  (`0x2ab078c[vehicle]`) in its slot `+0x371` and raises its charge `+0x399` to at least 1. With 4
  (`0x4e5a5f`), it puts its weapon (`0x2ab07b8[vehicle]`) in `+0x375`.
- **Writers of `+0x1c7`.**
  - Immediates: 3 (`0x4c7822`, `0x4d02d7`), 5 (trams), 7/8 (WSaw), 0xa (the controller), 0xb
    (WRotoHill).
  - Registers: the car's constructor (`0x4661bf`), the hiding (`0x4a2796`, 0), `0x4c5760` (0), and
    WGoldenGate (`0x4c6e35`, 1 for robots).
  - None writes 2 or 4. The controller compares `+0x1c7` with 2, 3, 4, 5, 7, 9, 0xa and 0xb, never
    with 1.
- **So** the shipped robots never get their own gadget (FlyPower) and never fly. WGoldenGate's 1 is
  read by nothing. Natively they do not use it either; to confirm under Wine.

## Hidden robots' laps and the shown row (2026-11-12)

**The lap pass** (`0x4a4170`, per car; its hidden branch `0x4a43e2..0x4a444c`): for a car with
`+0x2b1` & `0xfff0000` (hidden), when its section `+0x1b7` is 0, its previous section `+0x1bb` is
above 0 and its distance into the row `+0x2b5` exceeds the manager's `+0x34`, the laps `+0x187` and
`+0x19f` go up, `+0x1bb` = 0 and `+0x93f` = 1. Visible cars count at the line (`0x4a4201..`).
- The manager's `+0x34` (`0x49e552..0x49e56e`): a ray at the start line (`0x402199`) hitting a
  route polygon (flag `0x100000`) gives L − (L × the polygon's byte `+0x11`) >> 8, L row 0's length
  (`[+0x8c8]+0x20`); a miss stores 0 (`0x49e539`). Byte `+0x11` is the remaining fraction of the row,
  so this is the line's distance into row 0. Native: the line's section fraction × the row length.
- `+0x1bb` is the previous section: the trackers store the old `+0x1b7` before each change (the
  player `0x4a13ae`, the robots `0x4a234d`, a hidden car's row wrap `0x4a201d`); a forward crossing
  sets it to 0 (`0x4a4346`, `0x4a44c4`), a backward one to the last row (`0x4a435a`, `0x4a44dc`).
- Native: `Body::previous_section` (robots: `seen_section` compares each update), the rule in
  `virtual_robot`, and no counts once finished. The earlier guess (the line's distance passed during
  the step, with every section visited) is gone.

**The shown row (provisional).** A hidden robot is shown at its new row's entry WPortal; the original
then holds that row in `+0x1b7` until its tracker moves on, while the native section comes from the
ground. On `dr_t2` the ground under row 1's portal at (−39409, −9229) belongs to section 0, so the
shown robot read section 0, outside the window, and was hidden again 7 updates later at section 0's
end: with the player idle in section 2 every robot stayed in section 0 (61628 hidden robot-updates of
72000, no lap). Native: for up to 60 updates a robot shown into a row is not hidden while it stands on
the previous section.

**`native_drive_test`** (every course with 4 or more sections): the player placed at a robot's pose in
the middle section and left idle for 12000 updates; the best robot's laps must be at least 1 when
robots were hidden, and at most the course's laps. Results: `cc_t2` 3221 hidden robot-updates and 3
laps, `ct_t2` 8943 and 6 (9 laps), `dr_t2` 6647 and 3, and the same on the eight others. Controls on
`dr_t2`: without the shown-row grace, 0 laps (robots never advance); without the previous section
of visible robots, 0 laps (robots lap but none counts); without the finish check, 56 laps (a
finished robot standing at the portal is shown and hidden every 67 updates).

## `+0x93f` and the section-0 placement (2026-11-12; the placement ported later that day, below)

- Writers: the car constructor (`0x4661ef`, 0), the race start (`0x4a07b8`, 0), the lap pass
  (`0x4a444c`, 1: after a hidden lap; the visible path's lap completion jumps there too,
  `0x4a450c` → `0x4a4446`), and the hiding (`0x4a27a3`, 0 when the car's group `+0x104` differs from
  its tracker row `[ebx+0x4]`).
- Readers: the visible lap path (`0x4a4489`): while `+0x93f` is set, the crossing's result is
  replaced by 0 (no count). The show placement (`0x4a2108`): shown in group 0 with `+0x93f` set, the
  frame is that of the manager record's `+0x2c` object, `m_idStartLineModel` (registered at record
  `+0x28`, 8 bytes, the id resolver reading the second word; `cc_t1`: GDModel `0x3d3`), not the
  row's entry WPortal (`+0x48`).
- Read literally, a robot that stays visible counts its first crossing and then no other until it is
  hidden with group ≠ row, which would stop robots near the player from finishing; so part of
  this reading is probably wrong (the meaning of `[ebx+0x4]` at the hiding, or which cars take the
  `0x4a445b` branch). Not ported; a runtime check (robots' `+0x19f` under Wine with the player near
  them) is needed first.
- **Runtime check (2026-11-12, Wine memory reads, `cc_t1`, 3 sections, the player idle on the
  grid; the cars found by the WackyAuto vtable `0x51295c`, sampled every 10 s for 210 s).** The six
  robots run mostly in the far mode (`+0x2b1` = `0x1000`, which the lap pass's hidden test `0xfff0000`
  does not take), shown (1, 2) for a few samples. Each lap counts at the line in section 0
  (`+0x19f` 1 → 2 → 3 → 4 from about 40 s, 100 s, 130–160 s, 160–200 s); right after, `+0x1bb` = 0 and
  `+0x93f` = 1, and at the next sample, in section 1, `+0x93f` is 0 again. So `+0x93f` only blocks a
  second count right at the line and is cleared as the robot moves on (the clearing path is not
  identified; `0x4a27a3` is one candidate). The idle player keeps `+0x19f` 0, `+0x183` 0 and
  `+0x1bb` = 2 (the last row) the whole time. The native line counter already avoids double counts;
  the section-0 placement at the start line model applies only to a hidden robot shown in section
  0 after its hidden lap and stays unported.
- Robots' laps (sample times): about 60 s a lap from the first crossing, as the strip comparison
  (`analysis/native-hud-20261028.md`) had shown natively.

## The section-0 placement, ported (2026-11-12)

- `m_idStartLineModel` is the WModManager action's property `0x1e9`
  (`analysis/native-race-bootstrap.md`). `RaceTrack::start_model()` holds its frame, and the start
  line's when absent.
- On 19 of the 20 championship courses that frame equals the WStartLine's (`start_line()`): the same
  position and axes, within a unit (`dr_t1` 42 in y). On `sh_t3` they differ: the model is at
  (51522, 1959, 3601), the WStartLine at (170351, 4941, −3379), both facing +x.
- `Body::lap_mark` stands for `+0x93f`.
  - `count_lap` sets it, for the hidden lap (`0x4a444c`) and for the visible one (which jumps there,
    `0x4a450c`).
  - It is cleared once the car is out of section 0. That is provisional: the Wine reads show it 0 again
    in section 1, and the clearing path is not identified.
- `virtual_robot` shows a robot in section 0 with the mark set from `start_model()`, with the same
  offsets as from the entry portal (`0x4a2108` → `0x4a2131`).
- The native line counter already avoids double counts, so the visible path's `+0x93f` test
  (`0x4a4489`) is not needed.
- `native_drive_test` (courses with 4 or more sections): the player idles in the middle section
  until a robot laps hidden in section 0, then moves into section 1. The robot shown there must stand
  within 1000 of the start line model. The results are in TODO.md's item. Not compared with Wine.

## Robots' laps over a whole race against Wine (2026-11-12)

`cc_t1`, the player (Dingo-Limousine, vehicle 3) idle on the grid. Wine: the cars by the WackyAuto vtable
`0x51295c`, their vehicles `[car+0x18]+0x88`, `+0x19f` and the clock `0x6ab2d0` (1/300 s from the course's
load) read about once a second for 260 s. Native: `RaceSession(track, 3)`, the opponents' lap counts per
update. Wine's crossing counts 2, 3, 4 are the native laps 1, 2, 3.

| Vehicle | Wine (s): lap 1, 2, 3 ends | native (updates) | lap 2 and 3 durations, Wine s / native updates |
|---|---|---|---|
| 6 | 53.8, 114.7, 176.7 | 1550, 3335, 5149 | 60.9 / 1785, 62.0 / 1814 |
| 0 | 67.5, 132.5, 197.4 | 1967, 3828, 5687 | 65.0 / 1861, 64.9 / 1859 |
| 2 | 67.5, 132.5, 197.4 | 1629, 3481, 5339 | 65.0 / 1852, 64.9 / 1858 |
| 4 | 72.4, 142.3, 212.1 | 2111, 4139, 6145 | 69.9 / 2028, 69.8 / 2006 |
| 1 | 73.4, 148.2, 218.0 | 1705, 3705, 5701 | 74.8 / 2000, 69.8 / 1996 |
| 5 | 81.3, 160.0, 238.7 | 2328, 4588, 6862 | 78.7 / 2260, 78.7 / 2274 |

- Every lap duration converts at 28.6–29.3 updates a second (Wine's pace in this run; 27.3 was measured
  on `dr_t2` under Wine's software GL), so the vehicles' lap paces match within about 3 %, in the same
  order (6, then 0 and 2, then 4 and 1, then 5).
- At 28.8 updates a second the first-lap crossings of vehicles 6, 0, 4 and 5 match within 1 % (6: 1549
  against 1550). Vehicles 1 and 2 finish their first lap 300–400 updates earlier natively. Under Wine,
  vehicles 0 and 2 cross together on every lap (one held behind the other); natively 2 leads 0 by about
  340 updates. So the start differs for those two.
- **The grid explains it.** Read under Wine during the countdown (a RECOMMENCER restart, then the pause
  at clock 1.3 s): the seven slots are the native ones to the 0.1 unit, but the robots stand in other
  slots. Slots (2845, −6676), (3142, −6632), (3439, −6588), (3735, −6544), (3038, −6951), (3334, −6907):
  Wine 2, 0, 4, 1, 5, 6; native (seed 1) 6, 1, 2, 4, 5, 0; the player (vehicle 3) at (3631, −6863) in both.
  The order is random in the original: `0x49d580` makes count / 2 random pair swaps from the CRT
  `rand()` stream (`race_roster`), whose state depends on everything drawn before. So the first lap
  differs with the slots (natively vehicles 1 and 2 start in the front row), not with the driving.

## Robot lines against Wine (2026-11-12)

`cc_t1`, the player idle on the grid.
- **Wine:** the six robots' translations (`+0x54`/`+0x5c`) polled every 10 ms for 20 s (about 540
  frames), and their vehicles (`[car+0x18]+0x88`). The watch began some 10 s into the race.
- **Native:** `RaceSession(track, 3)`, every update for 50 s.

Each Wine point was measured against the same vehicle's native path (the nearest native point):

| Vehicle | Off its native path: median | 90 % | max | Off any native robot's path: median |
|---|---|---|---|---|
| 4 | 20 | 33 | 83 | 7 |
| 6 | 29 | 49 | 203 | 10 |
| 2 | 21 | 36 | 218 | 8 |
| 0 | 104 | 6307 | 7086 | 8 |
| 5 | 890 | 6455 | 7086 | 8 |
| 1 | 385 | 6384 | 7084 | 8 |

- **The fork:** the last three leave their native path at about (−14600, 15250) and rejoin it at about
  (−32500, 15200). There the route splits into a north branch (z ≈ 18720 at x −23000) and a south one
  (z ≈ 11640).
  - Wine sent vehicles 4 and 5 north and 0, 6, 1, 2 south.
  - The port sent 4, 0 and 1 north and 6, 5, 2 south.
  - The choice is the weighted random draw `0x4e9610` (`choose_route_link`), so which vehicle goes
    where depends on the random stream, which the builds do not share.
- **The pace:** 20 s of Wine wall time cover 15.7–19.2 s of native updates per vehicle. That is the
  original's ~28 frames a second against the port's 30 (`analysis/driven-race-20261112.md`); vehicle
  0's 15.7 s include its branch.
- **Conclusion:** on the same branch the robots' lines agree within about 20–30 units (90 % within 50),
  and every Wine point lies within about 10 of some native robot's line.
- **The start, the same day:** the watch started before the race's load, the robots standing from
  1.14 s to 3.59 s. Their six positions are exactly the grid slots listed above (the slot assignment
  differs, as known).
  - Over the first 12 s each Wine path is 30–75 from its own vehicle's native path (median) and 9–32
    from some native robot's. The 90 % figures (219–696) come from the start, where the slot order
    differs.
- **`dt_t1`, the same day:** the watch from the load, 40 s (1040 frames); native 300 s.
  - Each Wine path is 22–44 from its own vehicle's native path and 7–14 from any native robot's
    (medians). The 90 % figures are 34–148 from any robot's line, except vehicle 0.
  - Vehicle 0 stood near (16180, 19990) from 31 to 34 s, then jumped about 18000 in 3 s to
    (−1597, 27143): a return to the track, probably at the tram crossing. Its other excursion, near
    (9918, 12445) for 2.5 s, is also off every native line.
  - Pace: 40 s of Wine wall time cover 923–1203 native updates (31–40 s). Vehicles 4 and 0 trail
    (923, 958): vehicle 4's line is on the native ones; vehicle 0's includes the return.
  - Not compared: the trams' effect (the native trams run, but no native robot returned to the
    track in this window).
- **The other 18 courses, the same day:** the same watch from the race load for 40 s (1015–1080 frames
  each), run by one script that restarts the original for each course. Native: 300 s per course.
  - Each cell gives three distances, in units: off its own native path (median), off any native
    robot's path (median), and off any native path (90 %), each Wine point measured against the
    nearest native point.

| Course | vehicle 4 | vehicle 0 | vehicle 6 | vehicle 5 | vehicle 1 | vehicle 2 |
|---|---|---|---|---|---|---|
| `cc_t2` | 29/8/92 | 42/11/104 | 38/12/122 | 26/11/91 | 32/10/106 | 33/11/66 |
| `cc_t3` | 29/8/15 | 35/10/59 | 40/15/86 | 25/9/94 | 30/11/40 | 43/12/51 |
| `cc_t4` | 20/8/26 | 252/9/44 | 30/12/43 | 27/10/40 | 1263/11/40 | 79/10/42 |
| `sh_t1` | 26/8/24 | 28/8/27 | 31/9/27 | 22/9/25 | 27/8/54 | 27/8/22 |
| `sh_t2` | 17/8/20 | 27/7/18 | 31/9/110 | 23/9/25 | 28/8/46 | 24/6/25 |
| `sh_t3` | 23/7/17 | 24/7/17 | 27/8/26 | 19/8/25 | 25/7/20 | 25/7/19 |
| `sh_t4` | 36/10/59 | 27/8/23 | 35/9/34 | 31/9/62 | 26/10/52 | 29/8/24 |
| `dt_t2` | 19/4/22 | 21/5/21 | 38/17/513 | 19/6/26 | 19/5/29 | 26/9/74 |
| `dt_t3` | 22/8/41 | 27/9/35 | 27/9/53 | 37/9/37 | 24/8/58 | 23/8/28 |
| `dt_t4` | 32/12/42 | 31/10/37 | 29/12/46 | 24/12/62 | 245/12/94 | 30/11/35 |
| `ct_t1` | 24/7/20 | 27/7/23 | 29/7/26 | 22/7/31 | 21/7/26 | 27/7/17 |
| `ct_t2` | 28/8/25 | 29/8/21 | 37/12/57 | 22/10/83 | 25/9/46 | 27/8/20 |
| `ct_t3` | 28/10/38 | 32/10/60 | 45/17/156 | 21/10/78 | 31/10/64 | 33/10/59 |
| `ct_t4` | 39/10/28 | 32/10/25 | 41/14/65 | 31/11/41 | 33/12/41 | 31/10/27 |
| `dr_t1` | 21/9/45 | 31/8/36 | 38/13/115 | 22/9/54 | 31/8/39 | 30/8/51 |
| `dr_t2` | 26/9/63 | 28/8/25 | 33/8/60 | 24/8/35 | 27/9/74 | 28/7/22 |
| `dr_t3` | 23/6/77 | 35/11/75 | 33/14/142 | 28/10/119 | 20/8/101 | 44/13/100 |
| `dr_t4` | 21/7/25 | 28/7/21 | 38/11/251 | 21/8/76 | 26/7/33 | 30/7/20 |

  - Every Wine robot lies within a median of 4–17 of some native robot's line, so the course choice
    in the script was right each time (a wrong course would be thousands off).
  - Own-vehicle medians are 17–45, except `cc_t4` vehicles 0 (252) and 1 (1263) and `dt_t4` vehicle 1
    (245): another branch at a fork, as on `cc_t1`.
  - The larger 90 % figures are two incidents, not line differences:
    - `dr_t4` vehicle 6 (251): from 31.9 s it stays exactly at (−40763, −120830), about 251 off the
      line its native twin passed at update 866. With the player idle on the grid, this is probably
      the original hiding a far robot (both builds hide them); it stopped slightly elsewhere.
    - `dt_t2` vehicle 6 (513): from 27 s to the end of the watch it moves to and fro around
      (55400, −4300), 300–600 off every native line. The original's robot is trapped there for at
      least 13 s, while every native robot passes. Reproduced in a second run (33 s). It is the
      fountain's hold on a far robot, traced in `analysis/lap-line-geyser-20261106.md`; not resolved.
- **So the robots' lines agree with the original on all 20 courses.** Branch choice is random. One trap
  seen only under Wine (`dt_t2`'s fountain holding a far robot) is open.

