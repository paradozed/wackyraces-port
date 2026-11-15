# The ranks' progress `+0x1a3`/`+0x1a7` (2026-11-13)

## The original

The rank loop `0x4a3d4d..0x4a3f1a` walks the rank table `+0x7f0` and writes, for each car:
- `+0x1a3`, the distance into the lap, and `+0x1a7` = (`+0x187` − 1) × the lap `+0xf38` + `+0x1a3`;
- a car with lap `+0x19f` ≤ 0: `+0x1a3` = 0 and `+0x1a7` = count − index − 10000.

`+0x1a3` is a sum of `0x4a3c30`(car, route, brick) over the manager's routes (`+0x2c` → `+0x20[]`, count
`+0x1c`) up to the car's `+0x1b7` and their bricks (route `+0x3c`, stride 0xcf0, count `+0x38`) up to `+0x1bf`:
- **a passed brick:** its route length `+0x34`;
- **the car's brick:** `+0x34` less `0x4a3a10`'s remainder, which is the cursor polygon's `+0x17` share. It
  is 0 when the brick's object (`0x40647e`) is not the car's `+0x2b9` and the cursor polygon has `+0x11` flags;
- **brick 0 of route 0:** the manager's `+0x34` comes off too (the start line's place in the brick), and with
  `+0x1bb` > 0 (behind the line) its length counts twice.

