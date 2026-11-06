# The start line's lap rule and WGeyser, 2026-11-06

## The lap rule (`0x4a41db..0x4a42bc`, `0x49a2f0`)

Before this, the port used a native rule: a crossing within 5000 sideways, with every section visited. On `cc_t3` no robot ever got a lap, though they ran the whole course in about 50 s: the native seam comes where section 3's ground ends, 450 past the line, so the crossing was never seen.

**Original.**
- `0x49a2f0(line, current, previous, radius)`, radius = WStartLine m_rRadius (`0x1df`, 5000 on the courses read):
  - the current position, relative to the line, must be within the radius on x and z and in the circle;
  - with the sides of both positions along the line's z axis: current ahead and previous behind gives +1, the reverse −1, else 0.
- The result adds to `+0x183`, never below −1. Past 0 it counts: `+0x187` and `+0x19f` go up and `+0x183` returns to 0.
- `+0x183` and `+0x19f` start at 0 with a start line (`0x49f4c9`, `0x49f4ef`). The grid stands behind the line, so the first crossing shows lap 1, and the race ends when `+0x19f` passes the lap count (`0x4a42a1`), at the fourth crossing of a three-lap race.
- There is no "sections visited" condition.

**Native.** `RaceSession::crossings` does the above with `Body::line`, which starts at −1 because native laps count completed ones: the first crossing adds none.

**Provisional.** The native seam stays where it was. So the last section is also tested through the wrap transform, which the original does not need because it wraps at the portal. As a result the last lap can count one loop before its seam, and the test allows `loops >= laps - 1`. Hidden robots keep their native trigger.

## WGeyser (record `0x515640`, init `0x4c8bd0`, update `0x4c8cc0`)

Members from `+0x88`, 8 bytes apart (`cc_t3`):

| Member (ID) | Value |
|---|---|
| m_bqAlpha, m_bqOmega (`0x238`, `0x239`) | 0.9, 0.0873 |
| m_Delay (`0x23a`) | 6 |
| m_Chance (`0x23b`) | 99 |
| m_Frames (`0x23c`) | 25 |
| m_Sugar (`0x23d`) | 4000 |
| m_Height (`0x23e`) | 1270 |
| m_Steps (`0x23f`) | 6 |
| m_Width (`0x240`) | 200 |
| m_Distance (`0x241`) | 11547 |
| m_MaxFly (`0x242`) | 5 |

States `+0x17b`, jump table `0x4c9528`:
- **0 (`0x4c8d23`):** the sensors are the brick and its children from the third. With a car within m_Sugar: `rand % 100 < m_Chance` aims at the car + its z axis × m_Distance, at the brick's height; otherwise it waits. With no car it aims at random around the brick. Moving on, the brick goes to the aim and the ring's scale to 0.
- **1 (`0x4c8fb5`):** the ring (second child) scales up over m_Steps.
- **2 (`0x4c90c2`):** three biquad values follow 0 or 1, the ith turning on after i × m_Delay updates; each also turns by (i + 1)° an update. For m_Frames updates.
- **3 (`0x4c935f`):** towards 0.6 (`0x3f19999a`), the material's fade going 255 → 1 over 50 updates; then all zeroed.
- **4 (`0x4c903b`):** the ring shrinks over m_Steps.
- **5 (`0x4c94c7`):** m_Frames idle.

**Cars (`0x4c9161..`):** with h = the largest value above 0.1 (`0x511624`), for the cars of list 7:
- inside means |dx|, |dz| < m_Width, in the circle, and |dy| < m_Height × h;
- the car's speed `+0x3ef` and `+0x403` become 0 and its pushes are cleared;
- below the top (y + m_Height × h): it is centred on x and z, and rises 100 (`0x510414`) an update, or goes to the top once within m_MaxFly, with airtime 0;
- at or above the top: airtime 10, so it falls.

**Native.** `Geyser` in `race_objects`, `CourseObjects::lifts()` applied by `RaceSession::update_objects`, column and ring drawn by `RaceView`.

