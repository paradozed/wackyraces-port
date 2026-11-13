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
    no shortcut in its path. Not explained: a pickup (the driver uses no items, but picks them up),
    the opponents, or a rule for human cars on the last lap. Open.

