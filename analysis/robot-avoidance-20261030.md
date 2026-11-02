# Robot avoidance and road edges (2026-10-30)

WCtrlRoboCar's avoidance of the cars ahead (`0x4e69bd..0x4e72ea`, inside the
route-following states 1/2 of `0x4e4f10`) and the road edges it reads
(`0x468890`, thunk `0x4036e8`). Native: `road_edges`, `next_polygon` and
`RoboDriver::avoid` (`src/port/race_robot.cpp`), fed by
`RaceSession::other_cars`.

## Per-update caches

`0x469550` (start of every WackyAuto update) sets `+0x35d` = −1 and `+0x2ed`
= −1.0. Both are filled lazily the first time a robot asks:

- `+0x2ed` (`0x4e6c76..`): the distance the car moved in its own update,
  position − `+0x173`, as `max(|dx|, |dz|) + min(|dx|, |dz|) × 0.5`
  truncated (`0x4ee040`).
- `+0x35d`/`+0x361`: `0x468890`'s distances to the road edges.

## Road edges (`0x468890`)

Recomputed when the module `+0x2c5` changes, or the next polygon `+0x2c9`
(with `+0x2b9` set). For a car without WCtrlRoboCar (`+0x18 → +0xc0` = 0),
`+0x2c9` is first rebuilt: the last forward link (12-byte entries at
`+0x18`, count `+0x16 >> 4`) with a target other than `0xffff` and a
non-zero weight. `+0x2f1` is the polygon after that, by the same rule.

- The module's vertices (`+6..+0xc`) that the next polygon does not share
  go to `+0x2fd`, `+0x309` (trailing edge); up to two shared ones to
  `+0x315`, `+0x321` (leading edge). Unless exactly two are not shared,
  `+0x2fd` = 0 and `+0x35d` = −2 (no edges).
- With m the trailing midpoint and l the leading one, f = (−(l.x − m.x),
  l.z − m.z): the pairs are swapped when `(v.z − m.z) f.x + (v.x − m.x) f.z`
  < 0 for v = `+0x309` (then v = `+0x321`), so `+0x2fd`/`+0x315` run along
  one side ("left") and `+0x309`/`+0x321` along the other. The points are
  then transformed to world (`0x4200e0`, `0x43b880`).
- `+0x32d` = unit(`+0x315` − `+0x2fd`), `+0x339` = unit(`+0x321` −
  `+0x309`) (`0x452e20` normalizes in place).
- lat = unit(mid(`+0x2fd`, `+0x315`) − mid(`+0x309`, `+0x321`)).
  `+0x345` = (300 lat.z + 600 lat.x, 600 lat.y, −300 lat.x + 600 lat.z),
  `+0x351` = (300 lat.z − 600 lat.x, −600 lat.y, −300 lat.x − 600 lat.z)
  (`0x512610` = 300, `0x512a14` = 600): 300 along the route, 600 to the left
  or the right.
- `+0x35d`/`+0x361`: the car's offset from its foot on the left/right line,
  measured like `+0x2ed` (`0x46914a..`).

## Avoidance (`0x4e69bd..0x4e72ea`)

Only when `app+0x1a60` ≠ 1, the speed `+0x3ef` > 0.07 (`0x512a54`), outside
the two-player modes (`0x6f2d00+0xf30` 2/3 with `+0xb8` > 0), and while the
slow counter `+0xa8` < `0x65`. Best time starts at 120 (`0x42f00000`). For
each car in `0x6f2ce0[0..7]` other than itself (bit `1 << slot`, cleared in
`+0xc8` first):

1. Its module must be this robot's `+0x2c5` or `+0x2c9`.
2. d = its position − ours. Within 300 the two headings must agree within
   ±20° (`0x5129e8`/`0x5129f4`), else skip.
3. With dx, dz = d × 1/4096: under progress `+0x1a7` 20000 and with the bit
   already in `+0xc8`, atan2(dz, dx) within ±120° (`0x516b48`/`0x516b44`) of
   our heading; otherwise d · forward ≥ 0.
