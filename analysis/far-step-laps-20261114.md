# Far-step lap times against Wine (2026-11-14)

## Method

- `sh_t1`, single race, vehicle 3, the player idle on the grid, so every robot runs in the far state (`0x1000`)
  for most of the race. Native: `RaceSession(track, 3, 140)` (seed 140 = Wine's grid).
- Wine, scratch prefix on Xvfb `:77`: the memory reader logs every car's `+0x19f` (laps) from the car table
  `0x6f2ce0`, with an exact update count: robot 2's WCtrlRoboCar `+0xb8`, which `0x4e4f70` decrements every
  update with no floor (20 at GO, then negative).
- The original's race clock is wall time: `0x450f80` reads `timeGetTime` × 3 / 10 (1/300 s) into `0x6ab2d0`;
  the frame delta `0x6ab2dc` is read only by the pause code (`0x451060`). Under Wine on Xvfb the game ran at
  24.6 updates/s in the countdown and about 26.7 in the race, so lap times in seconds are not comparable.
  The earlier "native laps about 5% quicker" (`robocar-ai-20261028.md`, 2026-10-30) was this artifact.

## Before (updates after GO at each lap's end)

| Vehicle | Wine | Native | Native / Wine |
|---|---|---|---|
| 6 | 1227 / 2706 / 4184 | 1340 / 2911 / 4480 | 1.07..1.09 |
| 2 | 1571 / 3131 / 4693 | 1670 / 3341 / 5000 | 1.065 |
| 0 | 1574 / 3137 / 4700 | 1674 / 3333 / 4994 | 1.063 |
| 4 | 1693 / 3374 / 5055 | 1807 / 3595 / 5386 | 1.066 |
| 1 | 1718 / 3529 / 5218 | 1826 / 3749 / 5555 | 1.064 |
| 5 | 1925 / 3814 / 5690 | 2058 / 4056 / 6060 | 1.065 |

Native far steps (updates 300..2100 after GO): v0 74.6, v6 88.7. Wine (40..70 s): v0 82.9, v6 93.3.

## The far step `0x4e54ef..0x4e552a`

- step = row top speed (`0x6f2990 + vehicle × 0x60`) × car `+0x163` × WCtrlRoboCar `+0x4c` (integer, `fild`)
  × 0.56 (`0x516b5c`) when the robot's rank `+0x1af` is below the player's (`[0x6f2d00+0x38]+0x1af`), else 0.48
  (`0x516b58`); then × `0x6f2cd8` (catch-up) or `0x6f2cdc` (hold) by the rank decision.
- No frame factor. The port had used top × frame factor (0.9333) × 204.
- `+0x4c`: 1000 in the constructor (`0x4e449f`); at the init (`0x4e4804..0x4e4869`) the probes go to mode 2
  (`0x40134d`) and `+0x4c` = (int)|`+0x40b` − `+0x423`|, the distance between the two probe points.
  Read under Wine: 203 (0xcb) on all six robots; `+0x163` = 1.0 on all.
- The catch-up boost scales the row's top speed in place (`0x4e504d..0x4e505f`), so the boosted top is the one
  the step uses, as natively.

## After (ported: top × 203 × 0.56/0.48)

| Vehicle | Wine | Native |
|---|---|---|
| 6 | 1227 / 2706 / 4184 | 1261 / 2740 / 4219 |
| 2 | 1571 / 3131 / 4693 | 1566 / 3126 / 4692 |
| 0 | 1574 / 3137 / 4700 | 1569 / 3131 / 4695 |
| 4 | 1693 / 3374 / 5055 | 1693 / 3375 / 5057 |
| 1 | 1718 / 3529 / 5218 | 1716 / 3519 / 5214 |
| 5 | 1925 / 3814 / 5690 | 1927 / 3809 / 5693 |

Every robot within 10 updates on every lap but vehicle 6, which is a constant 34 updates behind from lap 1 on
(lost near the start, where it still drives physically). Native far step v0 79.5 (Wine 82.9 over a window that
includes near updates, flag 2 on 57 of 800).

## Not verified

- Other courses' lap times (cc_t1 done 2026-11-15, below); `+0x4c` on other vehicles' probe spans (all six read 203 here).
- (Vehicle 6: see below, the random `+0xb4` draw.)
- The "ahead" test: the original compares ranks, the port compares progress with the player's.

## Vehicle 6's 34 updates: the `+0xb4` draw

- Vehicle 6 leads, so the rank decision boosts it at once (catch-up, `0x4e4fe9`: the row's top × `+0xb4`) and
  the boost stays until lap 2 (`0x4e4f6b`). Its far speed on the switch (`+0x3ef` = top × 0.8, `0x4e52e8`) was
  0.7384 under Wine (top 0.923 = 0.74 × 1.2473) and 0.7180 natively (0.74 × 1.2128).
- `+0xb4` = 1 + c + rand() × 2^-15 × c / 4 (`0x49f51d`, c = 0.2 here). Read under Wine (2026-11-14): v0 1.21787,
  v1 1.22739, v2 1.20185, v4 1.23389, v5 1.24589, v6 1.24724, i.e. rand() 11711, 17953, 1215, 22210, 30077,
  30955, not from the default-seed sequence: the original's draws differ from run to run.
- Control: native with Wine's six `+0xb4` values (harness override) gives vehicle 6 1227 / 2706 / 4184, exactly
  Wine's; v2 1566 / 3126 / 4695, v0 1569 / 3130 / 4690, v4 1693 / 3376 / 5058, v1 1725 / 3523 / 5216, v5 1913 /
  3796 / 5680 (Wine 1925 / 3814 / 5690). So the 34 updates are the random draw, not a rule difference.

## The race stream is reseeded at every race load (ported)

- The game's `rand` is its own copy of the MSVC LCG (`0x430e40`, state `0x5472c8`; the CRT's `rand` `0x4eecd0` has
  4 callers and `srand` `0x4eecc0` none). `0x430e20` sets both `0x5472c8` and the effects stream `0x5472c4`; its
  one caller (`0x431fc3`, in `0x431d00`, reached through a vtable) passes 0x1312d6e (20000110).
- Wine (2026-11-14, `sh_t1`): logging `0x5472c8` from launch, the state is 0x1312d6e at 46.2 s (the menu) and again
  at 74.71 s and 74.74 s (the race load, the second time with the cars created). From there the six `+0xb4` values
  are draws 7..12 (1215, 11711, 22210, 17953, 30077, 30956), in grid order (v2, v0, v4, v1, v5, v6); draws 1..6
  are the grid shuffle: `race_roster(0, 3, 20000110)` gives Wine's grid 3 2 0 4 1 5 6, the one seed 140 gave.
  The `cc_t1` run gave the same six values bit for bit.
- So a race's grid and draws do not vary between runs. Ported: `RaceSession`'s stream carries on from the grid
  shuffle's state (it had restarted at 0x1312d6e, so the `+0xb4` values were draws 1..6), and the default seed
  is 0x1312d6e in `RaceSession` and in the game (it was the clock); `WPC_GRID_SEED` still overrides.
- With no override the native `+0xb4` values equal Wine's (1.2018540 ... 1.2472352).
  - `sh_t1`: v6 1227 / 2704 / 4179, v0 1568 / 3128 / 4687, v2 1572 / 3134 / 4700, v4 1698 / 3381 / 5063,
    v1 1715 / 3527 / 5224, v5 1917 / 3799 / 5688 (Wine above).
  - `cc_t1` (Wine, same method): v6 1372 / 3032 / 4716, v0 1760 / 3511 / 5260, v2 1765 / 3523 / 5281,
    v4 1890 / 3771 / 5661, v1 1927 / 3928 / 5828, v5 2138 / 4246 / 6387. Native: v6 1364 / 3015 / 4668, v0 1763 /
    3513 / 5263, v2 1750 / 3497 / 5245, v4 1899 / 3782 / 5660, v1 1920 / 3935 / 5829, v5 2131 / 4233 / 6333.
    All within 1% (at most 54 updates).
- Not verified: championship races and other modes (same routine assumed); whether the menu path can change the
  state before the race load (the load reseeds, so it should not).

## Near robots: time factor 1.0 and the stretched step (2026-11-14)

- Wine's `sh_t1` start (every robot's x, z and speed per update; update after GO = 19 − `+0xb8`, the counter
  being 19 already on the GO update) against native with the original's seed: the speeds matched to four digits
  every update, the positions drifted from update 2 (v2: 56 units by update 10).
