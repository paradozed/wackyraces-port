# WRotoHill, WMagnet, WVolcano, course WRot and WBumSteer (2026-11-06)

Native: `src/port/race_objects.{h,cpp}` (`RotoHill`, `Magnet`, `ModelSpin`, `Geyser::volcano`),
`RaceSession::update_objects`, `settle` (`Body::hover`), `robot_items`, `RaceView`.

## WRotoHill (dr_t2; vtable `0x515c48`, update `0x4cf6f0`)

Members: `m_88` radius (`0x268`, 1000), `m_90` step (`0x269`, 0.00436), `m_98..m_a0` (`0x26a`,
`0x26b`: 20..40), `m_a8..m_b0` (`0x26c`, `0x26d`: 100..200), `m_b8` rise (`0x26e`, 4).

Per car of its section, in bits 2..4 of `+0x1f3`, with the turn speed `+0x1f7` and timer `+0x253`:

| Phase | Update |
|---|---|
| 0 | within 2 x `m_88` with no gadget effect (`+0x385` = -1): a robot gets `+0x1c7` = 0xb. Within `m_88`: speed 0, timer `m_98..m_a0`, `+0x93b` saved and zeroed, phase 1 |
| 1 | the turn speed grows by `m_90`; at 0 the timer becomes `m_a8..m_b0`, phase 2 |
| 2 | holds; at 0, phase 3 |
| 3 | the speed falls by `m_90`; below 0, phase 4, `+0x93b` restored |
| 4 | past 10000 from the centre: phase 0 |

In phases 1..3 (`0x4cfa50`): speed and `+0x403` 0, slots cleared, heading + turn speed, offset turned
by minus it (`0x4530a0`), y + `m_b8`, airtime 0. `+0x12a` is not modelled (cleared at `0x46a1ab`, its
other car uses not traced).

**Compared under Wine** (a capture at the table, player walked there, no steering): phases in order,
phase 2 about 5 s and phase 3 about 1.1 s; y + 4.0 an update; heading steps 0.0393, 0.0436, 0.0480,
0.0524, 0.0567 (step 0.00436); offset (44, -333) -> (57, -330), the native turn's result; after
release the car falls, airtime up to 36. **Not compared:** a robot's capture, airtime during the hold
(read back as 1 under Wine, after the car's own update).

## WMagnet (dr_t3; vtable `0x515d38`, init `0x4d0940`, update `0x4d09e0`)

Members: on `m_88..m_90` (`0x270`, `0x271`: 30..60), off `m_98..m_a0` (`0x272`, `0x273`: 60..90),
turn `m_b8` (`0x276`, -0.244). `m_a8`, `m_b0` (`0x274`, `0x275`) are not read by these routines. Radius
`+0x123`: the distance between its **first and third** children, truncated.

While on, each car of its section with |dx|, |dy| below the radius, |dz| < 1000 and dx^2 + dy^2 below its
square gets a type-2 slot: yaw `m_b8`, decay 0 (`0x4d0b49`).

**Compared under Wine:** the radius (266; my first reading of the children gave 1986), the on and off
spans (on about 1.5 s, off about 2.5 s), its section 2. **Not compared:** the turn on a car. The placed
car was in section 3 there and was not turned, and the placements then threw it off the course.

## WVolcano (dr_t2; vtable `0x51576c`, init `0x4c9cc0`, update `0x4c9cf0`)

The geyser base with states 1 and 2 swapping every `m_a8` updates, values rising from 0.6 to 1, and no
column test: it only pulses. **Not compared under Wine; not seen on screen.**

## WRot on course models

The menu's action (`0x497b00`), type 2: unbounded, wrapped, `m_StepRotDeg{X,Y,Z}` per update. dr_t2 and
sh_t2 each have one, with no primitive, so it only shows. The turn order is the menu note's reading.
**Not compared under Wine; not seen on screen.**

## Robots

`0x4e5a4b`: `+0x1c7` = 0xb makes MonsterTires count (`0x2ab07a8`), cleared on use (`0x4e5d68`). The other
vehicle gadgets' use on 0xb stays unported.

## WBumSteer (dr_t2; vtable `0x515bd0`, init `0x4cec40`, update `0x4cede0`, end `0x4ced70`, hit `0x4ceba0`)

Children: two gates (0, 1), two runners (2, 3), two GDPathQs (4, 5). Members: `m_88` (`0x265`, 60), `m_90`
(`0x266`, 0.95) and `m_98` (`0x267`, 0.0873) for the gates' biquad. State `+0x11e`, timer `+0x122`, distance
`+0x256`, hit bits `+0x25a`.

- **State 0:** distance + 100 an update (`0x510414`); each runner takes `0x4be7f0(path, 2000, &d, &0, frame,
  0xe)`; past the end, state 1. A runner squashes cars (mask 0x100).
- **States 1, 2, 3:** `m_88` updates each. 2 -> 3 clears the bits, 3 -> 0 sets the distance back to 0.
- **Gates:** target 75 degrees in states 1 and 2 or with its bit, else 0; biquad; degrees to radians (`0x511518`),
  absolute, folded above pi/2 (`0x513328`), negative for the first gate; `0x43b690` on the saved frame.
- **Hit:** a car on a gate's solid sphere sets bit 1 (first gate) or 2, gets type 0x200 if idle, and the slot
  returns 0, so it bounces as well.

**Compared under Wine** (section 0, read every 0.5 s for 20 s): the runner's position against the distance (identical
points, such as (8545, -15, 5689) and (12279, -15, 8376), allowing for about 5 updates of read lag), its axes Y (0.81, 0,
0.58), the end distance 28800, the timer 60, the state order, the gates' peak (0.499, -0.499) and settling (about 0.48),
and a robot opening a gate on its own (bits 2, gate -0.474, the other shut). **Not compared:** the squash on a car.

**Test threshold:** the runners and gates knock robots off the line, so `dr_t2`'s avoidance aims on the route layer fell
to 86.9 %; that check's limit is 85 % there (90 % elsewhere). No comparison of that figure with Wine.

## WLogRide's frame corrected

`0x4be7f0` with flags 0xe puts y along the chord and z on the normal, then `0x43b4a0` turns it by pi. The earlier native
frame had z along the chord. Compared under Wine on `sh_t3`: at a read of about offset 4900, Wine X (0.696, 0.009, 0.718), Y
(0.718, 0.009, -0.696), Z (-0.013, 1, 0), world about (175472, 5086, -1265); native at 4800 X (0.757, -0.003, 0.653), Y (0.653,
-0.003, -0.757), Z (0.004, 1, 0), and at 28.6 a step about (175473, 5086, -1264) at 4900. The two offsets differ by read lag.
