# A `cc_t1` race driven in both builds (2026-11-12)

The same closed-loop driver ran in the original (under Wine) and in the port. Only Championship I's
`cc_t1` was driven:
- native: vehicle 3, `RaceSession(track,3)`;
- Wine: the menu's default selection, reached with Return presses. Its vehicle was not read, so the
  two vehicles may differ.

## The driver

- **Wine:** the memory reader `wmem.py drive` (the scratch tooling in `analysis/runtime-capture.md`).
  It reads the car at `+0x2c` (13 floats: the forward axis is (v6, v8), the translation v10/v12) and
  presses keys through XTest about every 50 ms.
- **Native:** a scratch harness, linked against the port's race sources, that calls
  `RaceSession::update` with the same decisions on two updates out of three (20 Hz).
- **The rules, the same in both:**
  - The target is the first waypoint at least 700 units ahead. The nearest waypoint is searched 25 points
    ahead of the last one (the whole route only beyond 4000).
  - Right/Left beyond 0.12 rad of error. Up below 0.45 rad, or when the car has stopped. Down above
    0.9 rad when moving.
  - Escape: when the index has not moved for 3 s, one neutral sample, then Down with a turn for 2 s.
  - Across a loop-seam jump in the waypoints (two consecutive points more than 3000 apart), the target
    is 1500 units on along the last points before it. The seam does not change the course frame: past
    it the car is back near the start in the same coordinates.
- **The waypoints:** robot 6's line, sampled under Wine every 5 updates. Lap 1 runs up the start
  straight (x ≈ 2900); from lap 2 the robots take the western branch (x ≈ 500, through (582, −7190)). The
  route file holds lap 1's points (357), then lap 2's (363) as the loop for every later lap.
  - The first versions used lap 1's points cyclically. On later laps the car then drove from the
    western branch's end towards the start straight and hit the wall near (1900, −6800), circling
    across the line. That was the "no crossing after lap 1" of `analysis/runtime-capture.md`.
- **Two driver faults, fixed in both builds:**
  - The brake was held from braking straight into reversing. After a stop, brake state 1 keeps the
    speed at 0 until the key is released (`0x467127`, `game_drive.cpp`), so the native car sat
    still for 120 s.
  - The "moving" test measured the distance from a lagging anchor instead of the last sample.

## The frame rate

`0x4ad540`, once a frame:
- it stores the frame's start time at `0x214c3a8`;
- the factor `0x5299b4` = ⌊Δms × 0.06⌋, clamped to 2..4;
- the loop at `0x4ad5d0..0x4ad5e8` waits until 33 ms (`0x21`) have passed since that start.

So the original never runs above 30 frames a second. Any frame under 50 ms advances 2/60 s of motion.
Under Wine (software GL on Xvfb) a race frame takes about 37 ms, so the game runs at about 0.9 of real
time: lap times in seconds are longer, but the motion per frame is the same. The port runs the 30 Hz
the limiter aims at (`race_update_seconds`).

The Wine driver counts frames with a thread that polls `0x214c3a8` every 2 ms.

## Results

- **Native**, 300 s run: three laps, of 1611, 1564 and 1523 updates (53.70, 52.13 and 50.77 s; race time
  156.67 s at the third), no respawns. The seam was crossed at 50.80, 103.30 and 154.07 s.
- **Wine**, counted run:
  - lap 1 (from a standing start on the grid to the crossing count `+0x19f` 1 → 2) took 1558 frames,
    57.83 s of wall time (26.9 frames a second), 3.3 % fewer than native's 1611;
  - on lap 2 the car went wide at the left-hander near (−58500, 5000) and stayed against the outer wall
    near (−57500, 4570) for more than 40 s.
- **Wine**, an uncounted run: laps of 99.6 s (35 s stuck near (−20000, 20000)) and 60.8 s; at 26.9
  frames a second, lap 2 would be about 1635 frames, against 1564.
- **Speed per frame:** the Wine samples' step lengths cluster at about 110–120 and 220 units: one and
  two frames at top speed. Native moves 108–117 units an update at top speed. The motion per frame
  agrees.

## Not established

- **Laps 2 and 3 in frames:** only lap 1 was counted under Wine. The 3.3 % (lap 1) and the estimated
  4.5 % (lap 2) are within what the driver's timing could explain: 50 ms is about 1.35 frames under
  Wine and 1.5 updates in the port.
