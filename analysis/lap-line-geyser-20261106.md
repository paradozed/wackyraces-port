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

## The fountain and far robots (2026-11-12)

- **Seen under Wine** (`dt_t2`, player idle on the grid, two runs): vehicle 6 reaches the fountain at
  (55095, 460, −4616) in the far mode (`+0x2b1` = `0x1000`, gliding at 0.775) and is caught at 27.08 s.
  The hold zeroes its speed `+0x3ef`, its steer `+0x403` and its airtime. From then on it stays,
  moving to and fro about ±150 around (55400, −4400), for the rest of each watch: 13 s and 33 s.
  The five other robots, also far, cross in about 0.5 s.
- **Native:** every robot is caught too (speed 0, airtime 0), but the far step keeps moving it about 100
  an update, so it leaves the column in about 10 updates.
- **The far step** (`0x4e54fa..0x4e5524`) uses the vehicle row's top speed × `+0x163` × the probe span
  × 0.56/0.48, not the car's speed, as natively. The two skips before it (a running bomba
  `0x4e53e5`, `+0x1c7` = 5 `0x4e53f0`) were not seen set.
- **The hold** (`0x4ca5b6..0x4ca647`):
  - it takes the car's x and z from its node's cached world matrix (`0x4200e0` → `0x43bf60`), writes
    them with the new height into the position copy `+0x288`, and rebuilds the frame `+0x2c` from it
    and the angles (`0x468620`);
  - it also clears the slots (`0x472650` on `+0x48b`).
- **Tried and withdrawn:** pinning a far robot's x and z to where the fountain first caught it (standing
  for a cache not refreshed while undrawn). Every native robot then stuck at the fountain for hundreds of
  updates, where Wine held only one of six. So what holds vehicle 6 is not that rule alone.
- **The cache, checked under Wine (third run):** for vehicle 6 and vehicle 2 the frame translation
  `+0x54`/`+0x5c` and its copy `+0xc0`/`+0xc8` are equal on every sample, before and during the hold. A
  stale cache is not seen in that copy, which argues against the reading above (the copy that
  `0x4200e0` returns was not identified for certain).
- **The approach differs:**
  - vehicle 2 comes down at x ≈ 54810, heading south, and crosses the column in about 1 s;
  - vehicle 6 first swings west to (54072, −2873), comes back diagonally south-east, and enters the
    column at about (55260, −4550), some 450 east of the others' line;
  - held there, its position goes to and fro about one far step (±150), as if its far turn (half
    the angle to the goal) flipped every update toward a goal it has overshot;
  - the port's vehicle 6 comes in on the others' line, (54690, −4048) → (54751, −4131), and passes.
- **The far turn, compared statically** (`0x4e5556..0x4e56b8`), matches `RoboDriver::far_turn`:
  - the angles to the two goals (`+0x58`/`+0x60` and `+0x64`/`+0x6c`) are wrapped into ±π
    (`0x512660`/`0x512618`/`0x510b54`);
  - on the same side the smaller one is taken, otherwise the first;
  - × 0.5 (`0x51026c`), and no turn without a route polygon (`+0x2c5` = 0).
  So the difference is in the goals, which follow the random link choice (`0x4e9610`).
- **Native over 8 random seeds:** no robot stays at the fountain longer than 60 updates. The
  original's vehicle 6 was the one held in all three Wine runs, which fits a random stream that starts
  the same way each launch.
- **Where it splits:**
  - all robots in both builds come down the same line: at z ≈ −2700, Wine x 54078–54102 and native
    54050–54100;
  - the 48 native entries into the column, over 8 seeds, are at x 54787–54879;
  - under the fountain the route polygon 132 (section 2) forks to polygon 327 (weight 32767) and
    polygon 165 (65535), so each is taken about half the time;
  - natively 29 of 61 passes take the eastern side and leave heading south-east, to about
    (55450, −5280);
  - Wine's vehicle 6 instead turns to (55261, −4552), north-east of the fountain's centre
    (55095, −4616), and is held there going to and fro.
- **So** the original's robot aims at another point for that link, one it then circles. The goal
  points come from `0x4026fd` (called at `0x4e556e` with `+0x2c5`/`+0x2c1`), the native
  `RoboDriver::update_goals`.
- **That routine** (`0x4026fd` → `0x4e9750`) runs only when the car's route polygon changes
  (`[esi+0x28]`, `0x4e9763`).
  - It picks the next links (`0x40552e`, into `+0x50`/`+0x54`); with `+0x1c7` at 9 or 10 it takes another
    path (`0x401ad2`).
  - The first goal (`0x4e9a1c..0x4e9a7b`) is the link's stored point (shorts at `+4`/`+6`/`+8`), carried
    by a matrix built from the route brick (`+0x2bd`, ×`0x5129bc`). It is then put in the world through
    the route node's **cached** world matrix (`[car+0x2b9]` → `0x404def` → `0x4200e0`), the per-group
    cache that the loop seam's placement uses (`0x5472d0`).
- **A lead, not checked:** `dt_t2` has three sections, and the fountain stands in the last one. With
  the player idle in section 0 that group is marked for the seam (`analysis/section-culling-20261107.md`),
  so a goal taken through the cache may be wrapped. Against it: all six far robots pass that polygon,
  and only one was held.
- **The goals, read under Wine (fourth run).** Vehicle 6's controller was found by scanning for its car
  pointer (the controller holds it at `+0x24`); its goals are at `+0x58`/`+0x60` and `+0x64`/`+0x6c`.
  For 40 s, held, its first goal cycles between three points:
  - (55308, −4186), the goal of polygon 270, which lies just east of 132 (flags `0x7900`), back into 132;
  - (55296, −5033), polygon 132's link to 327;
  - (54690, −5016), polygon 132's link to 165.
  The second goal follows the same links further on: (55763, −5802), (54299, −5859).
- **The port's link points are the same, to the unit** (`route_point` of each link: 354 → 132 at
  (54809, −4194); 132 → 327 at (55296, −5033) and → 165 at (54690, −5016); 270 → 132 at (55308, −4186)).
  So the goal computation matches, and the lead about a wrapped cache is ruled out.
- **The trap is a far-mode loop between polygons 132 and 270:**
  - in 270 the goal leads back west into 132;
  - each re-entry into 132 recomputes the goals and draws a new link (`0x4e9763`);
  - the half-turn step, about 100–200 a sample, overshoots back into 270;
  - the car crosses the boundary near x ≈ 55400 every few samples.
- **Why only vehicle 6:** it alone entered polygon 270. As the fountain caught it, it moved about 440
  east-south-east in 0.15 s, (54917, −4274) → (55261, −4552); vehicle 2 nearby moved about 240. No native
  robot entered 270 in 48 passes.
- **No jump at the catch:** per frame, vehicle 6 moves about 108 before and after it (26.77 → 26.93 →
  27.08 s, 434 and 441 units in about 4 frames each), the normal far step at factor 2.
  - Its approach curve, (54072, −2873) → (54406, −3569) → (54917, −4274), swung wider and passed east of
    polygon 354's goal (54809, −4194).
  - In 132 it drew the link to 327 (goal to the south-east) and drifted on into 270.
  - Vehicle 2 came in further west, (54802, −4309), drew the link to 165 and turned.
- **Conclusion:** the goals, the link draw and the far step agree with the port. The trap needs that
  wide approach plus the eastern link, which no native pass produced. Not identified: what makes the
  original's approach to 354's goal wider for that robot (its far turn's starting heading). No port
  change.