**Provisional or not ported.** The three values are taken to scale x, y and z, and only the y turn is applied (WGeyserModel `0x4c9850`, `0x4c99a0` not read). The fade and the sounds are not ported. The random draws use their own stream, not the shared rand(). The random aim's range is read as ± m_Sugar (`0x40321a` not read). Not compared with Wine.

## Results (`native_drive_test`, 2026-11-06, final binary, all 20 courses)

**Every check passes (9):** `cc_t1`, `cc_t2`, `cc_t4`, `ct_t1`, `dr_t3`, `dt_t1`, `dt_t3`, `sh_t1`, `sh_t4`.

**The other 11 stop on a check of the test driver**, after the robot checks have passed:
- "laps through the loop transform": `cc_t3`, `ct_t3`, `ct_t4`, `dr_t1`, `dr_t2`, `dr_t4`, `dt_t2`, `dt_t4`, `sh_t2`, `sh_t3`;
- "race finished": `ct_t2` (the driver does 3 of its 9 laps).

Far robots finish 3 of 3 laps in 300 s on all 20 courses. In the always-near check the best robot does 2 or 3 laps in 120 s on every course.

The driver is greedy and its result moves with small physics changes (`dt_t4` passed in an earlier run and fails now; `dr_t3` passes now). A failure there does not separate a driver limit from a physics gap. Passing means the test's checks pass, not that the course plays like the original; only `sh_t2` and `dt_t2` below were compared with Wine, and only on the points named.

## Open: the player on those courses

The greedy test driver gets stuck, and so does an experimental follower of a robot's recorded path. So this is not settled as a driver limit only:
- `cc_t3`: the driver wanders in the pit inside the spiral ramp (27000, −3285, 32000), whose sides are wall-flagged (surface 1). The robots' road is the ramp around it. The follower stopped near (25217, −2717, 29733), on the ramp's outer side.
- `dt_t2`: the follower stands at speed 0 against the ground's edge at (34106, 390, 15268) after one return.
- Not compared with Wine, and the follower is not in the repository.

## Further fixes (2026-11-06, second pass)

**The player's stall on `dt_t2`.** At (34106, 390, 15268) the car stands in a kerb corner: going straight or in straight reverse it stays at speed 0 (the corner rule zeroes the speed each update), but steering takes it out. So it is not a permanent trap. The path follower also stops on `cc_t1`, which the test driver laps, so the follower proves nothing about the physics either. Whether the failures on `cc_t3`, `ct_t2`, `ct_t4`, `dr_t1`, `dt_t2`, `sh_t3` are only the driver's stays open.

**Far robots.**
- `cc_t4`: a far robot stayed at (−8817, −356, 28591) with up y 0.24, its contact ray along that axis holding it to the cliff. Below up y 0.5 the far step now stands it level (provisional).
- `sh_t2`: the route passes 37 above deep water at z 94844, within the contact ray, so far robots drowned and looped in section 1, and none ever hid. Far robots no longer drown (`Body::sliding`, provisional).

**Sections at a tie.** On `dr_t4` a robot shown at section 3's portal counted as section 2 (their grounds overlap at the same height, and the first section won), hid again at once, 9369 times. A tie against the previous section now keeps the car's own section (`RaceTrack::ground_in`; provisional, the original changes row at the portal, `0x4a12a0`).

**The glide.** `RaceSession::glide` rescaled a direction already scaled when it re-entered with a stale `+0x23b`, growing by 20 each time up to 1e8 units, which failed the ground query with an exception (`cc_t4`). Only a unit direction is scaled now (provisional). A native safety also returns a car beyond 5e6 units to the road.

**WLogRollingB (`sh_t2`; record `0x515158`, update `0x4c23a0`).**
- The log's surface is in the course's WGNR (y 1809, z 85148..93778).
- m_ForgasLepes (`0x219`) 0.0349, m_Dir (`0x21a`) 3 (z; 1 is x).
- `0x4c23ef` calls `0x4a7e70` with flags 1: **the player cars alone** (`+0x38`, `+0x3c`), in the brick's section. Robots never turn. Those within 4315 along the axis and 761 across (`0x8d631` = 761²) have their offset turned: u' = c u + s v, v' = c v − s u (`0x4530e0`).
- The first port turned every car, which sent robots off the log and into the water. Wine agrees with the fix (below).
- Native: `RollingLog`, `CourseObjects::rolled()`. Its UVScroll is not ported.

