# The race random stream and the car update's order (2026-11-14)

Wine runs on `sh_t1` (grid seed 0x1312d6e, the player held still on the grid), memory read every update with
the reader's `farlog`, compared with the native `pack` harness (same course, no input). Harnesses in the
ignored `build/wine-scratch/`.

## The car update turns after it moves

- `0x46ab98` (the forward step) runs along the probe axis `+0x40b - +0x423` placed by the last update's frame.
  The update's angle steps are summed in locals (`[esp+0x80]` for the yaw: steer, slide kick, spin, pushes) and
  added to `+0x267/+0x26b/+0x26f` only at `0x46ba57..0x46ba7f`, after the move; `0x468280` (mode 1) then
  rebuilds the frame and the probes (`0x46baae`).
- Wine, `sh_t1` v0: `+0x26b` turns at update 20 (-1.5953 -> -1.7535), the position follows at 21 along -1.7535
  (offset 0.183 rad, dz -25.6 for dx 138.3).
- Ported: `RaceSession::move` adds the update's yaw after the forward step and the contacts, with the push yaw.
  The start hold goes back to the recovered 20 (`0x4e48d2`); the 21 fitted before was this order.
- With it the turn timing matches on every robot (v0 turns at update 20 in both); the packs stay as close as
  before (v2 within 10 units to update 111 or later, v4 to 38..41).

## Who draws from the race stream (`rand` `0x430e40`, state `0x5472c8`)

Traced with a hook (not in the port): `0x430e40`'s first instruction was replaced in the running Wine process
by a jump to a routine in the int3 padding at `0x406542`. The routine saves six stack words for each call into
a 32-entry ring at `0x2ad3a80`, in the unused tail of `.data`'s last page (virtual size ends at `0x2ad3a64`).
Each caller's return address, logged with robot 2's `+0xb8`, gives this order after the race load's reseed:

| draws | caller | what |
|---|---|---|
| 1..6 | `0x430f60` from the grid | the grid shuffle (ported) |
| 7..12 | `frand` from `0x49f51d` | `+0xb4` (ported) |
| 13..77 | `frand` from `0x457a00` (13 sites, `0x457a78..0x457df3`) | the five `Kosza` creatures' start (ported, below) |
| 78 | `0x467cfa` (routine `0x4679d0`) | vehicle 6's random weapon |
| 79..93 | `frand` from `0x458230` (3 sites, 5 calls) | the `Kosza` birds' first segment change (ported) |
| 94..111 (GO update) | `0x4e48bf`, `0x4e5949`, `0x4e596e` per robot | WCtrlRoboCar init `+0xac` = 100 + rand % 100, then the item code's preference timer (300 + rand % 300) and flag (`frand(1)` < `0x6f2c48`) |
| update 1 | `0x4e9671` x 12 | the goals' link draws, two a robot, during the start hold |
| from update 9 | `0x47058a` | each near car update's engine jitter |
| from update 19 | `0x4e7ab1`/`0x4e7b00` | the decision's factor 80 + rand % 30 |

- The engine pitch `0x47057e..0x4705a8`: when the row top (`0x6f2990`) minus `+0x3ef` is below 0.15
  (`0x512a08`), (rand % 20 - 10) x 0.03 (`0x512a34`) is added to the ratio. It draws from the race stream, in the
  car update, so it shifts every later decision. Far robots skip it: Wine's count drops by one as each robot
  goes far (updates 40..42).
- Within an update the draws interleave a robot at a time: car update (engine), controller (decision), next
  robot (the port now runs them in that order, see below).
- Ported: the GO-update draws (`start_boosts`), the link draws during the hold (the goals update before the
  hold's skip `0x4e6962`), and the engine draw in the near car update. The engine sound's pitch takes the drawn
  value itself (2026-11-14: `RaceCar::engine_jitter`, (rand() % 20 - 10) x 0.03, read by `RaceAudio`, which
  had its own generator before), as the original's `+0x3bb` is computed in the car update from that draw.
- Native counts after GO now follow Wine's update for update on `sh_t1`: with the 80 countdown draws added
  by hand, the native index after each update equals Wine's (144 after update 19, 6 an update).

## The robots' update order (2026-11-14)

- The hook also saved the caller's `esi` and `ebp` (the WCtrlRoboCar in the controller, the car in the car
  update). On `sh_t1` and on `cc_t1` the GO-update init draws and every update's draws come robot 4, 0, 6, 5,
  1, 2: car update (engine draw, `ebp` = the car), then its controller (decision draw, `esi` = its
  WCtrlRoboCar), then the next robot. The cars' addresses rise in the same order (sh_t1: car4 `0x441197c`,
  car0 `0x4413094`, car6 `0x44148e0`, car5 `0x44160a4`, car1 `0x4417cc4`, car2 `0x4419400`).