- Wine's step per unit of speed was 204.0 at low speed; native's 190.4 (204 × the frame factor 0.9333).
- `0x469550`: `+0x163` = 1.0 when `[car+0x18]+0xc0` is set, else `[0x5299b4]` (2 frame units) × 0.4667 (0.9333),
  capped at 2. Read under Wine: a robot's `+0x163` = 1.0 with `+0xc0` = 1, the player's 0.9333 (0x3f6eeeef) with
  `+0xc0` = 0. Ported: `move()` uses 1.0 for robots, the frame factor for player cars.
- Above speed 0.4 Wine's step per unit of speed is 204 × (1 + 0.09 × (speed − 0.4)) (204.8 at 0.443, 206.3 at 0.528,
  208.0 at 0.618), twice the port's front-half stretch. `forward_step` now stretches the whole 204 (fitted; the
  probe routine `0x468280` was not fully decoded; the wall probes keep `probe_span`).
- Result: every robot within 0..3 units of Wine for the first 19 updates (before: 56). The first split is the
  first steer: native turns at update 20, Wine at 21, with a different size (Wine's z step −25.6 then −43.9,
  native's −16.1 then −32.0); the order of the decision and the car update around it is not traced.
- Tried and reverted: applying each robot's steer one update late (the car update before the decision). Vehicle
  0's first turn then lands on update 21 as under Wine (z steps −20.4, −38.2 against Wine's −25.6, −43.9: still about
  20% short), but vehicle 1, within 4..8 units of Wine through update 60 without the lag, ends 53 off at update 40.
  So it is not a general one-update lag; the object update order (both updates are reached through vtables,
  WackyAuto `0x46a4a0` and WCtrlRoboCar slot 2 `0x4e4f10`) is not traced. The first steer's size may also follow
  the decision's random factor (80 + rand % 30, `0x4e7a9e..`), whose draw order was not compared.