4. k = 5 under progress 20000, else 15. If its `+0x2ed` − k ≤ ours, the bit
   is set and t = octagonal(dz, dx) / (ours − its + k).
5. If t < the best: both cars' edges must exist and our forward must run
   along our left side (`+0x32d` · forward ≥ 0). With our edge distances
   L, R and its L', R' (and the last side `+0xc4`):
   - if L < 301, L' < 701, or (L' < L, R > 699 and `+0xc4` ≠ −1): when
     R > 300, R' > 700 and (R ≤ R', L < 700 or `+0xc4` = 1), side 1 and the
     aim = its position + min(R' / 700, 1.3) × its `+0x351`
     (`0x516b40` = 1/700, `0x512a40` = 1.3);
   - else side −1 and the aim = its position + min(L' / 700, 1.3) × its
     `+0x345`.
   The best time becomes t only when an aim was set.

After the loop `+0xc4` = the side (0 when none) and `+0xc8` = the mask.

**Steering** (`0x4e79a4..`): with a best time under 120 (`0x512bf4`), the
angle b = heading − atan2(aim − position) gives `+0x2ad` = k × b × −0.4
(`0x5129d0`) up to progress 20000 (`< 0x4e21`), else −k × b, with k
m_rDirModifier. It replaces the goal steering (no random factor, no
m_aFWAdjustAngle gate, no pickup turn). The target speed's angle is left
at its default (provisional).

## Native

- `RoboCarState` carries the robot's slot (vehicle), progress
  (`race_distance`) and the other cars by slot: position, the position
  before their own update (robots `Opponent::previous`, the player
  `player_previous_`), heading, module (robots: the driver's `module()`/
  `next_module()`; the player: `route()` and `next_polygon`).
- The edges are computed on demand, not cached; the sort runs in world
  coordinates (the section transforms are rotations about y, which keep its
  sign). Fewer than two shared vertices are treated as no edges
  (provisional: the original keeps stale `+0x315`/`+0x321`).
- Modules in different sections never give edges (provisional).

## Verification

- `native_drive_test` (robots kept near, 3600 updates): 1342..3161 avoiding
  updates per course; ≥ 98% of the aims lie on the route layer and ≥ 95% of
  the edge spans are 200..8000 wide.
- Seeds 0..5, robots kept near, total robot laps in 120 s with / without
  avoidance: `cc_t1` 82 / 77, `sh_t1` 83 / 78, `dt_t1` 95 / 94, `ct_t1`
  80 / 79. Trapped robots (under 2 laps): 2 / 6.
- The remaining traps are on wall-flagged slopes (record flag 1, surface
  `0x1000001`, normal y 0.34..0.78) where `0x4e82cd` cuts the robot's
  throttle: off `cc_t1`'s bend near (−60600, 12400) and `sh_t1`'s jump at
  (22000, −19600). The human cars' slide off such slopes (`0x46a5a3..`,
  `0x4721f0`: the normal's horizontal part × (n.y + 1) × 20 per update,
  re-placed by a ray, out after 5 ordinary contacts, a rise over 1 or 400
  updates, with a 7-update cooldown `+0x247`) does not apply to robots:
  `0x46c858` clears their `+0x157`. How original robots avoid or leave
  these slopes is not known.
- **Wine, `cc_t1`'s bend at (−60500, 13000)** (2026-10-30). The player was
  walked section by section to (−57000, 1348, 14172) (section 2; a direct
  jump to another section is undone) and parked on the road; the six robots
  were logged every 100 ms for 200 s (16 passes). No robot left the road
  (lowest y in the bend 1373). The robots passed the parked player at
  z 14650..14790 on their first passes. Mean x where the line crosses
  z = 13000: −60849 (−60636..−61036); z = 12000: −61270 (−61030..−61501).
- **Native, same placement** (seeds 0..5, 200 s, 108 passes): x −60930 at
  z = 13000 and −61300 at z = 12000, no fall; 7 of the 108 crossings were
  avoiding. So the falls onto the bank only happen in the test mode with
  every robot kept near, in the first-lap pack, a state the original does
  not produce away from the player; it was not compared.
