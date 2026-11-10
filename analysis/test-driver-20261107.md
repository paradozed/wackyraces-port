# Test driver: spiral ramps and the route follower (2026-11-07)

Test-only: `tests/native_drive.cpp` (`RoboPilot`, `HybridPilot`). No game code changed.

## Why `cc_t3` failed

- **Not WDicksTrick2:** its pieces are in section 3, around (40682, 1353, 19324). The stall was in
  section 2, around (27000, 32500).
- **A spiral ramp:** there the route descends into a canyon floor (sand, surface 4, y −3270) and
  climbs a spiral ramp around (26800, 32700). It has two turns 2000 apart (y −2194 and −194 at
  (24900, 33000)) and a hollow centre.
- **The robots** go down and climb it to y 731 (heading −3.09 out of the top).
- **The earlier planner** (look-ahead on the route cursor plus 0.2 × the geodesic field) climbed to
  about y 700. There its progress stayed flat (2.82 → 2.86, one large route polygon), and it fell
  into the hollow centre: no ground at (25043, 32764) between −286 and the floor. That was a
  legitimate fall.
  - Its 60-update look-ahead saw nothing, because the progress only drops on landing.
  - A fall penalty in the look-ahead did not help enough.
- **The route query** is a vertical segment from +404 to −1000 under the car, so it keeps the right
  turn; the cursor does not jump here.

## The new driver

- **`RoboPilot`:** the port's `RoboDriver` (`0x4e5d..`) gets a `RoboCarState` from the player's car
  and returns a throttle and a steer.
  - The throttle maps to accelerate or brake.
  - The keys press left while the robot's steer (`robot_steering`) is above the car's steer by half a
    step, and right while it is below.
  - The keys stop at the 40-degree lock (`0x5129d8`), but a robot's steer does not. So the throttle is
    released when the wanted steer passes 80% of the lock above speed 0.3. Without this the car left
    the spiral by its outer edge; with 1.0 / 0.4 it still did, with 0.9 / 0.35 it took 248 s, and with
    0.8 / 0.3, 162 s.
- **`HybridPilot`:** the route follower drives.
  - After 300 updates without the measure rising by 0.002, the planner drives until the measure
    passes the stall by 0.02, or for 3000 updates.
  - This rescues `ct_t4`, where the follower stopped off the route at (59046, −1213, −28267): `drive()`
    gives throttle 0 there.
- **`ct_t2`:** it runs 9 laps (`m_nLapNum`). The test drives 3 and checks the finish and standings
  only where the course's laps are 3.

## Results (`native_drive_test`, every check)

| Driver | Courses passing |
|---|---|
| Planner (before) | 15 of 20 (not `cc_t2`, `cc_t3`, `ct_t2`, `ct_t4`, `sh_t3`) |
| Route follower alone | 18 of 20 in a scratch harness (not `ct_t4`, `dt_t2`) |
| Hybrid, in the test | 20 of 20, finishing 2nd–7th, 89–270 s for 3 laps |

## Open

- **`dt_t2`, in the scratch harness:** a robot's hit at u2088 caps the speed at 0.3 and turns the
  car. It lands on route polygon 2/594, whose goals ((57716, 1159), then (58037, −760)) lead into a
  grass pocket.
  - The pocket is closed by a wall at least 3000 high at z ≈ 10, with route polygons 2/765 and 2/582
    beyond it. Neither driver leaves it.
  - In the test run the hit did not happen and the course passed.
  - Traced 2026-11-08: no object closes it. The pocket is a box (x 55657..57600, z ≈ 10..1090,
    grass 0x10, no route polygon) closed on all four sides by walls of section 2's own WGNR
    (record 75, resource `0x33d`, flags 0x200000, over 3000 high). The goals (57716, 1159) and
    582 lie on the road east of it. A car inside got there through a wall, probably pushed past
    the contact rays by the hit (not verified).
  - The original resets a car only at its reset (`0x469550`, vtable `0x51295c` slot 2) and on
    the 0x800000 surface return (`0x47014a`), both through `0x4a6730`: no stuck timer.
- Passing proves that a course can be finished, not that it plays like the original.