## `ct_t1` laps (2026-11-14, current build: factor 1.0, stretched step, original seed)

Wine (update after GO = 19 − `+0xb8`) against native, the end of laps 1 / 2 / 3; the grid matches (vehicle 0 at
(−36584.5, 9453.8) in both):

| Vehicle | Wine | Native | Native − Wine |
|---|---|---|---|
| 6 | 1419 / 3141 / 4843 | 1420 / 3146 / 4860 | 1 / 5 / 17 |
| 0 | 1819 / 3632 / 5457 | 1833 / 3657 / 5489 | 14 / 25 / 32 |
| 2 | 1836 / 3643 / 5459 | 1816 / 3637 / 5441 | −20 / −6 / −18 |
| 4 | 1994 / 3945 / 5906 | 1967 / 3926 / 5875 | −27 / −19 / −31 |
| 1 | 2017 / 4094 / 6053 | 2004 / 4069 / 6057 | −13 / −25 / 4 |
| 5 | 2232 / 4419 / 6582 | 2230 / 4425 / 6598 | −2 / 6 / 16 |

All within 0.6%. Courses compared so far: `sh_t1`, `cc_t1`, `ct_t1` (and `dt_t2` up to the fountain).

## `dr_t1` laps (2026-11-14, same build; grid matches, vehicle 0 at (−123464.1, 14402.1))

| Vehicle | Wine | Native | Native − Wine |
|---|---|---|---|
| 6 | 1392 / 3010 / 4629 | 1313 / 2931 / 4552 | −79 / −79 / −77 |
| 5 | 1650 / 3623 / 5601 | 1603 / 3606 / 5582 | −47 / −17 / −19 |
| 2 | 1710 / 3414 / 5116 | 1716 / 3414 / 5114 | 6 / 0 / −2 |
| 0 | 1737 / 3433 / 5128 | 1727 / 3423 / 5122 | −10 / −10 / −6 |
| 4 | 1841 / 3635 / 5437 | 1823 / 3624 / 5423 | −18 / −11 / −14 |
| 1 | 1878 / 3676 / 5521 | 1842 / 3641 / 5439 | −36 / −35 / −82 |

Within 1.7%. Vehicle 6 gains its 79 updates on lap 1 (the start, where it drives near the player) and keeps the
same far pace after; not traced.

## Second card of each environment (2026-11-14, Wine batch, same build)