- **The two Wine stalls:** native went off line at the same left-hander (heading error 3.09 rad at
  93 s) but was back on the route within 2 s. One run each does not tell the wall response (TODO:
  WackyAuto's wall/contact response) from the driver's timing.
- **Not compared:** the standings, the robots' finishing order and the finish screen of this driven
  race.

## Second attempt at laps 2 and 3 (2026-11-12, later)

The scratch tooling was rebuilt after `/tmp` was wiped. The route is now robot 0's lap 1 recorded in
the port (`RaceSession`, robots always near, a point every 5 updates): 248 points that close on
themselves, one seam jump, and it stays on the start straight every lap.

- **Native,** with the same driver: lap 1 in 1531, then 1514 and 1496 updates as the driver was tuned:
  - start at waypoint 0 rather than the nearest point, which is the route's last one, behind the grid;
  - brake when the heading error is over 0.6 at more than 60 units a sample.
  Lap 2 never ended.
- **Where it stalls:** the left-hander at waypoints 59–64, near (−19000, 18100) to (−21000, 18750).
  - On lap 2 the car arrives at full speed (0.58). It turns as told (yaw 0.64 → 1.71 with Left held), but
    it slides outward to z ≈ 19800, about 1000 north of the road.
  - From there it pushes south into a wall at speed 0.19 without moving, and the driver's
    reverse-and-turn escape does not get it out.
  - A longer lookahead (coast above 0.5 and brake above 0.8 of error to the point 2000 ahead) put lap 1
    into the same pocket, so it was withdrawn.
- **Wine,** the same route: started from the nearest point (waypoint 247, behind the grid), the car
  crawled with Left held (it barely turns at a crawl), was reversed to the east by the stuck rule, and
  stayed against a wall near (4000, −4300) for 150 s. The waypoint-0 start was not run under Wine.
- This corner is where an earlier Wine run stalled for 35 s, near (−20000, 20000). So the pocket
  catches the driver in both builds.
- **Not done:** laps 2 and 3 counted in frames under Wine. The next step is a better route through
  that corner (an inside line, or speed control from the curvature ahead), not a port change.

## Third attempt: speed control and the steering creep (2026-11-12, later)

- **Speed control:** a target step of K / (the route's total turn over the next 2000 units).
  - Before the creep was ported, K = 40 drove the port round all three laps: 2185, 2122 and 2088
    updates. K = 60 and 90 stalled.
  - Under Wine the same driver crept on the grid for 8 s, then stalled 150 s at waypoint 151, near
    (−52600, 11300), circling.
- **The creep:** the grid behaviour led to the steering creep, now ported (`analysis/steer-creep-20261112.md`).
  - With it, the port's car stalls at the same waypoint 151, near (−52340, 11200): the two builds now
    agree there.
  - In the escape, a steering key at a speed below 0.1 creeps forward (`0x4677da`), so reversing
    with a turn just circles, in both builds. The drivers now reverse straight and steer only once
    the speed is negative.
  - It still stalls. On the hairpin after waypoint 150 the car brakes and steers right, and is
    carried about 740 west, outward, into a pocket behind a wall.
  - Traced update by update (native): two wall impacts at 42.03 s, then from 42.23 s a glide
    (`player_glides` 1, the slide off a wall-flagged slope `0x4721f0`). For 27 updates the yaw stays at
    −0.405 and the car moves a constant 27.5 a update, whatever its speed and keys, until the glide
    ends. Wine's car ended in the same pocket, and the original's glide is known under Wine
    (`sh_t1`). So this is the course and the driver's line, not a port difference.
  - Waypoints 145–153 moved 200 or 350 west, away from the slope, did not get past it either.
  - K = 20, 25, 30 and a lookahead shortened at low speed did not get past it.
- **Not done:** laps 2 and 3 in frames under Wine. Since the same driver now stalls at the same place
  in both builds, a route through that hairpin (an inside line) is the next step.

## `sh_t1`: lap 2 counted in both builds (2026-11-12, later)

The same driver (speed control K = 40, the creep-aware escape) on robot 0's line on `sh_t1`, recorded
natively. Both runs start from the grid after 25 s idle.

| | Original (frames, `+0x19f` and the frame counter `0x214c3a8`) | Port (updates) |
|---|---|---|
| Lap 1 (from the standing start) | 2225 | 2374 |
| Lap 2 | 2029 | 2701 (one reset) |
| Lap 3 | not clean (see below) | 2184 |

- In the original the crossing count went 1 at frame 281 (the start line), 2 at 2225 and 3 at 4254,
  and then stayed at 3, the last lap (the finish crossing does not raise it).
- After lap 2 the route index wrapped every 1000–1500 frames, so the car was cutting or being reset.
  Those laps are not counted.
- **The original is 6–7 % quicker per frame on comparable laps** (lap 1: 2225 against 2374; Wine's lap 2
  against the port's clean lap 3: 2029 against 2184). That fits `cc_t1`'s lap 1, 3.3 % quicker
  (1558 against 1611).
- **Separated the same day: it is the driver's timing.** The port harness was set to decide every
  1.35 updates, which is what 50 ms is under Wine at about 27 frames a second:

  | Port, decisions every | Lap 1 | Lap 2 | Lap 3 |
  |---|---|---|---|
  | 1.35 updates | 2076 | 2046 | 2013 |
  | 1.5 updates (the default) | 2718 | 2283 | 2239 |

  - Wine's lap 2, 2029 frames, is within 0.8 % of the port's 2046 at the same rate.
  - Lap 1 differs by the standing start (Wine 2225 is the slower here).
  - So the 3–7 % was the driver, whose laps move by 10–30 % with its decision rate, not the car. That
    agrees with the matched speed per frame (about 110–120 units at the top).
- **Lap 3 in the original, found in the same log:**
  - after lap 2 (161.46 s, frame 4249) the car follows the route continuously, through the loop
    seam at about 221.5 s, back to the start at 222.91 s, frame 5808. That is the finish: the crossing
    count stays at 3;
  - the later wraps every ~1000 frames are the car driving itself after the finish (a finished car is
    robot-driven, `[+0x18]+0xc0` = 2), not laps;
  - so the original's lap 3 is **1559 frames**, against its lap 2 (2029) and the port's lap 3 at the
    same decision rate (2013);
  - every stretch of lap 3 is quicker than lap 2's: waypoint 102 → 223 in 27 s against 36 s. There is
    no shortcut in its path.
  - Per frame on the same waypoint stretches (laps 2 / 3, in units a frame): 0–40 55.1 / 55.5, 40–80
    62.4 / 70.8, 80–120 70.6 / 104.4, 120–160 95.8 / 78.0, 160–200 47.5 / 124.6.
    - Lap 2 lost time at 160–200 (459 frames against 171): held up there.
    - Lap 3's 124.6 a frame is above the 110–120 measured at top speed, so something sped the car
      (a boost, or a push).
  - So neither of the original's laps is a clean match: lap 2 has a hold-up, and lap 3 a faster stretch.
    The port's laps at the same decision rate (2046, 2013) lie between them.
- **The fast stretch, found in a repeat run** (2026-11-13; the player's `+0x3ef`, `+0x385` and `+0x19f`
  polled every 10 ms):
  - the player's effect `+0x385` becomes 8, TurboSpeed, at 198.48 s on lap 3, at (21943, −42581);
  - the speed climbs to 1.276, against the Dingo's top of 0.58, until the effect ends at 200.94 s;
  - this run's crossings came at frames 296, 2009 and 3730.
- **The driver never presses the gadget key.** The saved MenuPrefs (`system.reg`, record 0x178) bind
  player 1 to the defaults: the arrows for accelerate, brake, left and right (actions
  `0x40000`/`0x80000`/`0x10000`/`0x20000`), Return, `.`, `/` (the gadget, `0x800000`) and right Shift.
- **The spot** is the refill pickup power-up 564, which the port's player also takes on every lap at
  (22064, −43263). The original's refill branch (`0x46ab42..0x46ab58`) only empties the slots
  (`+0x39f`, `+0x3a0`, `+0x371`, `+0x375`).
- **The port's player never gets effect 8** on the same drive (effects logged).
- **Open:** what starts the player's TurboSpeed in the original without the key. The TurboSpeed action
  writes `+0x385` = 8 at `0x4d6d4d` and `0x4d7302`; their callers and conditions are not traced.
  (The port's charge of 4 in that log is the moment between the pickup's increment and the item
  update, which caps it at 3, as the original's `0x46809f` does; not a difference.)


## The TurboSpeed explained: the race ends once every robot has finished (2026-11-13)

- **The repeat run's log** (`+0x385`, `+0x371`, `+0x375`, `+0x399`, `[car+0x18]+0xc0` polled every 10 ms):
  the player's controller flag `[+0x18]+0xc0` becomes **2 at 193.32 s**, mid-lap 3 at (21203, 1178), before
  the TurboSpeed (198.48 s) and long before the player's own last crossing (211.61 s). With the flag at 2
  the car is robot-driven: the robot item code used the player's slots, which is the TurboSpeed seen
  without a key press.
- **What sets the flag:** the race state handler `0x4a2dbd` calls the lap pass `0x4015be` and the rank
  update `0x405b91` = `0x4a8a10`. If the lap pass returns more than 0 or the rank update returns non-zero,
  state `+0xf30` = 4 and `0x403bd4` = `0x49eb20` writes 2 into the player car's `[+0x18]+0xc0`
  (`0x4a2df8`; `0x49eb20` also sets bit 2 of the car's `+0x1c3` object).
- **The rank update `0x4a8a10`:** it sorts the cars by progress `+0x1a7` into `+0x7f0` and writes the ranks
  `+0x1af`; then (`0x4a8b2f..0x4a8b61`) it counts the cars in the list at `+0x60` (stride 0xc, count
  `+0x40`) whose lap `+0x19f` is above the lap count (app `+0x1a70`), and returns 1 when all are, with a
  non-empty list.
- **The list:** filled at `0x49d6ec..0x49d752` from the car table `0x6f2ce0`, skipping empty slots and the
  player (`[+0x38]`), up to the robot count (`[edi+0x8c]`); the cars beyond it are parked (flag 0x100,
  `0x49d757`). So it holds the robots. In two players `0x49d949` and `0x49d977` set other counts; the
  port's two-player race has no robots (its rule needs a non-empty list).
- **Native:** `RaceSession::check_lap` (the lap count) ends the race when the last robot finishes in a
  single-player race with robots: phase Finished and `finish()`, as when the player finishes. The player
  is then last in the standings (finished robots rank ahead by their finishing update).
- **Checks:** `native_drive_test` on the 20 courses all pass (2026-11-13). Before the test was adjusted,
  `dt_t4` and `sh_t2` failed: every robot finished before the test driver (on its laps 3 and 2), which
  is the control showing the rule acts; the test now accepts that end when every robot has 3 laps and
  the player is 7th. The 18 other courses are unchanged (the test driver finishes first in the race of
  its own laps).
- **The player's autopilot after the end (provisional, 2026-11-13).** With `[+0x18]+0xc0` non-zero the speed
  update takes the robot branch on the throttle byte `+0x2ac` (`0x466e0b`), and the steering copies `+0x2ad`
  (`0x467886`). Only WCtrlRoboCar code (`0x4e5332..0x4e7d28`) writes those two fields, so a robot
  controller drives the player's car; which object that is for the player was not traced. In the Wine run
  the car drove on to its crossing at 211.61 s and kept lapping (wraps every ~1000 frames): its lap count
  presumably stays below the finish (inferred: the lap pass `0x4015be` is called from the state-3 handler
  `0x4a2dbd`, not traced in state 4), so the finished robots' brake
  never applies to it.
  - **The controller, found under Wine** (2026-11-13, `cc_t1` race, memory census): the seven WackyAuto
    objects (vtable `0x51295c`) and seven WCtrlRoboCar objects (vtable `0x516ae4`, constructor `0x4e4320`,
    made by the class factory at `0x4e3a35`). Each car's `+0x1c3` points at its own WCtrlRoboCar, the player's
    car included:
    ```
    car 4532a44 ctl 0440abc4 vt 005129a0 flag 00000000 1c3 045342a4   (the player)
    car 451c154 ctl 0440197c vt 005129a0 flag 00000001 1c3 0451d9b4   (a robot; five more alike)
    robo 45342a4 car(+0x24) 00000000                                  (the player's, not yet attached)
    robo 451d9b4 car(+0x24) 0451c154
    ```
    The race setup sets bit 2 of each robot's `+0x1c3` object `+0xc` (`0x49d73d`); `0x49eb20` sets it on the
    player's when it writes 2 (`0x49eb47`). So the autopilot is the player car's own WCtrlRoboCar, enabled at
    the end. Not traced: where its `+0x24` gets the car (0 during the race).
  - **Its items:** WCtrlRoboCar writes 5 into the gadget state `+0x3a0` (TurboSpeed `0x4e5a2a`: the slot's item
    `+0x371` is the TurboSpeed, top speed not boosted, wait `+0xb0` 0, not battle) or the weapon state `+0x39f`
    (`0x4e5edb`/`0x4e5f33`: item `+0x375` set, charge ≥ 1, no running weapon `+0x36d`, gadget state not 5).
    The car's item update takes state 5 for any controller: charge − 1, the item's action, state 4 (`0x467c9c`,
    `0x46803b`). That is the TurboSpeed of the Wine run.
    - Native: `human_items` has state 5 for both slots, and `autopilot_items()` makes the robot decisions on
      the player's slots while the autopilot drives (before the item update, as `robot_items` does). The gates
      use the slot's state 3 and a charge above 0 for "the slot holds an item" (the original tests the item
      field; provisional). On `dt_t4` the autopilot fired weapon 14 within 300 updates of the end; with the init's item wait (below) it
      fires none in those 300 updates (all 20 courses pass, 2026-11-13; after the init `sh_t2` drives 13709 units
      with one respawn, `dt_t4` 19938).
  - **Native:** `RaceSession::autopilot()` (phase Finished in a single race, not practice or two players):
    `drive()` asks a `RoboDriver` (`autopilot_`, given the route forks as the robots are) for the control,
    then runs `update_robot_speed` and `robot_steering` instead of the keys. A player who finished
    (`body_.finished`, the lap count reached before the end) drives on too (see the Wine check below).
  - **Its init** (2026-11-13): `0x4e45e0`, WCtrlRoboCar's vtable slot 4 (`0x516ae4` + 0x10, thunk `0x40535d`; no
    direct caller). It writes the controller's `+0x8c`/`+0x94`/`+0x9c`/`+0xac`, attaches the car (`+0x24` =
    `+0x10`), doubles the car's gravity `+0x98`, and in a single race or championship (app `+0x1a60` 0 or 3) with
    the manager in state 4..7 (the race has ended, `0x4e468c..0x4e46a4`) sets `+0x30` = 0, row `+0x10` × 1.0
    (`0x52e66c`), m_rDirModifier 0.25 (`0x52e62c`) and the item wait `+0xb0` = 100 + rand() % 100. It then compares
    `+0x24` with the player's car (`0x4e4704`). So the end-of-race branch is the player's controller's init.
    That the init runs when `0x49eb20` enables the controller is inferred (the player's `+0x24` is 0 during the race).
    - Native: `start_autopilot()` right after the end: `tuning_.gravity` × 2, the autopilot's dir modifier 0.25,
      its item wait 100 + rand() % 100.
  - **Not ported:** the rank scaling (`0x4e4f6b`) on the player's car.
  - **Check:** `native_drive_test` copies the ended race and runs 300 more updates: on `dt_t4` the car
    drives 22619 units (one seam jump), on `sh_t2` 11143 (two respawns: the autopilot sticks at the same
    spot near (42700, 92500) as the test driver did, which made it last). Control: with `autopilot()` forced
    false the `dt_t4` car coasts 2052 units to speed 0 and the check fails. Not compared with Wine.
  - Two `dt_t1` checks parked the player while the robots raced 3 laps; the race now ends there, so the idle and
    hidden-robot loops stop at the end and the tram loop keeps the car parked. All 20 courses pass (2026-11-13).

## Finished cars drive on; the init confirmed (Wine, 2026-11-13)

Two runs on `cc_t1` with the memory reader. In the first, the player's lap `+0x19f` was set to 3 at the grid;
in the second, every car's was. Then Up was held for 15 s, so the first crossing finished the race.
- **The player's WCtrlRoboCar** (`+0x1c3`) before and after the end:
  ```
  start:   robo+24 00000000 +30 0 +b0 9400ffff +b4 3f800000 | ctl+c0 0 ctl+98 3d4ccccd | lap 0 | mgr f30 3
  after15: robo+24 04532a44 +30 0 +b0 00000000 +b4 3f800000 | ctl+c0 2 ctl+98 3dcccccd | lap 4 speed 0.584 | mgr f30 5
  ```
  The init `0x4e45e0` ran: the car is attached (`+0x24`) and its gravity `+0x98` went from 0.05 to 0.1. The
  player's `+0xb4` is 1.0, against a robot's 1.234 (`3f9df01a`), so a rank boost would leave its top speed.
- **Every car keeps driving after its finish.** 15, 25 and 40 s after the end, the robots at lap 4 run at 0.56
  (`3f0f5c29`) in the far (`+0x2b1` = 0x1000) or near (2) mode, and their positions move by thousands of
  units. The player's car (lap 4, `+0xc0` = 2) runs at 0.43–0.58. No lap count changed in those 40 s.
- **The stop state** `+0x2b1` = 0x10000000, which brakes a robot to a stop (`0x4e532b..0x4e5389`, then
  0x20000000 at `0x4e5396`), is written only by the manager's battle and time-limit ends (`0x4a317e`,
  `0x4a3188`, `0x4a31d5`, `0x4a31df`). Nothing writes it when a car finishes.
- **Native, before:** a finished robot braked to a stop and parked past the line (`robocar-ai-20261028.md`
  called this "states 0x10000000/0x20000000"), and the far mode skipped finished robots. **Now:** a body flag
  `stopped` (set only by the battle's end) brakes; finished robots and the autopilot drive on, far or near. A
  finished car counts no more laps (provisional: no lap change was seen within the 40 s).
- **Rank scaling and items after the finish** (static, 2026-11-13): WCtrlRoboCar's update `0x4e4f10` runs the
  rank scaling (`0x4e4f52..`, single races and championships) and, in the near mode, the item code (`0x4e53d4`
  → `0x4e5931`) with no lap or finish test. Only the stop states return first (0x10000000 `0x4e532b`,
  0x20000000 `0x4e53a8`). The port's `!b.finished` guards on `scale_by_rank` and `robot_items` are removed
  (the item guard is now `!b.stopped`). The autopilot gets the same scaling (`autopilot_scale()`). Its `+0xb4`
  is 1.0, so a boost scales only row[4] (× 0.3) and sets the dir modifier to 0.4. Leaving the boost restores
  them and 0.25. The init clears `+0x30` (not boosted).
- **Checks:** all 20 courses pass `native_drive_test` (2026-11-13). `dr_t3` now also ends by the robots (the test
  driver on lap 3, 7th; before it finished 6th): with finished robots no longer parked, the last robot is no
  longer held up past the line. After the end the autopilot drives 23351 units in 300 updates on `dt_t4`, 8982
  on `sh_t2` and 17032 on `dr_t3`.
  - Checks: all 20 courses pass (2026-11-13). After the end the autopilot drives 20493 units in 300 updates on
    `dr_t3`, 23351 on `dt_t4` and 11531 on `sh_t2` (still two respawns at the pocket).

## The autopilot against Wine (`sh_t1`, 2026-11-13)

- **Setup, both builds:** player 1's lap set to its last (Wine: `+0x19f` = 3 at the grid; native:
  `debug_last_lap()`), Up held from the start until the finish, then nothing. Wine logged the player's
  position, lap, speed and `[+0x18]+0xc0` every 10 ms for 75 s (`ap_watch.log`). Native ran a scratch harness
  linked against the port's objects, printing every 60 updates.
- **The finish:** both at (−16808, −1457), native after 84 updates.
- **Wine after the finish:** about 0.56–0.58 all along the route: (−6448, −1475) at 4 s, (−267, −5707) at 8 s,
  (−2729, 9286) at 14 s, (22020, 972) at 24 s. A TurboSpeed (1.27) on the straight south of (22374, −13021)
  at 28 s, then a fall at (26371, −24601) and a return at (22599, −11240) at 32 s. The lap wraps at 49 s, and at
  54 s the car sticks at (−6117, −3073) at the first hairpin, speed 0.164, for the 20 s left.
- **Native, first run:** the same route, but the speed fell to 0.1 or reversed every few hundred updates; the
  far hairpin came at about update 1020. The autopilot's stall counter (+0xa8: 100 updates with less than
  50 units moved since `+0x173`) read the position at the start of this update, so it always counted. After
  101 updates the unstick (30 back, 30 forward at ±5°) started. Fixed: `+0x173` is the last update's start, as
  the robots pass it.
- **Native, fixed:** 0.56–0.58 along the route, (22020, 972) at about update 650 (21.7 s; Wine's 24 s are
  ~21.6 s of motion at its ~0.9 pace). It falls into the water past (23453, −30811) and returns near (22231, −16696).
  It does not stick at the first hairpin on its second lap. Wine's car did, a matter of line, not checked further.
- **Pickups after the end:** the Wine car took its TurboSpeed after the finish (it had no item at the
  crossing), so the car update's pickup `0x46aad5` runs after the end. The port took pickups only while
  racing; now in the Finished phase too. The native autopilot then takes power-ups and uses weapon 14,
  MonsterTires and weapon 22 within 1800 updates.
- Checks: all 20 courses pass (2026-11-13). After the end the autopilot drives 28759 units in 300 updates on
  `dt_t4`, 22028 on `dr_t3` and 18034 on `sh_t2` (one respawn; before the stall fix 11531 with two).