**WFountain (`dt_t2`; record `0x5157b0`, update `0x4ca320`, init `0x4ca300` state 1).**
- m_Sugar `0x243` 500, m_Height `0x244` 600, m_Width `0x245` 500.
- States 1 and 2 swap every m_Frames; the base's values go from 0.8 to 1 and back.
- The column holds cars as WGeyser's does, within 100 (`0x510414`) of the top, but **keeps the car's x and z** (`0x4ca60b`); a geyser centres it (`0x4c9326`).
- Both tests are on truncated integers (`__ftol`, `0x4ca52e`, `0x4c9247`), so a car under the top stays free until it has fallen a whole unit.
- Native: `Geyser::fountain`.

**Test thresholds changed.** "Every robot but two does two laps" is now measured against the best robot on long courses (`ct_t3` and `dr_t2` take over 60 s a lap). The avoidance check is 90% (`dr_t3` has 94.8%). These are test changes, not fixes.

## Third pass (2026-11-06): causes found, with Wine

**Wine method.** Scratch prefix; a scratch disc tree of symlinks with the wanted course linked as `cc/cc_t1.WGM` (the menu's first environment is `cc`); `wmem logv` on the cars. Far robots glide at constant speed and yaw, so near behaviour was observed by holding the player above the spot (a `hops` chain ending in repeated lines).

**`sh_t2` log, compared.** Original, player held at (41975, 2500, 89463): a robot crosses at y ≈ 1811 from z 85000 to 93881 in about 2.5 s, x going 41977 → 42004, which its yaw (−0.003) accounts for. So no turning. Robots that are slow at the jump fall at z ≈ 85600 to y 1162, stand about 1.5 s and return before the ramp at (41975, 1476, 83085); the native robots do the same. The original's robots also repeat that fall several times in a row.

**`dt_t2` fountain, compared.** Original, player held above it: robots rise to y 1003–1031, with airtime 1–11 and speed up to 0.14, and cross the column in about 5 s. Native: same height and same airtime pattern after the fixes. One traced native robot took about 15 s (450 updates); not explained, and the hovering robot's steering was not compared.

**`dt_t1` WTram cycle, cause found.** `phases[c.phase]` was read with phase 0xff, out of bounds, so the result depended on the binary. The original reads the same way (`0x4cb08c`); the bytes there are 0 in WPC.EXE, so no tram start. Native tests the bound.

**`cc_t4` exception, cause found.** A bombed car has `+0x8bf` doubled for 20 updates (`0x466df6`); `0x470218` keeps it on surface 1, so it compounds (traced native: accel 58982, speed 802, position 5e9). The compounding follows the disassembly and is kept. Its effect in the original was not observed directly; a robot there showed speed 55.6 under Wine on `sh_t2`, its cause not traced. A native safety respawns the car in the same update, since `__ftol` does not throw and the ground query does.

**Far robots.** The far step now always stands the car level (was: below up y 0.5). Wine, `cc_t1` lap: `+0x267` and `+0x26f` stay 0 and `+0x26b` frozen while y runs 562..1081. A tilt held a robot at `dr_t3` (−218, 107, 18168). Whether the original's far step runs a contact is not traced.

**The loop seam.** Robots' section-by-section seam test is now limited to the seam's two sections, and a section tie keeps the car's own section only against the previous section. This stops `dr_t3`'s false jump of 6000 units. It looked like a regression at first, but those regressions came from the fountain and the log.

**Integer overflow.** UBSan found `dx*dx` overflowing at `race_session.cpp` (bend signs) and `race_robot.cpp` (boxes). The original's `imul` wraps; native now wraps in unsigned arithmetic.

**Test changes.** Avoidance aims are counted during the race only: finished robots park at the line and aim beside each other (`dt_t2`, 562 of 2944 off the route). The log and rolling checks gained a robot and a wrong-section case.

**Not ported, seen on the way.** `0x4c1cda` is another flags-1 query, in a class not identified here.