End of laps 1 / 2 / 3, updates after GO; the grids match natively (vehicle 0's slot to 0.1 unit).

- `ct_t2`: every robot within 1.1% (largest −80 of 7925, v5 lap 3).
- `dr_t2`: within 2.0% (v6 −37 / −78 / −125).
- `cc_t2`: v0, v2, v6 within 0.9%; v1 and v4 within 1.5%; **v5 1896 / 3397 / 5213 against native 1904 / 3756 /
  5608**: natively it loses 359 updates on lap 2 (not traced).
- `sh_t2`: **Wine's v6 1032 / 5566 / 6823**: about 3300 updates held somewhere on lap 2; native 1031 / 2267 / 3511.
  v2 native 141 late on lap 1 (1151 / 1292); v0, v1, v4, v5 within 7% (not traced).
- `dt_t2`: Wine's v6 never laps (the fountain); v4 stops after lap 2 (as in the earlier run); **v0 996 / 3286 /
  4270**: held about 1270 updates on lap 2, then free, so the fountain's hold can end (native v0 1021 / 2014 /
  3005: not caught, its frozen yaw is −1.58). v2 native 143 late on lap 1; v1, v5 within 3%.

Where Wine's `sh_t2` v6 and `dt_t2` v0 were held, and how the latter got out, is not traced.

## `sh_t2`: Wine's vehicle 6 frozen on a log ride (2026-11-14)

