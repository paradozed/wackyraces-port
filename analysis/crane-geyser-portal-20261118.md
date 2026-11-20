# The crane's squash, dr_t4's geyser and a view through a portal, against Wine (2026-11-18)

Follows `analysis/course-objects-census-20261117.md` (its three open items) and
`analysis/saw-crane-gate-road-20261107.md`, `analysis/lap-line-geyser-20261106.md`.

## dr_t3's crane: the third arm's squash (compared; one rule added)

Wine (`build/wine-scratch/crane_dr3.sh`, `wmem.py crane`). The WJunkyard object is found by its vtable
`0x515b48`. Its third arm (`+0x12f`, node 706) has a PRIM sphere centred at (-3.145, -121.940, 17.785).
The logger composes that centre up the arm's parent chain (`+0x10`), from each node's local rows `+0x2c`,
`+0x38`, `+0x44` and its place `+0x54`, the root excluded. At the first sample this gives (116.5, 325.0,
4712.6), the native arm's first drop. The player car went along a robot's line to a drop point,
(-380, 2.6, 3854), and was left there for 90 s.

Four squashes (type 0x200) by the arm, giving the sphere-to-car distance before the hit and at it:
353.7/300.6, 344.3/286.9, 348.2/321.9 and 317.1/332.9. In the last two the car was moving; taken from
the car's place before its update, they are 311.6 and 314.1. All four fit the native reach, 215 + 100 = 315
with the radius unscaled (native: 343.3 no hit, 285.9 hit).

What differed: in the original the squashed car is **pushed off the sphere**. It moved (-6.3, -3.3),
(-16.8, -8.8), (-30.6, -16.5), (-48.1, -26.6) in the updates from the hit, away from the sphere's centre. The
port's car stayed put. The arm's PRIM has sensor group `0x10000`, the same bit that makes the rock's and the
walls' spheres solid to the car's own object query (`0x46a786`, the response `0x46a7d7`). So
`CourseObjects::solid_spheres` now includes the third arm's sphere at its current world frame.

Native with the car at Wine's offset from the drop's lowest point (69 x and 47 z off: (-450.5, 3806.3)): hit
at 300.6 (354.3 before), then (-6.3, -3.2), (-16.6, -8.7), (-30.3, -16.4), (-47.4, -26.5).
`native_drive_test dr_t3` checks it. Control: without the solid arm the car does not move (exit 1).

## dr_t4's geyser (compared; one rule added)

Wine (`geyser_dr4.sh`, `wmem.py geyser`). The WGeyser's vtable is `0x515664` (record `0x515640` + 0x24; slot 2
the update `0x4c8cc0`, slot 4 the init `0x4c8bd0`). What the run read:
- the state is `+0x17b`;
- the aim is the brick's own place `+0x54`: the aim is written into the brick's frame at `0x4c8f00`; `+0x19f`
  is not it;
- the three values are `+0x157`, `+0x15b`, `+0x15f`, stored after `fabs` (`0x4c910a`), with the counter
  `+0x11e`;
- the chosen car is `[+0x19b]`.

The car went near the geyser along a robot's line. At the first eruption (state 1 to 2) the logger hopped it
onto the aim + 50 x.

- The three values are equal in both builds at every update (`m_Delay` 6; e.g. n 33: 1.0193, 0.9875, 0.8907;
  peak 1.0224; state 3 towards 0.6).
- The rise (100 an update, centred) and the first top (674.4) are equal.
- Then the original lets the car fall free at 1275..1292 above the aim and catches it again at 1268.9
  (+100 to 744.1 at n 37). The port had snapped it to the top at n 33.
- The cause is the car list query: `0x4c91ed` calls `0x405051` -> `0x4a7e70` with range `m_Height` (1270) and
  flags 7 from the aim. Each axis must be strictly within the range (`fabs`/`fcomp`), then the truncated
  squared distance must be below range² (`0x4a7fac`). A car above aim + 1270 is not seen, whatever the
  column's height (1270 x h, up to 1298). Ported for WGeyser (the fountain's update does not call
  `0x405051`).

After the fix, native with the robots off (one was in the column in the native run, under the player) matches
Wine to the printed digit over the whole eruption:
- 674.4 at n 24, the fall to 644.1 at n 36, 744.1 at n 37;
- the catches 105.4 -> 205.4 and 122.1 -> 222.1 in state 3;
- -625 by state 5's n 30.

`native_drive_test dr_t4` checks those values. Control: without the range test the car is caught at n 33
(669.7 against Wine's 661.1).

## A view through a portal (dt_t4): what differs is the far plane

Portal 1043 (5000 x 4500, facing z, between rooms 1031 and 1029). Both builds viewed it from (-5053.7, 1221,
28030), 2070 south of it, with the car's grid heading (+z). Wine: `dt4_portal.sh`; native: `WPC_PLACE`.

Between the glass towers the original draws a stone building with arched windows and a pointed roof, tinted
purple by the fog. The port showed the blue skyline backdrop there instead. Checks:
- The player's group `+0x104` and the camera section `0x5472c0` are 0 in Wine, as natively.
- With every batch drawn in place, and with no section mask (temporary builds, reverted), the building was
  still missing.
- The rooms have no descendants. Their lists are `0x01` (a count, then the neighbouring rooms) and `0x02` (a
  count, then node ids, filled only for the car park's rooms 990..1003).
- With the far plane at 60000 (temporary build, reverted) the port draws the building.

So the building lies beyond 20000 from the camera. The port clips there: every WTrackCamera and the shared
WFollowCamera_2 carry far 20000. The original (renderer 1) still draws it, fogged. The rooms and portals change
nothing here. Not ported: how renderer 1 clips and fogs geometry past the camera's far plane. That needs its
clip path traced (the port's fog follows the note in `race_view.cpp`, `0x4ae060`).

Checks: ctest 14 of 14; all 20 courses exit 0 in `native_drive_test` with both changes.