Then:
- **a hidden robot** (`+0x2b1 & 0xfff0000`) adds its virtual distance `+0x2b5` (`0x4a3ec4`);
- **otherwise, `0x4a3f28`:** if the cursor polygon `+0x2c5` has forward links (`+0x16 & 0xf0`, the counts
  word's forward count) and the car's brick is `+0x2b9`, then with d = the car − the first forward link's
  point (`+0x1c`/`+0x1e`/`+0x20`, put in the world by the brick's frame `0x4200e0`/`0x43b880`), `+0x1a3` −=
  (ftol(max(|dx|,|dz|)) + ftol(min(|dx|,|dz|)) >> 1) >> 2 (`0x4a403e..0x4a4085`).

So the progress is continuous inside a route polygon. The port's was the `0x4a3937` share alone, in steps
of 1/256 of the row.

## Native

`RaceSession::race_distance` subtracts the same term: the cursor keeps the polygon's first forward link
point in world coordinates (`Body::cursor_target`). Hidden robots are left out. `race_rank`, the robots'
rank scaling and the progress strip use it.

## Check against Wine (`sh_t1`)

- **The run:** Up held 30 s from the grid. The player's position and `+0x1a3` were polled every 10 ms. 104
  samples, 150 units or more apart.
- **Native:** a scratch harness put the player at each sample (`place`, one update) and read
  `race_distance(-1)`.
- **After the first crossing:** native − Wine is 3952 at 48 samples, 3951–3956 at 26 more, and 3961 at one.
  This constant is the start-line offset, the same for every car, so it does not change a rank.
- **Control:** without the term the difference scatters from 4103 to 22066.
- **Before the first crossing:** the difference is 25575 (native counts the car as at the end of a lap; Wine as
  behind the line, negative). It is the same for every car on the grid.
- **Outliers:** four samples at the first hairpin, (−6228..−6076, −2950..−3288). Wine's `+0x1a3` stays exactly
  5608 there. The harness started each sample from a fresh copy of the grid session, so its held cursor was
  the grid's. Explained below.

## The cursor's writer and a miss

- **The writer:** the player's `+0x2c5` is written by the shadow update `0x47c180`. Its query (`0x402199`) runs
  from 403.7 above the car to 1000 below, with mask `0x818000ff`, or `0x80000000` in the far mode (`+0x12a` > 0).
  A route hit (result flag 0x100000) sets `+0x2b9`/`+0x2bd`/`+0x2c1`/`+0x2c5` and keeps the old polygon in
  `+0x2cd` (`0x47c29e..0x47c2e7`). The port's `RaceTrack::route` already uses this ray and mask.
- **A miss** clears all four (`0x47c331..0x47c345`). The rank loop then skips the car (`0x4a3d91`, `+0x2c5` = 0
  and not far), so `+0x1a3`/`+0x1a7` keep their last values. The port kept the old cursor and recomputed.
- **At the first hairpin** the port's query misses at the four samples too (no route polygon under the car there).
- **Native now:** the player's distance is held from the first miss until the query hits again
  (`Body::cursor_lost`, `held_distance`). Provisional for the robots, not applied: their `+0x2c5` is also
  written by their driver (`0x4e82be`), and the order of the two writers is not traced.
- **Check:** one continuous session placed at the Wine samples in order. The four hairpin samples now hold
  (native 9527, Wine 5608, difference 3919: each build freezes at its last value before the miss, and the
  samples are 150 units apart). Four later samples near (−6000, −5300) hold in both builds as well; there
  `place()` puts the harness car on the cliff top (y 3500–4400, Wine 733).
- **Not ported:** the HUD fraction's fallback without a polygon (`0x4a394e`).


## Test change

- **What broke:** `native_drive_test`'s paint-gun check on `dt_t1` fired from the grid straight ahead and scored
  a hit only at update 109, on a robot that had driven into the shots' lane after the start. With the term, the
  player's grid progress is 1597 (1835 before). The robots' rank scaling then differs from the start (update
  63), and with it their lines, so the shots missed.
- **The fix:** the check now turns the player toward the nearest grid robot during the countdown. The shot
  paints it (type 4, blue blast) and the gun still ends at update 31.
- **Checks:** all 20 courses pass `native_drive_test` (2026-11-13); ctest 14/14.
- **Checks after the cursor hold:** all 20 courses pass `native_drive_test` (2026-11-13); ctest 14/14.

## The share byte: `+0x11`, not `+0x17`

- **Dead code:** the routine at `0x4a38d0`, which holds `0x4a3937`, has no caller. Neither its thunk `0x405d3a`
  nor the routine's address appears anywhere in the executable. The port took its share from there: the
  counts word's high byte, `+0x17` on a quad. Its no-polygon fallback (`0x4a394e`) is dead too: the length
  `0x4a3830` from the car to the brick's `+0x50` object.
- **The live remainder** `0x4a3a10` (used by the rank loop through `0x4a3c30`) reads the route flags' byte 1:
  `+0x11` × the row length `+0x34` >> 8 (`0x4a3ab3..0x4a3aba`).
- **Where they differ:** the two bytes agree at 21 of 24 sampled `sh_t1` polygons. They differ at the start
  polygon 189: `+0x11` = 239, `+0x17` = 146.
- **Wine settles it:** before the first crossing, native − Wine was 25575 against 3952 after it. The gap
  21623 is (239 − 146) × 59521 / 256 = 21623.4, with 59521 the length of section 0. With `+0x11` natively,
  the samples before the crossing now also give 3952 (62 samples exactly 3952, 26 at 3951–3955). The remaining
  outliers are the held values (3919, 3989) and one cliff-top placement (4185).
- **Native:** `RouteNode::remaining` = route flags byte 1. The "fresh grid shows 7th" rule in `rank()` is
  unchanged (not re-checked against the Wine start frame).
- **Checks after the byte switch:** all 20 courses pass `native_drive_test` (2026-11-13); ctest 14/14.
- **The grid rank** (scratch harness, a fresh race on four courses): after 200 updates the player is 7th on
  `cc_t1`, `sh_t1`, `dt_t1` and `ct_t1`. With the old `+0x17` byte, `sh_t1` showed 1st there (the start polygon's
  146 put the player ahead), so the switch also mends that. On the very first update `cc_t1` and `sh_t1` show 1st
  with either byte (the robots' cursors are not set yet). The original ranks every car with lap `+0x19f` ≤ 0 by
  its rank-table index instead (`0x4a3d66`: count − index − 10000).
- **Before the first crossing (ported, 2026-11-13):** `race_distance` returns (n + 1) − index − 10000 for a car that
  has not started, n being the robots' count. The index is the car's place in the rank table, which
  `0x49d806..0x49d841` fills from the highest slot down: robots in reverse grid order, then player 2, then
  player 1, which comes last.
  - `race_rank(-1)` is now 7 from the first update on the four courses.
  - **The HUD shows `+0x1af`:** `0x4a4650` copies it into the per-player block (`0x6f314c`, `0x4a46de`). WHudBrick
    draws it with a `%d` format (`0x485d8d..0x485da0`) and picks the suffix string `0xb6` + rank (`0x485dda`). So
    `rank()` (HUD, audio, standings, roulette place) is now `race_rank(-1)`, no longer its own "level counts as
    ahead" formula. The fresh grid shows 7th from the first update on the four courses; all 20 courses pass.
  - Not compared frame by frame with Wine.
- **The table order:** `0x49d79d..0x49d7f0` shuffles the robot entries with count/2 random swaps; the port's
  `race_roster` makes the same swaps and gives robot slot i to entry i + 2. So the table, from the highest entry
  down, starts with slot 5's robot, the back row beside the player. Before the crossing that robot ranks 1st
  and, having no car ahead, gets the rank boost.
- **Wine (`cc_t1`, 2026-11-13):** the player's `+0x1a7` before its crossing is `0xffffd8f1` = −9999 = 7 − 6 −
  10000 (index 6, last), as natively. By the time the key script reached the grid, the robots had been racing for
  a while (laps 1–4 a few seconds later, far mode, `+0xb4` 1.20–1.25). So the earlier Wine run behind
  "straight ahead drowns at the first bend" had no robots near the player.
- **Test change:** with the pre-crossing ranks the native robots' start changes, and their contacts turned the
  test car (yaw 0.147 → 0.668 by 60 updates in). It then missed the river within 300 updates. The drowning check
  now holds the robots (`set_robots_enabled(false)`), as the Wine case had none nearby. All 20 courses pass
  (2026-11-13).

## The robots' cursor (2026-11-13)

- **One writer:** `0x4e82be` is not a second one. It runs only when `+0x2c5` is already 0 (`0x4e6014..0x4e6020`),
  and then zeroes `+0x2b9`/`+0x2c9` with it. So every car's cursor comes from its shadow query `0x47c180`.
- **Far mode:** with `+0x12a` > 0 the query's mask is `0x80000000`, the route layer alone, so it still finds the
  route polygon. Wine (`sh_t1`, the player left on the grid, slot 0's robot polled for 40 s): in far mode
  (`+0x2b1` = 0x1000, `+0x12a` = 1) `+0x2c5` was never 0 in 982 samples. It advanced along the route and
  `+0x1a7` rose with it (2486 at 3.8 s, 79989 at 39 s). At GO, still near and on the grid, it read 0 once.
- **Tried natively:** the player's miss rule applied to the robots, with a fresh query each update. On `ct_t3` the
  avoidance aims on the route layer fell from above 90% to 81% (969 of 1193). `sh_t1`'s avoiding updates rose from
  2391 to 3607, and `cc_t1`'s worst robot lost 2 laps. Natively the robots leave the route layer more often (the
  provisional wall and edge responses). A held progress kept them in the early window (progress under 20000),
  so the change was reverted. The robots keep their driver's module as cursor.
- **Later the same day:** with the start boosts ported (`analysis/start-boost-20261113.md`) the robots leave the
  player behind, and the drowning check passes with them running again. Holding them there was undone.