- Logged per update from 60 s: from update 1521 at least to 2675 (the log's end) vehicle 6 stays at exactly
  (40570.8, 1167.5, 89821.4), far (`+0x2b1` 0x1000), speed `+0x3ef` 0, `+0x1c7` 0, no WackyAutoBomba running
  (`[+0x36d]+0x2c` 0), `+0x26b` 1.7380, and `+0x89f` = 0x44aa38c, a log.
- The ride is WLogRide's update (`0x4cc450`, vtable `0x515970`): for a car whose `+0x89f` child has class
  0xedda0012 it adds the log's motion to the position copy `+0x288`, rebuilds the frame from it and the angles
  (`0x4039e0` → `0x468620`) and clears `+0x89f` (`0x4cc6d0`). The far step writes `+0x288` itself (`0x4e5858`), so
  the copy is not stale; the rebuild does reset the heading to the frozen `+0x26b`, as at the fountain.
- Not explained: why the position does not change at all (the far step moves about 100 an update, and no skip
  condition was seen set). The native far robot is not held there (v6 laps in 1031 / 2267 / 3511).
- **Lead (2026-11-14):** with a route contact (`+0x2bd` set) the far step places the car at the stored contact point
  `+0x2d5` plus the step along its frame (`0x4e5752..0x4e57a8`), not at its position plus the step; without one, at
  its position plus the step and 100 lower (`0x4e56da..0x4e5750`, `0x51152c`). `+0x2d5`/`+0x2bd` come from the shadow
  query `0x47c180` (`0x47c2b2..0x47c32c`; a miss clears them, `0x47c331`). So a far robot whose contact is not
  refreshed while its heading is reset every update (WLogRide's rebuild from `+0x26b`) lands on the same point each
  update: Wine's freeze. When `0x47c180` runs for a far robot (and why not here) is not traced. The port rests the
  far step on the route layer under the new position instead; no port change yet.
- The route contact is refreshed by the car effects' update: `0x47a460` (WCarFx: tracks, POUSSIERE puffs, then
  `0x40625d` and the shadow query `0x4019c4` → `0x47c180`). Natively `sh_t2` has 4 sections; vehicle 6 passes
  z 89821 at x ≈ 41975 in section 1 (inside the player's window 3/0/1, not hidden). Wine's is frozen about 1400
  units further west, at x 40570, on a log: it left the native line before the freeze. Where it left it and whether its
  WCarFx stops updating there are not traced.
- **Logged from 46 s (2026-11-14):** vehicle 6 follows the native line (x ≈ 42005, z 82000 → 89565, far, speed
  0.592; a WSaw's `+0x1c7` = 8 for an update at 76463) until update 1469. At 1479 a storm cloud's strike is running
  (WackyAutoBomba `[+0x36d]+0x2c` = 0x400), speed 0, and the car stands 1420 west at (40583, 1801, 89738), falling to
  y 1162 by 1499. The strike ends at about 1509, at (40570.6, 1167.5, 89821.4); from there it does not move for
  the 1150+ updates logged. `+0x89f` held the same value (0x44aa38c) the whole time, so it is not a fresh log
  contact. So the hold is: a far robot struck by the storm, set down 1420 off the route in the pocket below, and
  never moved again by the far step. The port's far hit rule (provisional) lets it glide on after the hit. Which
  robot the storm strikes depends on the robots' item draws; how the strike moves a far car and why it then stays
  are not traced.
- **Ported (2026-11-14):** the 1420 west is the car update driving the struck far robot along its frozen yaw.
  While a WackyAutoBomba runs, a far robot skips the far step (`0x4e53df`) and goes through the car update, which
  rebuilds the frame from the angles (`0x468620`), so its heading snaps to `+0x26b` (1.738 here, about due west);
  at about 120 an update that is the 1420 in ten updates, off the route into the pocket. Natively a far robot's
  first car update after a far step now takes `angle_yaw` (`Opponent::far_framed`); back to near the angles come
  from the frame (`0x4e5249`), so no snap there. All 20 courses pass `native_drive_test`.
- Still not explained: why it then stays in the pocket once the strike ends (the far step resumes there).
- **Why it stays (2026-11-14, Wine):** at the landing (update ~1493) the drowning counter `+0x8e7` is set (0x26
  logged; 40 at `0x4700ac` on surface 0x800000). While it is non-zero the shadow query skips (`0x47c1ad`), so the
  route contact stays at (40583.5, 89737.9), and the far step places the car at that contact plus its step: it
  settles at (40570.8, 89821.4) by update 1514. The counter counts down to 1 and stays 1 (logged to 1567 and,
  earlier, the car unmoved to 2675): the return below 1 (`0x4700ec..0x47011f`, in the car's contact) is not reached
  while the robot is far. So a far robot that drowns waits until it is near again.
- **Ported:** a drowning robot's counter is held at 1 while it is far (the `0x4e4f10` distance test, re-evaluated
  while it drowns), and returns once near. All 20 courses pass; `dt_t2` now has one robot trapped at the fountain
  (it was three before the factor-1.0 change; Wine one at first, a second later).
- Not ported: the frozen route contact itself (the native far step is skipped while drowning anyway); native
  drowning lasted 45 updates (`drown_delay`) against the original's 40 (fixed below).
- **Drowning delay:** natively 40 updates now (was 45): `0x4700ac` sets `+0x8e7` = 0x28, and the Wine log shows 0x26
  two updates after the landing. The 45 came from "about 1.5 s" measured in wall time, which at Wine's ~26.7
  updates/s is 40 updates. All 20 courses pass.

## Cards 3 and 4 of each environment (2026-11-14, Wine batch; native on the current build)

Every grid matched natively (vehicle 0's slot to 0.1 unit). Worst lap-end difference per course, and what stands out:

| Course | Worst | Notes |
|---|---|---|
| `sh_t3` | 2.5% | v4, v5, v6 within 0.4% |
| `ct_t3` | 2.8% | |
| `dr_t4` | 5.7% | v1 −124 / −196 / −263; the others within 2.7% |
| `cc_t3` | 6.3% | **every robot faster natively, growing each lap** (v0 −59 / −110 / −231, v2 −65 / −128 / −239): a systematic gap, not an event |
| `sh_t4` | 12.1% | v0, v1, v4 within 0.4%; Wine's v5 and v6 lose 475..969 and 345..705 from lap 2 (events in Wine) |
| `dt_t4` | 11.2% | mixed signs per lap (events on both sides) |
| `ct_t4` | 14.2% | v1, v2 within 0.6%; native v5 223 / 467 / 707 late, v0 252 / 507 late from lap 2 |
| `dt_t3` | 14.1% | v0, v2 native ~130 early from lap 1, constant after (the start) |
| `cc_t4` | 16.3% | mixed: v0, v2, v5 native late from lap 1, v1 early |
| `dr_t3` | 20.6% | v6 native 358 late on lap 3 alone |

So the far step itself matches on most courses (gaps that stay constant after lap 1 come from the start;
single-lap jumps are events such as hits, traps or drowning). `cc_t3` is the exception: all six robots gain on
Wine on every lap, about 2% a lap. Not traced.

## `cc_t3`: the geyser's aim (fixed 2026-11-14)

- Wine (per update, robots far): every robot is held once a lap, standing still about 60 updates, with speed 0, no
  hit and no effect: v0 at (−942, 26054) from update 530 (snapped back about 150 from (−796, 25951)), v2 at 527, v6
  at 410, v5 at 647 and 761. That is cc_t3's WGeyser putting the car on its axis (`0x4c9326`) while it erupts.
- Natively nobody was held: the geyser aimed at points such as (−80733400, 1151974528). The aim is the chosen car's
  position + its z axis × m_Distance (`0x4c8d23`), and m_Distance (`0x241`) is a **float** (1700 on `cc_t3`, 1000 on
  `dr_t4`); the port read it as an integer, i.e. its bits (1.15e9). The earlier note's 11547 was that misreading too.
- Fixed (`n.real(0x241)`). The native geyser now erupts at (−1007, 26251) over updates 520..580, about 200 from
  Wine's hold point; native robots still pass it (their line there differs by a few hundred units from the start
  pack on). Laps: v1 1491 / 2890 / 4345 (Wine 1432 / 2885 / 4355), v6 1145 / 2467 / 3860 (1139 / 2458 / 3719);
  v0, v2, v4, v5 still 55..151 early.

## The start hold, 21 updates (fitted, 2026-11-14)

- On `cc_t3` the first split is the first steer, and it is a clean one-update shift: Wine's v2 turns at update 21 with
  step (78.1, 124.1), native at 20 with (73.2, 127.1); v0 (46.4, 132.8) against (42.8, 131.4).
- With `start_hold` 21: `cc_t3` v2 within 6..18 units to update 40 (was 76..112), v4 first apart at 32 (was 21); `sh_t1`
  v2 within 2 to update 35, v1 unchanged. Laps: `cc_t3` v0 1332 / 2751 / 4247 (Wine 1396 / 2769 / 4210), v2 1328 / 2744 /
  4240 (1391 / 2774 / 4205); `sh_t1` lap 3 all 7..13 updates before Wine's. All 20 courses pass.
- Fitted: `+0xb8` is 20 at the init and Wine's counter reads 19 on the GO update; which part of the update order
  gives the extra update is not traced. v0's first turn still differs in size on both courses.
- The next split on `sh_t1` (v2, update 40) is the far test: native tests the position before the update's move
  (dx 4937 at update 39, far from 41); Wine is far and stepped at 40. Either it tests after the move or its player
  stands slightly further west; the player's position was not logged.

## The far switch in the same update (2026-11-14)

- Wine `sh_t1`, rerun with the player logged too: the player stands at (-17494.4, 722, -1440.3) through the
  start, as native's does, so the split is not the player's position. At update 40 v2's route contact `+0x2d5`
  reads (-12408.3, -1058.9), which is native's update-40 position after the near move, and the car stands one far
  step further on, at (-12330.2, -1074.3), far (`+0x2b1` 0x1000), speed 0.56, in the same update.
- So the car update (the near move) runs before the controller's far test and far step (`0x4e4f10`, `0x4e54ef`):
  the update that takes a robot past 5000 also runs its far step from the moved position. The port re-tests the
  distance at the end of the near path and, when the robot is now far, runs the far branch once more in the same
  update (`drive_opponent(..., again)`; the start boost count and the start-hold decrement are not repeated).
- Result (`sh_t1`, the same harness against the new Wine log): v2 far at update 40 at (-12329.8, -1069.5), 5 units
  from Wine's, and within 10 units up to update 111 (was split from update 40); v0 also switches at 41 in both.
  v0's 40..150-unit gap from update 25 is its first-turn size, still open.
- The Wine harness was rebuilt after a reboot wiped `/tmp`; the scratch prefix now lives under the ignored `build/`.

## cc_t1 (2026-11-15)

(A re-run: cc_t1 was compared on 2026-11-14 above, native then within 1%. Since then the player's engine draw,
the contact's dropped draw, the robots' section-row power-up lists and the objects' place in the update moved
the race stream, so the native laps below differ from that run's by up to 142 updates (v5's third), while
staying within 1.4% of Wine. sh_t1 re-run the same day with Wine's grid: v6 1268 / 2739 / 4212, v0 1569 /
3128 / 4689, v2 1574 / 3135 / 4692, v4 1689 / 3370 / 5048, v1 1709 / 3515 / 5210, v5 1912 / 3800 / 5678, all
within 1% of Wine's.)

Same method: the player idle on the grid, vehicle 3, the original's seed; Wine logged each car's laps `+0x19f`
against robot 2's `+0xb8` (updates after GO = 20 - `+0xb8`; Wine's counter reaches 1 at the first start-line
crossing just after GO, so its laps 2..4 are the native laps 1..3). Native: `RaceSession(track, 3)`, idle,
`opponent_laps` (everything ported to date: the far step, the avoidance caches, the update order, the
objects before the cars, the streams).

| Vehicle | Wine | Native | Native / Wine (lap 3) |
|---|---|---|---|
| 0 | 1760 / 3511 / 5260 | 1751 / 3516 / 5283 | 1.004 |
| 1 | 1927 / 3928 / 5828 | 1921 / 3917 / 5807 | 0.996 |
| 2 | 1765 / 3523 / 5281 | 1747 / 3490 / 5296 | 1.003 |
| 4 | 1890 / 3771 / 5661 | 1886 / 3762 / 5638 | 0.996 |
| 5 | 2138 / 4246 / 6387 | 2168 / 4322 / 6475 | 1.014 |
| 6 | 1372 / 3032 / 4716 | 1362 / 3004 / 4648 | 0.986 |

Every robot within 1.4% over three laps, the finishing order the same (6, 0, 2, 4, 1, 5). Vehicle 5 loses
about 30 updates a lap and vehicle 6 gains about 20; not traced.

## dt_t1 and ct_t1 (2026-11-15)

Same method (Wine menu: environment Down x2 / x3, course 1).

dt_t1:

| Vehicle | Wine | Native | Native / Wine (lap 3) |
|---|---|---|---|
| 0 | 1198 / 2377 / 3579 | 1138 / 2292 / 3506 | 0.980 |
| 1 | 1247 / 2637 / 3885 | 1337 / 2600 / 3972 | 1.022 |
| 2 | 1244 / 2429 / 3556 | 1246 / 2429 / 3614 | 1.016 |
| 4 | 1222 / 2510 / 3796 | 1221 / 2486 / 3798 | 1.001 |
| 5 | 1537 / 3011 / 4354 | 1447 / 2985 / 4325 | 0.993 |
| 6 | 1022 / 2168 / 3341 | 1077 / 2205 / 3277 | 0.981 |

ct_t1:

| Vehicle | Wine | Native | Native / Wine (lap 3) |
|---|---|---|---|
| 0 | 1820 / 3633 / 5458 | 1816 / 3645 / 5474 | 1.003 |
| 1 | 2018 / 4095 / 6054 | 1986 / 4082 / 6053 | 1.000 |
| 2 | 1837 / 3644 / 5460 | 1833 / 3665 / 5482 | 1.004 |
| 4 | 1995 / 3946 / 5907 | 1967 / 3926 / 5885 | 0.996 |
| 5 | 2233 / 4420 / 6583 | 2240 / 4412 / 6596 | 1.002 |
| 6 | 1420 / 3142 / 4844 | 1418 / 3128 / 4861 | 1.004 |

- ct_t1: every robot within 0.4% over three laps, the order the same (6, 0, 2, 4, 1, 5).
- dt_t1: within 2.2%; single laps differ by up to 90 updates both ways (v1's first lap +90, v5's -90). Not
  the tram crossing: a Wine far log of the same race (4919 updates after GO, every robot's three laps) has
  `+0x1c7` = 0 on every car throughout, so no robot waited (`+0x1c7` = 5); natively none waits during the
  race either (v1 and v6 wait once each, after their third lap).
- Where v1 loses its first lap's 90 updates (2026-11-15, Wine's far log against a native position log, each
  Wine position matched to the update native passed nearest it): within a few units and 2..4 updates until
  update ~725 at (9676, 12340), where the two take different branches of a fork (Wine east, x 9700 -> 11500;
  native west, x 9200 -> 6800), rejoining near z 17300 with native about 62 updates behind. The rest is the
  hidden-robot handling: around updates 901..951 Wine's v1 stands hidden (flag `0x10000`) at (16183, 19949)
  and is shown 15000 further on.
- The fork follows the race stream (the robots' link draws), and on dt_t1 the stream has split by then. Wine's
  `rng` (`0x5472c8`) in the far log against the native state, one update apart (Wine's line is read after its
  update): equal through update ~23, a draw or two apart by update 40, then Wine takes about 18 more draws
  between updates 75 and 100. Which callers differ is not traced (the next step: the rand hook with callers
  on dt_t1 against native per-caller counts). The order differs only between v2 and v0,
  23 updates apart under Wine (Wine 6, 2, 0, 4, 1, 5; native 6, 0, 2, 4, 1, 5).
- With sh_t1 (2026-11-14) and cc_t1, Championship I's four courses are compared.

## `+0x4c` per robot (2026-11-15)

- dt_t1's far steps under Wine: the median of step / speed (x and z, far updates 150..700) is 142.5..142.7 for
  robots 0, 1, 2, 4 and 5, natively 142.1: 204 x 0.7 against 203 x 0.7. So `+0x4c` is 204 there, while all six
  read 203 on sh_t1.
- `0x4e4804..0x4e4869`: the init puts the probes in mode 2 (`0x468280`: each probe is the position plus the
  frame's rows x (-52, 0, +-102), the 102 stretched above speed 0.4, all in float) and stores ftol of the 3D
  distance between `+0x40b` and `+0x423`. In local terms that is 204 x |row 2|, but the float sums at world
  coordinates of ~15000 (a float step of ~0.002 there) leave it just under or at 204. Emulated natively at GO
  (float sums, the difference in long double): sh_t1 203.99894 for all six (Wine 203), dt_t1 204.00000 for all
  (Wine 204), cc_t1 203.999997 (203), ct_t1 203.99999 for four and 204 for v5 and v6.
- Ported: `Opponent::span`, computed in `start_boosts` (the controller's init at GO), used by the far step,
  the hidden robots' run (`+0x2b5 += +0x4c`) and the target speed (`RoboCarState::probe_span`, which had kept a
  default of 204).
- Laps after it: sh_t1 and cc_t1 unchanged (203 as before); ct_t1 within 0.5% (v0 1848 / 3660 / 5481 against
  1820 / 3633 / 5458 ...); dt_t1 lap 3 within 7.6%, the order 6, 2, 1, 0, 4, 5 (Wine 6, 2, 0, 4, 1, 5): the
  equal step lengths do not settle dt_t1, where a far robot's turn toward its next goal comes at other
  updates (robot 1 at update 307 turned about 12 updates before Wine's, with the same goal), not traced.

## The far turn on dt_t1 (2026-11-15)

- `0x4e5642..0x4e56a5` read again: b (the goal-2 angle, wrapped by the float -pi/pi/2pi and kept as a float at
  `+0x50`) is used when a >= 0 and b >= 0 or a <= 0 and b <= 0, and |b x 57.29578| < |a x 57.29578|, else a;
  the result x 0.5 (`0x51026c`). The port's rule is the same.
- Robot 1 at update 306: its b reaches +0.00002 natively, so a (-0.47) is taken and it turns; Wine's heading
  stays at 1.09 until the goal changes at update 320, so its b stayed <= 0 (b halves toward 0 each update).
- The frame's turn is `0x4530a0` (x' = x cos t + z sin t, z' = z cos t - x sin_f t on the frame's forward as
  floats) and the frame is rebuilt against the normal `+0x2e1` with float crosses and normalizations
  (`0x4e57bc..0x4e5818`). An emulation of that (a float forward carried between far steps, the angle choice in
  extended precision) changed nothing: robot 1 is already ~97 units off Wine's line at update 300, so b nears 0
  from other geometry. That gap starts earlier, 23 units at update 100 with a heading 0.01 off from about
  update 98, after the robot's near updates; not traced. The emulation was not kept.
- Robot 1 before it goes far (Wine's far log against native, updates 40..100): position within 0.1 unit, yaw to
  4 decimals and the decision steers equal through update 52; the steer then differs in the 4th decimal (update
  53: -0.0610 against -0.0606, a ratio of 1.007 that no step of the decision draw's factor, multiples of 0.01,
  gives), with the robot about 200 units from its goal, where a sub-unit position difference moves the goal
  angle by that much. It grows (update 58: 0.219 against 0.181; 68: +0.129 against -0.076), and at the switch to
  far (update 71) the robot is ~60 units off. That is the car update's float arithmetic against the original's
  x87, amplified near a goal; no rule difference found, so the dt_t1 chain stops here (2026-11-15).