- That is the order of the WackyAuto templates in `general.wgm`: ids 11, 40, 118, 144, 175, 200, 225 for
  vehicles 4, 0, 6, 5, 1, 2, 3. The player (vehicle 3) comes last.
- Ported: `RaceSession::update_order` (a recovered table) orders the robots' updates and the GO init draws;
  `drive_opponent` now runs the car update first, with the control left last update, then the controller
  (the far test on the moved position, the far step, or the decision for the next update). The first
  decision comes on update 19, as Wine's `+0x2ad`: the GO update's controller counts the hold once. The
  same-update far step and the engine draw's place follow from the order and lost their special cases.
- Result on `sh_t1` (stream offset by the 80 countdown draws by hand): all six first decisions equal Wine's to
  4 digits (v2 -0.0789, v0 -0.2634, v4 0.0630, v1 0.1050, v5 0.0212, v6 0.0951; with the grid order v0 was
  -0.1974); v0 within 10 units to update 33 (was 22), v4 to 47, v6 to 43. A search over the 720 orders, scored
  on Wine's steers over updates 19..60, put v1 and v2 last as well.
- The player's place (2026-11-14): a rand-hook run on `sh_t1` with the tank (vehicle 5; Return x 3, Right,
  Return in the menu) and Up held. The car table `0x6f2ce0` is indexed by vehicle, so the player is car 5 and
  a robot drives vehicle 3. Each update reads `E:car4 D:robo4 E:car5 C:car1 E:car1 ...` (E engine `0x47058f`,
  D decision, C contact `0x46b827`): the player's car update runs at vehicle 5's template place, after
  robot 4 and before car 1, and car 3 (now a robot) comes last. Ported: `update_opponents(input, first,
  last)` runs the robots before the player's place, the player's car update follows, then the rest
  (`RaceSession::template_place`; the hidden robots' section counts carry over as `placed_`). Vehicle 3
  is unchanged (place 6, nothing after it).

## Kosza, the course creatures (2026-11-14)

- The countdown draws were `Kosza` actions, not `Eso` (`Eso` is the rain action on the `GDWorld` of the rain
  courses). `Kosza` (record after `Eso`'s, factory `0x405a97` -> `0x457460`, 0x135 bytes, vtable `0x5125c0`)
  derives from `WGoPath`; its members are `type` (`0xd5`, course id `0xd7`, field `+0xd9`), `mode` (`0xdd` /
  `0xd8`, `+0xe1`), `r` (`0xe5` / `0xd9`, `+0xe9`), `speed` (`0xed` / `0xda`, `+0xf1`) and `wait` (`0xf5` / `0xdb`,
  `+0xf9`); the constructor's defaults are 0, 0, 1000, 100, 0 and state `+0x131` = -1.
- Corpus: sh_t1 and sh_t2 five birds each (GDHModel, type 1, mode 7, r 3000, speed 100, scale 4, round
  (22601, 4172, -27810) on sh_t1); cc_t1 and cc_t2 one mode 0 GDModel and one mode 1 GDHModel (wait 100, on a
  2-point GDPath); dr_t1 one mode 0; dr_t2 five mode 7; dt_t3 five mode 7 and one mode 4; cc_b one mode 1.
- Start (slot `+0x10`, `0x457a00`): modes 4..7 draw four waypoints `+0xfd..+0x129` = the node's place `+0x9c`
  plus frand x r on x and z (and frand x r / 3 on y in modes 5 and 7): 8 or 12 draws. The rate `+0x70` =
  |frame row 2| (`0x43bf30`) x `0x52a980[type]` (0.15, 0.2, 0.00002, 0.15) x speed / D, D = 4r (modes 4..7),
  2 pi r (0, 3) or the GDPath's length (`0x4569d0`, modes 1 and 2). A model with a clip starts it at frame
  frand x 300 (one draw). With a wait: state 0 (`0x458e90`); else state 1 (`0x458f10`), which runs the update.
- Update (slot `+8`, `0x459030`): state 0 moves on frand() < 0.1 twice (`0x510228`, double); state 1 runs the
  mode's mover (modes 4..7 `0x458230`, 1 and 2 `0x457f80`, 0 and 3 `0x458be0`), t `+0x6c` growing by
  `0x5299b4` (2) x `0x512620` (1/3) x rate; `0x458230` redraws the waypoint two ahead (2 or 3 draws) when the
  segment int(4 frac t) changes; a mode 1/2 creature at the end of its path restarts t and waits. State 2
  (after a car hit, scale (1, 0.1, 1)) moves again on frand() < 0.01 (`0x510b48`).
- Wine (sh_t1, the objects found by their vtable and logged each update): t grows 0.004444 an update, is 0.28
  at GO (63 updates, the countdown's and the start's own), and the segment changes at 0.25, 0.5, 0.75 ...
- Ported (`CourseObjects::init_creatures`, `step_creatures`), the update before the cars. Native draws after
  the constructor 77, after the first update 78, at GO 111, after update 19 144: Wine's, on sh_t1; on cc_t1
  (102 at GO, the waiting creature's two-tenths draws each update) Wine's through update 20.
- The curve, the path walk's placement, the clip and the car hit are ported since: `analysis/kosza-motion-20261114.md`.

## The avoidance's caches (2026-11-14)

- `cc_t1` split at update 21: native v0 entered the avoidance of v2 (follow mask 4, aim (2945, -3716)), Wine's
  did not, with the same positions, yaws, goals and modules.
- The avoidance (`0x4e69bd..0x4e72ea`) reads two per-car caches in the WackyAuto, filled on the first request:
  the step `+0x2ed` (float, -1 unset; `0x4e6c76` for our car, `0x4e6db0` for the other: octagonal |dx| + |dz|/2
  of the node's position minus `+0x173`, `_ftol`) and the edges `+0x35d/+0x361` (-1 unset, -2 none; `0x4e6fba`
  calls `0x468890` through `0x4036e8` for the other car first, then ours). The only writer of -1 is `0x469550`
  (WackyAuto vtable `0x51295c` slot 2, `+0x2ed` and `+0x35d`; `+0x361` is left as it is).
- Measured with three hooks in the running Wine process (caves in the int3 padding at `0x406542`, a 32-entry ring
  at `0x2ad3a80` filtered on one WCtrlRoboCar): at the module test `0x4e69fb` (both cursors `+0x2c5`, our
  `+0x2c9`, the mask `+0xc8`), after the ahead test `0x4e6c76` (both positions) and at the step test `0x4e6f06`
  (both `+0x2ed`, the other's `+0x173`):

  ```
  cc_t1 update 21, robot 0:  t3 car2 2518.93 -4477.07  car0 2821.77 -4707.63
                             t2 car2 its 155.00  ours 146.00  car2 +0x173 2540.45 -4622.14
  cc_t1 update 21, robot 1:  t2 car0 its 156.00  (car 0 just after its own controller used 146)
  sh_t1 update 27, robot 4:  t2 car0 its 161.00 ours 125.00;  t2 car2 its 130.00 ours 125.00
  ```

- A robot's own step is always its last update's move: car 0 at `cc_t1` update 21 uses 146 (its update-20
  move; 156 natively before), car 4 at `sh_t1` update 27 uses 125 (its update-26 move; 127 natively), though car
  4 runs first in the order. A car that has had its turn this update is seen with this update's move (robot 1
  reads car 0's 156 right after car 0's controller used 146); one still to come with its last move. So each car's
  caches are cleared at the end of its own turn, after its controller, and the cars after it on that update or
  before it on the next fill them with what was then its latest move. The caller of `0x469550` is not traced;
  the rule is fitted to these values (provisional).
- With 146: 155 - 5 > 146, v2 is pulling away and skipped at 21; at 22 163 - 5 > 156 again; at 23 158 <= 159 and
  the avoidance starts, as under Wine (v0 side 1 mask 4 from update 23 in both). At `sh_t1` 27, robot 4's
  denominator for car 2 is 125 - 130 + 5 = 0, so t is infinite and nothing is avoided; natively (127) it avoided.
- Ported: `AvoidCache` (`race_robot.h`), shared by the cars, filled on the first request in `RoboDriver::avoid`
  in the original's order (our step at the first car past the ahead test, the other's edges before ours), cleared
  per car at the end of its turn in `update_opponents` and after the player's update.
- Result (Wine runs of 2026-11-14 against the `pack` harness, first update a robot is more than 10 units from
  Wine): `cc_t1` v0 185, v4 199, v5 242, v1 157, v6 98, v2 75; control without the cache v0 23, v1 36, v4 45,
  v5 42, v6 48. `sh_t1` v0 163 (was 33), v4 210 (47), v6 177 (43), v1 162, v2 111, v5 106. v0's steers on
  `cc_t1` equal Wine's to 4 digits through the avoidance (0.1435, 0.0787, -0.2807, -0.2178, -0.1615, -0.1151).
- The remaining first splits (`cc_t1` v2 75 and v6 98, `sh_t1` v2 111 and v5 106) are far robots. The far step
  (`0x4e5642..0x4e56a5`, the same rule as the port's) turns by half the goal-2 angle b while it has the goal-1
  angle's sign and is smaller; a far robot that has settled on goal 2 has b halving towards 0, and the update
  it changes sign at rounding level and the robot turns to goal 1 differs (Wine `cc_t1` v2 at 74, natively 78,
  native yaw settled to 1e-7). That is x87 against SSE rounding, not a rule.

## Still apart

- `0x467cfa` (one draw on the first update) is vehicle 6's random weapon (`0x4679d0`, `[esi]` = car 6), drawn
  natively at the same place.
- (The first decisions' mismatch, v0 -0.1974 against Wine's -0.2634, was the update order, fixed above.) The
  decision's draw has a bypass the port lacks:
  `0x4e7a68` takes the draw without the |s| <= `+0x34` (`m_fwAdjust`) test when `[esp+0x8c]` is 2. That local
  (also stored in `+0xa4`, `0x4e644a`) is the result of `0x498610` (thunk `0x401ad2`), a route search between two
  route positions: 3 in the same polygon, otherwise a walk of the route graph with a 0x280-byte buffer giving
  0..2. Not traced further, because it cannot change a decision (2026-11-14): the test it bypasses
  (`0x4e7a72..0x4e7a98`) skips the draw only when `+0x34` < s or s < -`+0x34`, and `m_aFWAdjustAngle` is
  2 pi in every WackyAuto template (`analysis/robocar-ai-20261028.md`) while s is an angle wrapped to
  +-pi. After it, s < 0 (`0x4e7a9e`) and s > 0 (`0x4e7ae9`) both draw 80 + rand() % 30 and s = 0 draws
  nothing: the port's `s != 0` test.
- The player's engine draw (2026-11-14, ported): a second rand-hook run on `sh_t1` with Up held for 25 s
  (`keyhold.py`; the earlier `keys.py Up 25` was a 0.15 s tap, and the player never left the grid) and every
  car-table slot's `+0x3ef` logged. Slot 3 (the player) reaches 0.553 (top 0.58) and its car update draws
  `0x47058f` 87 times (`ebp` = car 3), each the last draw of its update, after cars 5, 1 and 2's: the same
  test as the robots', in vehicle 3's place in the order. Ported after `drive` in `RaceSession::update`.
  The row top is TurboSpeed's while it runs (`0x4d7326` restores it at the end), for the robots too
  (the robots' test used the unscaled row; changed, not compared with Wine).
- The same run shows `0x46b827` (43 draws): a `rand()` in the car-to-car contact (`0x46b822`, while
  building the type 3 push into `+0x48b`) whose result is dropped. It comes before that car's engine draw.
  Ported in `RaceSession::car_contacts`. Neither draw was compared count for count with Wine (the key
  script's timing cannot be matched natively).
- Fixed on the way (2026-11-14): `CourseObjects::step_creatures` had the state changes of states 0 and 2
  inside the comments of their tests, so a waiting creature drew its two tenths but never started. No run
  compared here reached a start (the `cc_t1` packs are unchanged).

## The course objects' draws and the second stream (2026-11-14)

- `0x404a8e` is rand() % n on the race stream (`0x430f60` -> `0x403a85` -> `0x430e40`) and `0x40321a` its
  float (`0x430ec0`). Their callers in the course objects' code: `0x4c48e7` (WLogJam's path), `0x4c6a37`,
  `0x4c6a7a`, `0x4c6b5c` (the doors, `0x4c69b0`), `0x4c7e0a`, `0x4c8da2`, `0x4c8e95`/`0x4c8eb8` (floats),
  `0x4cbba5` (WShiftedRoad), `0x4ce47b` (float, the crane), `0x4cf862`/`0x4cf902` (WRotoHill), `0x4d0a35`.
  The port drew these from a separate generator (`geyser_random_`, seed 1). Ported: `CourseObjects::update`
  takes the race stream and every such draw goes through `draw_random()` (the separate generator remains
  only for the synthetic checks that call update without a stream).
- Their place (2026-11-15, Wine sh_t4 with the rand hook, the stack words naming the wrapped call sites):
  WShiftedRoad's `0x4cbbaa` (10 draws) and WGoldenGate's `0x4c6b61` (38). The race's first logged draws read
  `ROAD GATE 0x467cff GATE ...`; `0x4cbb60` and `0x4c6b10` are the two objects' updates (their vtables) and
  `0x467cfa` is in vehicle 6's car update (`0x4679d0`, called from `0x469550`). So the objects update before
  the cars, as scene actions that the race load's cars follow. Later in the race an object draw falls
  between the last robot's draws and the next car 4's, which fits either order. Ported: `update_objects` runs
  right after the creatures, before any car (it ran after them all, and not at all while the player
  drowned). Their order among the creatures is provisional, and matters on one course only: the
  drawing objects' call sites belong to WLogJam `0x4c48d0` (sh_t3), the doors `0x4c6b10` (sh_t4's
  WGoldenGate, cc_t3), WDicksTrick2's choice `0x4c7e00` (cc_t3), WGeyser `0x4c8cc0` (cc_t3, dr_t4),
  WShiftedRoad (sh_t4), WJunkyard `0x4ce430` and WMagnet `0x4d09e0` (dr_t3), WRotoHill (dr_t2); of those
  courses only dr_t2 has creatures. A dr_t2 run (Wine, 80 s, 2026-11-15) logged the birds' draws
  (`0x4582a3`/`0x4582d2`/`0x4582f7`, 90 each; the start's `0x457b9e..0x457df8`) but no WRotoHill draw (no
  car caught), so the order stays open there.
- `native_drive_test`: all 20 courses pass. sh_t4's paint-shot check aimed at the first solid sphere, which
  belongs to the moving gate; it now aims at a solid that stays put over an update.
- The second stream `0x5472c4` (`0x430ef0`, 20 callers: effects, bees, WFire, `0x486960`, `0x4aac30`) is
  seeded with the race stream's seed by the load's srand (`0x430e20` writes both states). The view's WFire
  and bee draws now share the session's `fx_random_` with the car effects.
- `native_drive_test`: all 20 courses pass; the courses with these objects drive differently (cc_t3,
  dr_t2, dr_t3, dr_t4, sh_t3, sh_t4), the others are unchanged.

## The item slots' place in a car's turn (2026-11-15)

- dt_t1, the rand hook from launch (the race's draws counted from the grid shuffle `0x49d7a3`/`0x49d7b1`,
  the first after the load's reseed): robot vehicle 2 picks up a box at update 23 and its roulette
  (`0x467d60`, `rand() % 9 + 0x11`) is the race's 103rd draw, a BeeHive (18). Natively it was draw 102, a
  Landmine (19), which vehicle 6 then drove over at update 47 (held about 27 updates), and the race diverged
  from there. In Wine's update the roulette follows car 2's engine draw `0x47058f`.
- `0x4679d0` (the slots) is called only from `0x469550`, WackyAuto's vtable slot 2; the car update `0x46a4a0` is
  another slot, reached through its thunk `0x40132f` in the vtable. A car's turn: the car update (engine draw),
  its controller (decision), then `0x469550` (slots, the avoidance caches cleared, see above).
- Ported: `robot_slots` runs after `drive_opponent`; the player's `human_items` runs after its car update and
  engine draw, in its place in the order (also while it drowns). With it the native roulette is draw 103,
  BeeHive, and the stream follows Wine's through update 36 (before: 23).
- The split at update 37 (traced the same day): robot 1's goals and position equal Wine's; its decision picks
  s = a = 0.0565 (Wine's steer -0.0483 fits), but native's pickup turn (`0x4e4a10`) then bent s toward box 7,
  dead ahead: native had filled its power-up list at update 31, counting `+0xcc` from GO. In the original the
  list refresh `0x4e7321` is in the decision path, after the start-hold skip (`0x4e6962..0x4e697d` jumps to
  `0x4e7d60` while `+0xb8` > 0) and the hit check `0x4e53df`, so `+0xcc` does not count during the hold (the
  code's own note: Wine's first list count about 22 frames after GO). Ported: `robot_boxes` only once the hold
  is over and no hit runs. The stream then follows Wine's through update 47.
- At 47 the next split: robot 2's beehive caught robot 6 at update 45 under Wine and 48 natively, with v6 on the
  same line in both. The hive's init `0x4de850` places it at the car less its forward axis x 270
  (`0x512a8c`), then 10 up, as WPU_OilSpill's `0x4ddb90` and the detour's `0x4e3290` do (the landmine does
  not); the port's `drop_oil` (oil and hive) had no offset. Ported; the catch is now at 45 as in Wine.
  `native_drive_test`'s hive and oil checks move the car onto the spot (270 behind, out of range otherwise)
  and check the 270.
- Lap times on dt_t1 after both (Wine 1198 / 2377 / 3579 ...): first laps v0 1195 (1198), v1 1245 (1247), v4
  1220 (1222), v6 1014 (1022), v2 1188 (1244), v5 1476 (1537); later laps still drift (lap 3 within 6.5%):
  under the bees v6's path parted from Wine's from update 48 (before the first kick at 50), and it later ran
  into v5 (both stopped from 66, while Wine's v6 drove on at 0.3).
- The bees' first updates (traced the same day): a Wine reader logging every object with WPU_BeeHive's vtable
  `0x5167e4` (state `+0x34`, `+0x3c`, `+0x68`) shows `+0x68` = 6257.73584 (bits `0x45c38de3`) from the drop to
  the first draw at `+0x3c` = 75, then -0.57753, then 0.20316 at 60. Nothing in the hive's code writes `+0x68`
  before that draw, so the value is what the cloned object holds; added to the steer each update, it pins
  robot 6's at +0.6981 for the first five updates (Wine's `+0x2ad`), and being positive it makes the first
  swing negative. The port started the kick at 0 (steer unpinned, first swing positive). Ported as the
  hive's initial `kick` = 6257.73584 (provisional: one object observed). v6 then follows Wine's swerve within
  6 units through update 56 and 100..200 later, and the stream stays within a few draws to about update 750.
  dt_t1 laps (Wine / native): v6 1022 / 2168 / 3341 against 979 / 2102 / 3201, v0 1198 / 2377 / 3579 against
  1198 / 2504 / 3684; the order is now Wine's (6, 2, 0, 4, 1, 5), lap 3 within 4.3%. `native_drive_test`: all
  20 pass.
- sh_t2: robot 6 is now struck by the storm while far and held drowning at 1 in the pocket for good, the
  behaviour recorded under Wine on 2026-11-14; the far-robots check counts such a robot apart.
- Lap times after the change (Wine as before): ct_t1 and cc_t1 unchanged (no roulette before their splits);
  dt_t1 v0 1194 / 2445 / 3571 (Wine 1198 / 2377 / 3579), v6 995 / 2120 / 3219 (1022 / 2168 / 3341), v2 1166 /
  2401 / 3652 (1244 / 2429 / 3556): the first laps move toward Wine's for v0, v6 and v5, v2's away; the spread
  stays within 3.7%. `native_drive_test`: all 20 pass.

## dt_t1 after the bees (2026-11-15)

- Robot 1's first lap (native 1337, Wine 1247): it follows Wine's path within about 100 units to update 891,
  is hidden at 892 and shown at 975 in both builds (shown at (1188, 22997), Wine (1183, 22988)). At the show its
  first goal is Wine's (1013.3, 23534.6) but the second is not (native (644.7, 23932.9), Wine (588.5, 23565.3)):
  a fork's link draw (`0x4e967b`), from a stream by then a few draws off, so native takes the flat branch and
  Wine's climbs (y 38 -> 620).
- The stream's offset (Wine's far-log state against native, one update apart) stays within +-3 to update ~680
  (partly the reader's timing) and settles at about -8 from 690. Wine's draws there are far robots' link draws
  only (15 in updates 675..703), which come when a far robot reaches a goal; the far robots' positions differ by
  up to ~100 units by then (v1 87 at update 400, 108 at 500), the far step's rounding-level difference noted
  above (x87 against float), so the goal changes fall on other updates. No rule difference found past update
  48.
