# WRolling, WChiselCrossover and WBlowUp (2026-11-06)

Native: `src/port/race_objects.{h,cpp}` (`RollingRock`, `ChiselCrossover`,
`BlowUp`, `path_frame`), `RaceSession::solid_contacts`, `update_projectiles`,
`robot_items`, `RaceTrack` (route links), `RaceView` (pieces).

## Compared under Wine (cc_t4, scratch prefix, player held near the brick)

| What | Original | Native |
|---|---|---|
| Rock's `+0x176` | `0x1b` (`m_loop` 0) | `0x1b` |
| Rock's x axis at the path's start | (-0.30, 0.35, 0.89) | (-0.30, 0.35, 0.89) |
| Chisel route bits `+0x123`, weights | 8, (0, 0x8000) | 8, (0, 0x8000) |
| Blown chisel's weights | (0xffff, 0) (robots blew it up) | (0xffff, 0) |
| Blown pieces after 200 updates | positions within 0.1 of the formula, scale 0.00028 | the same formula |
| Blown action | flags still enabled, counter 0 | stays on |
| Rock in motion | swings: turning at d about 2200 of segment 0 and 140 of 2, about 60 at the bottom | swings; same energy reading (`sqrt(2 g h)` = 55 for that height), no frame-by-frame comparison |
| Car told to fire | player's `+0x1c7` = 3 | 3 |

**Not compared:** a rock squashing a car; a car bouncing off the wall or rock
(the sphere normal is read from the code); a projectile blowing up the rock;
`ct_t2` and `dr_t4` (read from the code and the data only); a robot's shot at
the wall itself, though Wine shows robots do blow walls up.

## WChiselCrossover (cc_t4)

Record `0x515c9c`, vtable `0x515cc0`: init `0x4d0020`, update `0x4d0290`, hit
slot `+0x34` `0x4d01f0`. Member `m_node` (`0x26f`, `+0x88`): 98 and 101.

- **Init** runs once (`+0x12f`), on the first activation. For route node n of
  the section under the brick (section provisional, as for WMisty), compare
  links 0 and 1 (`+0x18`, `+0x24`) with nodes n + 1 and n + 2: bits 1, 2, 4, 8
  (`+0x123`). Link 0 weighs 0x8000 with bits 3, else 0; link 1 with bits 0xc.
- **Update:** unhit, the nearest car within 3000 gets `+0x1c7` = 3 if it is 0.
- **Hit:** a WackyAuto sender (class `0xb83c3db3`) returns 0, which leaves the
  car's bounce to run. Otherwise link 0 weighs 0 with bits 3, else 0xffff, link
  1 likewise with bits 0xc, the WBlowUp starts, and `+0x11e` = 1. Nothing
  undoes it during the race.
- On cc_t4 the node has three forward links: link 0 (weight 0 while whole) is
  the passage behind the wall, so robots only take it once it is blown.

## WRolling (cc_t4, ct_t2, dr_t4) and WLogModelQ

WRolling: record `0x5154e4`, vtable `0x515508`, init `0x4c7680`, update
`0x4c77b0`, end `0x4c7710`, hit `0x4c7730`. WLogModelQ: vtable `0x515390`,
start `0x4c4f60`, frame `0x4c4ea0`, activation `0x4c5000`, update `0x4c51c0` ->
`0x4c5030`.

Members: `m_88` chord (`0x22e`), `m_90` radius (`0x22f`), `m_98` gravity
(`0x230`), `m_a0` drag (`0x231`), `m_b8` range (`0x234`), `m_c0` loop (`0x235`).
`0x232` and `0x233` are not read by these routines.

- **Init:** rolling from the path's start. `+0x176` = 0xb, with 0x10 added
  without `m_loop`. Boulder activation sets the grace `+0x172` = 2.
- **Update:** the timer `+0x12f` counts down. The query (`0x4a71e0`, from the
  **brick's** position, not the boulder's) ranges over `m_b8` in state 0, else
  20000. The nearest car gets `+0x1c7` = 3 if it is 0. Then, by state:
  - 0: with `m_loop` restart rolling; otherwise a car sets state 1.
  - 2: with the action ended, no car and timer 0, go home at rest (`0x401e33`
    does nothing then).
  - other: without `m_loop`, no car and timer 0, go home at rest.
- **Step (state 1):** `v = (v - m_98 x dir[segment].y) x m_a0`, `d += v`, `spin
  += v / m_90`, wrapped once to 0..2pi. Past either end of the path `0x4be7f0`
  returns true and the state becomes 0 (the frame stays). The cars are queried
  (`0x4a60c0`, mask 0x100) after the grace; the first hit gets type 0x200 if
  its WackyAutoNormal is enabled and WackyAutoBomba idle.
- **Hit:** the timer becomes 60 **before** the class test, so a car touching it
  restarts the timer. A non-car starts the action, state 2.
- **End:** `0x4c7710` calls `0x4bfc20`, on leaving the player's section window.
  cc_t4 has three sections, so its rock is never restored once blown.
- cc_t4's path is a valley, so the rock swings and never reaches an end.

## `0x4be7f0`, the frame along a path

Points `+0x136`, unit directions `+0x13a`, lengths `+0x13e`, plus the path's
translation only. d and the segment are normalized in place; true past either
end, for d or d + chord. Flags: `a = f & 3` the axis along the chord, `b = f >>
2 & 3`, `next = a + 1` (4 wraps to 1), bit 0x10 taking the direction from the
first point to the last. `u = norm(dir x (-z, y, x))`, `v = dir x u` when `b ==
next`, else `u x dir`. Axis `a` is dir, axis `b` is u, the third is v; the
translation is the midpoint.

**Finding for WLogRide:** it uses flags 0xe, so y runs along the chord and z is
u. `analysis/log-ride-20261102.md` and the native LogRide assume z along the
chord (provisional). Not changed in this pass; listed in `TODO.md`.

## WBlowUp

Record `0x514fb0`: init `0x4bf920`, update `0x4bfb30`, end `0x4bfc20`.

- **Init:** per child, saved position p, velocity p / 13 (`0x513010`), y + 20
  (`0x51152c`), y at least 20; axes identity; counter 200; the owner's
  primitives lose bit 0x10000.
- **Update:** while the counter runs, each child scales by 0.96, moves, turns
  pi/36 about x (`0x43b2b0`), and loses 2.78 (`0x51500c`) of y velocity. The
  action is not disabled at 0.
- **End:** only if enabled: identity axes, saved positions, bit 0x10000 back,
  disabled.

## Who hits them

Slot `+0x34` callers that can reach a brick: the car's object query
(`0x46a786`, sender the car) and the projectile's (`0x475926`, mask 0x10000,
sender the projectile, which flies on). The kicker, flipper, TNT and mine calls
work on the cars from their car queries.

A sphere contact's normal (`0x482190`) runs from the primitive's centre to the
car. The response is the rock response already ported (`0x46a7d7`).

## Robots

`0x4e5ea1`: a robot with `+0x1c7` = 3 or 4 skips the wait `+0xac` before firing
at its target, still needing a car in view beyond 700, and firing clears the
state (`0x4e5ef5`). Near mode only. Far robots and the player ignore it. The
dropped weapons fire at once and leave the state set.

## Provisional

- The section of a chisel's route node (under the brick).
- `0x4a71e0` is reduced to the nearest shown car not being hit; its heading,
  section and last-section rules are not applied to the bricks' 2pi queries.
- The update order within a tick (brick, then boulder; bricks after the cars).
