# Kosza: the creatures' motion, clip and car hit (2026-11-14)

Follows `analysis/race-stream-20261114.md` ("Kosza, the course creatures"), which ported their states,
waypoints, rates and race-stream draws. This note covers where each move puts the node, the GDHModel's
clip, the waiting state's visibility and the car hit.

## Census (all `*.WGM`)

| course | creatures |
| --- | --- |
| sh_t1, sh_t2, dr_t2 | five type 1 mode 7 birds (GDHModel) |
| dt_t3 | five type 0 mode 7 (GDHModel), one type 2 mode 4 (GDModel) |
| cc_t1, cc_t2 | one type 3 mode 0 (GDModel), one type 0 mode 1 on a GDPath (GDHModel, wait 100, `m_bCollision` 1) |
| dr_t1 | one type 3 mode 0 |
| cc_b | one type 0 mode 1 |

Modes 2, 3, 5 and 6 are used by no course. Every record serializes `m_bRotate` (`0x27c`) = 1; only the
mode 1 creatures have `m_bCollision` (`0x27e`) = 1. Every creature node's parent frame is the identity,
so the node's local frame (`+0x2c`, place `+0x54`) is its world frame.

## The moves (each before t grows by 2/3 x rate)

- `0x458230` (modes 4..7). f = 4 frac t (`0x510434`), seg = int f, u = f - seg. The segment runs from
  p1 = points[seg] to p2 = points[seg+1] (indices mod 4), with p0 = points[seg+3] and p3 = points[seg+2]
  (the one redrawn when the segment changes). ta = unit(p2 - p0), tb = unit(p3 - p1) (`0x452e20`).
  - Modes 6/7 (`0x458565`): e = p2 - p1, l = |e| (`0x457270`, `fsqrt`); T1 = ta l, T2 = tb l;
    a = T1 + T2 - 2e, b = (e - T1) - a; place = ((a u + b) u + T1) u + p1; heading
    d = (3a u + 2b) u + T1 (`0x511894` = 3).
  - Modes 4/5 (`0x45846e`): place = p1 (1 - u) + p2 u (`0x457310`, `0x4572a0`); heading
    unit(ta (1 - u) + tb u).
