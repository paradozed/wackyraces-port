# WRolling's squash under Wine, the bricks' query at the seam, the chisel's section (2026-11-17)

Follows `analysis/rolling-chisel-blowup-20261106.md`.

## The rock squashing a car (cc_t4, compared with Wine)

Wine (scratch prefix, Xvfb `:77`, `build/wine-scratch/rock_cc4.sh`). The player car was hopped along a
robot's line recorded natively (`line7`, 32 points, then two short hops) to the rock's valley floor at
(25800, 60, -2230). It kept its start heading, row 2 (1, 0, 0). `wmem.py rollog` logged, on each change:
- the WRolling's timer `+0x12f`;
- its boulder's (`[+0x11e]`, the WLogModelQ that `0x4c77c5` reads) state `+0x152`, v `+0x15a`, d `+0x15e`,
  segment `+0x162` and grace `+0x172`;
- the car's place and row 2, its WackyAutoBomba's (`+0x36d`) type `+0x2c` and its `+0x1c7`.

The car arrived with the rock in segment 2 going away (d 104.1, v 115.1). Native (`rock2.cpp`): the player
placed at the same spot, facing the same way, when the rock is in segment 2 going away (d 133.8, v 127.4).

| | Wine | native |
|---|---|---|
| rock d, the last update without a hit, and the first with one (segment 1, going back) | 1274.2, 1155.3 (v -118.9) | 1302.0, 1168.3 (v -133.7) |
| the car's hit | type 0x200, `+0x1c7` 3, timer 60 | type 0x200, timer 60 |
| car, 1 update after | (25788.4, -2227.6) | (25787.8, -2227.7) |
| 2 updates | (25777.9, -2223.8), row 2 (0.174, 0.985) | (25777.1, -2223.9), (0.174, 0.985) |
| 3 updates | (25774.7, -2219.0), (-0.876, 0.483) | (25775.0, -2219.1), (-0.876, 0.483) |

So the squash, the slide and the spin match. The touch distance agrees: both builds hit at the first
update with d below about 1160..1270. The rock is about 12% faster natively at that point; its swing has
decayed (drag 0.999 an update) for a different time in each build. Not compared: the swing's amplitude
over a whole race (it depends on when the rock started, at the first car within `m_b8`).

`native_drive_test cc_t4` checks it: `rock squash of the parked player 33 updates after the placing, type
200, timer 60; car 25787.8,-2227.7 then 25777.1,-2223.9 r2 0.174,0.985 then 25775.0,-2219.1 r2 -0.876,0.483`.

## The bricks' query and the loop seam (`0x4a71e0`)

All three bricks call it the same way. The calls are WRolling `0x4c780a`, WChiselCrossover `0x4d02c4` and
WDicksTrick `0x4c802a`, with the arguments (owner = the brick, its frame `0x4200e0`, section = its group
word `+0x104`, angle 2 pi (`0x40c90fda`), range, 0, 0, flags 1).
- Flags 1: racers only, so the second list is skipped.
- Angle 2 pi: the cone always passes.
- During the update `0x5472bc` is clear, so every group's matrix is the plain `+0x98`: the brick's own place
  is not moved.

What the port had left out (`nearest_car`): a car in the last section seen from group 0 goes through the
loop transform, and the reverse (as `RaceSession::find_target`). Ported: `CourseObjects::set_seam`
(RaceTrack's wrap, unwrap and last section) and `nearest_car(cars, at, range, group, seam)`, with the group
the brick's linked section (the groups are the linked sections, `analysis/section-culling-20261107.md`).

Effect: cc_t4's rock (group 0, three sections; range 20000 once moving) over 9000 updates of an idle race
(`bwrap.cpp`). It found a car on 9000 updates with the seam against 8309 without, and a different car on 1376
(the first at update 4339). On ct_t2 (group 3, the last) and dr_t4 (group 2, not at the seam) nothing
changes in the same run. Not compared with Wine: a robot across the seam as the rock's nearest car.

## The chisel's route section

`0x4d0068..0x4d007f` takes the route from the section of the brick's group `+0x104` (section 0 when the
group is out of range). It is not the section under the brick. The port still finds the route under the
brick (`RaceTrack`). On cc_t4, the only course with chisels, both chisels' groups (linked sections) are 1,
and so is the section under them (checked natively). So nothing changes; the comment now says this.

## Still not compared with Wine

- a car bouncing off the wall or off the rock while driving (the sphere normal is read from the code);
- a projectile blowing up the rock;
- ct_t2's and dr_t4's rocks;
- a robot's shot at the wall.

Checks: ctest 14 of 14; all 20 courses exit 0 in `native_drive_test` with the seam change; cc_t4 exits 0
with the squash check.
