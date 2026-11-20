# Kosza: the reversing car, mode 2's walk, two players' car order (2026-11-17)

Follows `analysis/kosza-motion-20261114.md`, whose open items were: a reversing car's touch not
compared with Wine, mode 2's walk `0x456af0` not ported, and the object query's car order not
checked with two players.

## A car reversing into the creature

Wine (scratch prefix, Xvfb `:77`, `build/wine-scratch/rev2_cc1.sh`): on cc_t1 the player car was turned
round at the start (Up + Left held until its row 2 z dropped below -0.95, `wmem.py turn`), then hopped
along the recorded line to (-35380, 562.9, 15000), the last two hops only while the path creature
waited (`hopk`, so that it did not stop on the arriving car). Row 2 there: (0.130, 0, -0.992), the
car's back towards the creature's approach. With the creature at t 0.364 the car reversed (`trig ...
rev`: Down, release, Down, as the port's brake-to-reverse). No robot within 21000 units (`khit` now
logs the nearest other car).

| | Wine | native before | native now |
| --- | --- | --- | --- |
| first car move | creature t 0.36807, car z 15003.2, v -0.02 | one update later | one update later |
| touch | t 0.41461, z 15290 | t 0.41461, z 15290 | the same |
| squashed | yes (row 1 y 2.5 to 0.25) | **no** | yes |
| car after the update | (-35403.5, 15179.7), v -0.17 | (-35403.1, 15176.5), v -0.17 | the same |
| restart | t 0.41884, unsquashed | | t 0.41884 |

The port's test was sign(speed) x the normal's world z (`0x45935f..0x4593df` read as row 2 x speed
against the normal turned by the car's rows). Driving forward into the creature (row 2 about +z) and
reversing into it with the back turned (row 2 about -z) both squash under Wine. The two runs share
only one thing: the car moves towards the creature in world space. The port now tests
speed x dot(forward, unit(creature - car)) > 0. Inferred from the two runs: the literal frame chain
(`0x43c930` inverts the creature's matrix, so `0x482190` works in the creature's frame, then
`0x43b980` turns the normal by the car's rows) predicts that only one of them squashes. It is not
reconciled. The port's forward is the yaw's (no pitch); the original's row 2 has the pitch too.

An earlier run (`rev_cc1.sh`) parked the car just as the creature arrived. There the creature was
squashed several times while the player's car rested. The parked car was also pushed from z 15000 to
14911, probably by robots lapping; that run logged no robot places, so nothing was concluded from it.

`native_drive_test cc_t1` checks it: `path Kosza struck by the reversing player at update 325,
t 0.41461 z 15290.0, y axis 0.250, car z 15176.5 speed -0.17`. Control: with the old test the check
fails (y axis 2.500, exit 1). All 20 courses exit 0, ctest 14 of 14.

## Mode 2's walk (`0x456af0`)

Called from `0x457f80` when `+0xe1` = 2, as `walk(path, t, &place, &tangent)` (mode 1 calls
`0x444be0(t, &place, &tangent)` on the same path). Read from the disassembly:

- t >= 1.0 (`0x510ae0`, a double) keeps its fraction (`__ftol`); a negative t is not wrapped.
- segments = n when closed (`+0x12e`), else n - 1 (`+0x122` = n, points `+0x126`); f = segments x t,
  seg = int(f) mod segments, u = f - int(f), next = (seg + 1) mod n. Each point goes through the
  path's world matrix (`0x4200e0`, `0x43b880`).
- T1 = unit(P2 - P0), P0 the point before seg (wrapping to n - 1 when closed; an open start uses
  P2 - P1); T2 = unit(P3 - P1), P3 the point after next (wrapping to 0 when closed; an open end uses
  P2 - P1) (`0x452e20`).
- Then the Hermite of modes 6/7: l = |P2 - P1| (`0x457270`, fsqrt), a = T1 l + T2 l - 2E,
  b = (E - T1 l) - a, place = ((a u + b) u + T1 l) u + P1, tangent = (3a u + 2b) u + T1 l. The vector
  operations are `0x4572a0` (add), `0x457310` (scale) and `0x457380` (integer scale, 2 and 3).

Ported as `path_walk`; modes 6/7 and mode 2 share `creature_hermite`.

Wine (`mode2_cc1.sh`, `mode2c_cc1.sh`): every course's mode 1 path has two points (cc_t1 and cc_t2 node
16/27), and on two points the walk is the chord. So the run also wrote four local points into spare
`.data` at `0x2ad3f00`: (2597.112, 0, -14.430), (630, 0, 1200), (-1340, 0, -1100), (-3311.317, 0,
82.145). They were hooked to both GDPath copies that held the original two points (`+0x126`, `+0x122`
= 4; the live one is at 0x43c53e8, its pointer unaligned, found with `wmem.py scanu`). Then cc_t1's
path creature's `+0xe1` was set to 2 during the race. Wine's node then swings about +-1200 units in x.
Compared with `path_walk` at the previous update's t (the log reads t after the move's increment), over
2183 moving updates: place within 0.11 units, heading parallel to Wine's row 2 within 0.03 degrees.
The frame at init `+0x74` turns row 2 to face the opposite way, as in mode 1. On the original two-point
path: 2255 updates within 0.08. `native_drive_test` (the synthetic part) checks one of these values: t
0.26230 gives (-36525.9, 576.7, 16201.3).

Not compared: a closed path (no course has a mode 1 or 2 creature on one).

## The query's car order with two players

Wine, cc_t1 in two players (menu: Return, Down, Return, COURSE, the two characters on one page,
Le Grand Canyon, Le Grand Canyon 1): the car table `0x6f2ce0` holds only slot 3 (player 1) and slot 5
(player 2). There are no robots in a two-player race. The manager's list (`[0x6f2d00]` + 0x48, 12 bytes
apart) reads player 1's car, player 2's car, then empty entries. So the second entry, empty in single
player, is player 2's. The port's `car_probes` gives the player and then the opponents. In two players
the only opponent is player 2 (`RaceSession`'s grid), so the order is the same. Not checked: a
two-player race with weapons or creatures touching both cars at once (the query takes the first).
