# cc_t2's bascule bridge (WMisty), 2026-11-06

## Why it mattered

On `cc_t2` the robots looped in section 1. The WGNR ground stops at x 10460 and only water lies below (surface `0x800000`), while the route layer goes on over the gap (polygon 255, y −999). The robots drove off, drowned, returned to the section's entry and did it again. This replaced the earlier reading of the failure ("robots hover on their wall rays").

## Data

WMisty #348 (record `0x5153f0`, class id `0xedda0006`), brick members from `+0x88`, 8 bytes apart:

| Offset | Member (ID) | `cc_t2` |
|---|---|---|
| `+0x88` | m_Forgas (`0x222`): the axis, 1..3 | 3 (z) |
| `+0x90` | m_MaxForgas (`0x223`) | 1.31 rad |
| `+0x98` | m_BeSugar (`0x224`) | 200 |
| `+0xa0` | m_KiSugar (`0x225`) | 300 |
| `+0xa8` | m_bqAlpha (`0x226`) | 0.95 |
| `+0xb0` | m_bqOmega (`0x227`) | 0.0873 |

Children:
- GDModel #22 and #21 (`+0x196`, `+0x19a`), each with three sensor-group-2 PRIMs. At angle 0 they span x 10287..12784, z 19338..20540, exactly over the gap, pivoting at their outer ends;
- two GDSounds (`+0x19e`, `+0x1a2`);
- GDStdBrick sensors #353 (8086, −983, 18420) and #356 (14974, −983, 18268) from `+0x1a6`, each with a GDSound.

## Init (`0x4c5840`, `0x4c57d0`)

- `0x440ab0(1, alpha, omega)` builds the biquad `+0x1b3`: c0 = 2 a cos w, c1 = −a², k = (1 − c0 − c1) × 0.28985509, b = k, 1.8 k, 0.65 k (unit gain).
- The frames are saved at `+0x11e` and `+0x152`.
- `0x4050f1(model, 1)` registers each half with the manager (`+0x18e`, `+0x192`).
- `0x4856f0(n)` walks the row's route nodes, each (links + 2) × 3 words long. `+0x1aa` points at node 14's `+0x18`, its first link. `+0x1ae` says whether that link's target is node 40.

## Update (`0x4c5b50`)

1. **State `+0x1af`:** 0 shut, 1 opening, 2 open, 3 shutting. In state 1 the angle `+0x186` is the biquad towards m_MaxForgas, in state 3 towards 0 (its absolute value). Within 1.745e-5 (`0x514864`) of the last value the state moves on, to 2 or 0, and the sounds change.
2. **Frames:** half 0 turns by −angle and half 1 by +angle, about the axis (3: `0x40312f` → `0x43b690`), from the saved frames.
3. **Sensors:** `0x403d69` with radius m_BeSugar finds a car; if the sensor's bit in `+0x18a` is clear, it is set, the sensor's sound plays and the scan stops. Radius m_KiSugar with no car clears the bit.
4. **A fired sensor** goes through table `0x4c5f14`: states 0 and 3 become 1, states 1 and 2 become 3.
5. **Route fork (`0x4c5e83`):** the link's weight `+0x1a` becomes 0xffff when `+0x1ae` differs from "state is 1 or 2", else 0.
6. **Cars:** a car in the row whose `+0x89f` belongs to a WMisty, while the state is 1 or 2, sets state 3; `+0x89f` is cleared.

On `cc_t2`, route node 14 of section 1 (polygon 237) links first to 150 (file weight 32767), then to 564, the way over the bridge (65535). So when shut, the first link has weight 0 and robots take the bridge; when open it has 0xffff and they take the other way.

## Native

- `MistyBridge` in `race_objects` runs the above.
- `CourseObjects::log_ground` includes the halves' triangles. `RaceSession::ground`/`ray` merge them with the static WGNR.
- `RoboDriver::set_route_fork` applies the weight in `choose_route_link`.
- `RaceView` draws the halves at their frames.

## Provisional

- The halves' ground: the sensor-group-2 PRIMs are taken to be the model mesh, and only vertical queries see it (no wall contact on a raised half).
- The section of the fork: the one under the first sensor; `+0x104` is not traced.
- The sensor query's distance is 3D, over the non-hidden cars.
- The sounds are not played.
- `0x405e8e` and `0x403279` are taken as the x and y turns (only axis 3 is used on `cc_t2`).
- Not compared with Wine.

## Checks

`native_drive_test cc_t2.WGM` now passes every check:
- the deck carries cars when shut;
- a sensor opens the bridge, which reaches m_MaxForgas with no deck over the gap and the halves raised;
- a new car shuts it in 133 updates;
- robots best 2 laps in 120 s, worst 1; far robots 3 of 3;
- the test driver does 3 laps.

An Xvfb capture shows the bridge with a robot crossing it.