- `0x458be0` (modes 0, 3), round the place at init (`+0x74`'s `+0x28` = `+0x9c`, `0x43bf60`):
  - mode 0: a = t x 6.28 (`0x512640`, a double); offset r (cos a, 0, sin a); heading (-sin a, 0, cos a);
  - mode 3: a = t x 6.28, b = t x 3.14 (`0x512630`); offset r (sin a, 0, sin b); heading (cos a, 0, cos b).
- `0x457f80` (modes 1, 2): mode 1 the GDPath's point and tangent at t (`0x444be0`, the port's
  `path_point`); mode 2 `0x456af0` (a Hermite over the path's points, not ported, unused). Then t grows
  and past `+0xc4`/below `+0xc0` it stops there (returns 1) or wraps with `m_bLoop` (`+0xd0` = 1).
- With `m_bRotate` (`+0x48`, WGoPath's constructor default 1 at `0x4d18b6`): rows
  x = cross(d, (0, -1, 0)), y = cross(d, x) (`0x452ed0`; x is left unnormalized), z = unit(d)
  (`0x458bb0`, `0x458b50`, `0x458b80`), and the frame = `+0x74` (the node's frame at init) x those rows
  (`0x43c7b0`). The place is written either way (`0x43bf90`, then `0x41eeb0(0)`).

Ported: `creature_move` and `creature_place` in `race_objects.cpp`; `Creature::placed` is drawn by
`RaceView` from the node's subtree (taken out of the static world, as the WGoPath movers are).

## Visibility and the clip

- Waiting (`0x458e90`) sets node flag `0x100` (not drawn); moving (`0x458f10`) and squashed (`0x458fa0`)
  clear it. cc_t1's path creature (wait 100) is therefore invisible until its first start and after each
  run. Ported: the view skips state 0.
- The start (`0x457da5`) plays clip 0 (`0x405e1b(0, 0, 1, -1)`) with speed `+0x24` = speed x
  `0x52a990[type]` (0.1, 0.03, 0.1, 0.1) and time `+0x2c` = int(frand x 300) (`0x512610`). Each moving
  update sets the speed x 2/3 (`0x459233`, `0x440a70`): 2 for the birds. The controller's step is
  dt x `+0x24` (`0x43fed9`). Waiting and squashed freeze the clip (command 3, `0x4569a0`, when flag 4 is
  clear); moving unfreezes it. Ported: `MotionController::set_speed`, a posed instance per creature
  (`RaceView::creature_clips_`, the view's provisional 10 per update).

## The car hit (`m_bCollision`)

- Moving (`0x459311`, after the move): the object query `0x4a60c0` (mask `0x100`, the cars) with the
  creature's PRIM (cc_t1: three spheres r 24 at z 40, 0, -30). The query puts each sphere's centre through
  the node's world matrix (`0x4200e0`, `0x43b880`: x the node's scale 2.5), but its radius is the PRIM's own
  (`0x4a6198` reads `[ebp+0x54]`, kept at `[esp+0x84]` from `0x4a6252`), not scaled: r 24 against the car's
  100 (corrected 2026-11-15, see the hit's verification below). The pair test
  `0x482190` writes n = unit(creature - car) at `+0x38`; `0x4a64df` turns it by the car's frame
  (`0x43b980`, n x R) into `+0x20`. The car's row 2 (`0x43bf30`) x its speed `+0x3ef`, normalized, dotted
  with that: above 0 squashes, y axis x 0.1 (`0x43b190(1, 0.1, 1)`). Either way the creature goes to state
  2 (`0x401546` -> `0x458fa0`). With orthonormal rows the dot is sign(speed) x n's world z; a zero speed
  normalizes to NaN and fails it.
- State 2 (`0x45905b`): with a car touching, nothing more happens unless its speed is nonzero, it comes at
  the creature and |y|^2 > 0.2 |x|^2 (`0x51264c`, i.e. not flat yet): squashed again. No car: frand() <
  0.01 starts it (the next move rebuilds the frame from `+0x74`, unsquashed).
- The normal comes from the first of the creature's spheres that touches (the query stops there), whose
  centre lies up to 100 units along the creature's own axis (cc_t1: local z 40 x 2.5), so "coming at it"
  depends on which sphere the car meets.
- The query's car order (Wine, cc_t1, 8 s after GO, 2026-11-16): `0x4a60c0` walks the manager's list
  (`[0x6f2d00]`: `+0x44` + 2 entries, 12 bytes apart from `+0x48`, the loop at `0x4a6195..0x4a61b5`). It held
  vehicle 3 (the player), an empty entry, then 2, 0, 4, 1, 5, 6: the grid order (natively 3 2 0 4 1 5 6),
  not the rank (the player ran 7th) nor the robots' update order. The port's car list (`car_probes`: the
  player, then the robots in grid order) is the same order. Read once; the second entry is presumably
  player 2's (not checked in two players).
- Ported as read: `creature_touch` (the first car not hidden, in that order),
  `creature_struck`, `creature_squash`. Not compared with Wine: no car touched the creature in the Wine run.

## Verification

Wine (scratch prefix, Xvfb `:77`), each Kosza found by its vtable `0x5125c0` and logged on every change of
t, with its node's `+0x54` and `+0x44` (row 2):

- sh_t1, the five birds: native and Wine agree within 0.4 units on 904 logged updates up to t = 0.51
  (about 115 updates, GO included). First line, both builds: bird 24 at (24691.4, 4747.2, -27246.9), row 2
  (-1.5638, 0.8351, 3.5857). From t = 0.51 they split by a few units: the segment-2 waypoint redraw at
  t = 0.5 takes later race-stream draws, which by then differ from Wine's (the stream, not the curve).
- cc_t1, mode 0: 673 of 675 Wine lines within 1 unit of base + r (cos, 0, sin)(t x 6.28) with Wine's own
  t; the other two (8 units) are reads that straddled an update. Wine's t runs half a step (0.0008) off
  the native one: one load-time update with the frame tick `0x5299b4` at 1 (timing; provisional).
- cc_t1, mode 1: the path creature's 236 updates of its first run are equal to the printed digit (place
  and heading) when matched by t. It started 7.0 s into Wine's log, at native update 186.
- Native capture (sh_t1 start, Xvfb `:78`): the birds are drawn in the sky, wings at different poses.
- Native cc_t1, robots only: at update 738 a robot touches the path creature; it is squashed (y axis 2.5
  to 0.25), stopped 48 updates, then moves on unsquashed. `native_drive_test` checks the sh_t1 bird and
  the cc_t1 path creature at Wine's values, the mode 0 circle, and a synthetic hit both ways.

Control: with the frame build disabled (a scratch copy of `race_objects.cpp`, the build untouched) the
birds' row 2 misses Wine's by up to 7.98 over the 107 updates before t = 0.5; with it, by 0.0001.
`native_drive_test`: all 20 courses exit 0 and ctest passes 14 of 14.

dt_t3 (Wine, 2026-11-14; menu Return x 4, Down x 2, Return, Down x 2, Return): six objects, the mode 4
GDModel first, then the five birds, in the native order. Wine's t runs half a step off again (0.00944 =
8.5 steps), so the harness shifts each native t by half a step before the first update. Matched by t on
1493 updates: the mode 4 creature's place is exact (0.00) up to its t 0.5 and its heading up to t 0.27;
the birds' place and heading are exact (0.00) up to their t 0.25. Each split starts at a waypoint redraw
(the heading uses the redrawn p3 first, the place only from the next segment), whose race-stream draw
already differs from Wine's there.

### The hit under Wine (2026-11-15)

cc_t1, Wine (scratch prefix, Xvfb `:77`, `wmem.py khit`: each Kosza's state, t, node rows and place, the
player car's place, row 2 and speed `+0x3ef`). A single 40000-unit hop of the car froze the original at
00:04, so the car went along a robot's line recorded natively (92 points, a quarter second apart) and was
parked at (-35380, 562.9, 15000) with its own heading, row 2 (-0.147, 0, 0.989), across the path creature's
line. A scratch harness parks the native car there with the same heading (`RaceSession::place`) and logs
the same values.

| | Wine | native, radius x 2.5 (before) | native, radius unscaled |
| --- | --- | --- | --- |
| stops at (state 2) | t 0.43153, z 15190.0 | t 0.42730, z 15215.0 | t 0.43153, z 15190.0 |
| stopped, car at rest | unsquashed (row 1 y 2.5) | unsquashed | unsquashed |
| Up held: first car update | (-35380.4, 15002.6), speed 0.01, unsquashed | the same | the same |
| next update | squashed (0.25), car (-35381.1, 15007.6) at 0.03 | the same | the same |

So the speed-zero touch stops it without a squash (the NaN test fails), and the first touch with the car
moving towards it squashes it, in the update after the car first moves (objects before cars). In both builds
the car then drives through the creature (no push). In Wine the creature had not started again 7.4 s after
the car left (state 2's frand() < 0.01 an update; the run ended there). The stop one step early was the
scaled radius: at place 15215 the -30 sphere (z 15140) lies 140 from the car, inside 100 + 60 but not 100 + 24.

The same query serves the port's dr_t3 crane arm, sh_t4's shifted roads and the WRolling rocks; their nodes
have scale 1 on every course, so the fix changes only the Kosza (cc_t1, cc_t2: scale 2.5).

`native_drive_test` checks it on cc_t1 (the player placed there, yaw 0.1475; Up held 60 updates after the
stop): `path Kosza stopped by the parked player at update 329, t 0.43153 z 15190.0, y axis 2.500 while the
car rests, 0.250 once it moves`. Control, the radius scaled again: update 328, t 0.42730, z 15215.0, the check
fails (exit 1). All 20 courses exit 0 and ctest passes 14 of 14 with the fix.

### A moving car and the restart, under Wine (2026-11-15)

The same parking, then Up held 1.6 s from the creature's t 0.36 (a trigger thread in `wmem.py` polling `+0x6c`;
the car's first move came with the creature at t 0.37230). Wine: the first touch, the creature still moving,
squashes it (`0x459311`) at t 0.41461, z 15290, the car at (-35403.7, 15159.8) at speed 0.14. Native, Up
pressed so that the car first moves at the same t: the same update, t, place, car place and speed. Driving
forward the touch is moving for any press between t 0.25 and 0.41; reversing (brake, release, brake) gives a
moving touch only for a press near t 0.41 (the car outruns the creature otherwise), too narrow for the key's
latency, so the reverse case was not run under Wine.

The restart: counters patched into the original (caves at `0x406742`: entries into state 2 at `0x45905b`,
the no-car branch `0x4591fb` with the frand it draws, the restart call `0x459211`). One run: 310 updates in
state 2, the no-car branch 301 times, one restart, at log 48.5 s. Another: 24 draws, all >= 0.013 but the last,
0.00861, which restarts it; the values spread evenly (mean 0.46). So the restart is frand() < 0.01 an update as
ported; two earlier runs that saw no restart for 7.4 s and 10 s were chance (a simulation of the stream at
strides 1..40 gives about 100 updates on average and 10-14 % past 210). After the restart Wine goes on from
t 0.41884 (z 15265), unsquashed: the native restart does the same (update 354 natively; its timing follows the
race stream, which the player's moves under Wine take out of step).

`native_drive_test` checks this case too: `path Kosza struck by the moving player at update 325, t 0.41461 z
15290.0, y axis 0.250, car z 15159.8 speed 0.14; restarted at update 354, t 0.41884, y axis 2.500`. Control:
with the struck test disabled the cc_t1 run fails (exit 1).

### The clip's pace under Wine (2026-11-16)

`0x440a70` (Kosza's speed write through `0x406429`, `this` = node + `0x15e`) looks the controller up by
`0x40526d(0, -1)` and writes `+0x24` of `[[this + 8] + 4 index]`, i.e. of `[node + 0x166]`'s element. On
sh_t1's first bird that is element 2 (`wmem.py kclip`): speed 2.0 (0.1 x 30 x 2/3, the port's `clip_speed`),
time `+0x2c` 338, 358, 378, 398, ... 6298 after 298 updates, 20 an update (the t steps by 0.00444 alongside),
flags `+0x28` = 1; the tick `0x5299b4` stayed 2. So each update steps the controller by dt 10 x speed, as the
port's view does (`MotionController::update(10)` per race update, `RaceView::pose`).

Not compared with Wine: a reversing car's touch; the clip's phase (the start frame int(frand x 300) follows
the race stream).

Follow-up (2026-11-17, `analysis/kosza-open-20261117.md`): the reversing car compared with Wine (it squashes;
the struck test corrected), mode 2's walk `0x456af0` ported and compared on a forced path, the two-player car order read.
