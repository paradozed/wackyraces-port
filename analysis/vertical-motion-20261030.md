# WackyAuto vertical motion and ground contact (`0x46a4a0`, 20261030, analysis only)

Read from a headless decompile of `0x46a4a0`, made from the local Ghidra project opened read-only; the output stays in the scratchpad. Nothing is ported yet, and nothing is compared with Wine.

## Motion

- **The step.** The update's motion is (front probe − rear probe) × speed (`+0x40b − +0x423` from `0x468280` mode 2, × `+0x3ef`). It includes the probes' vertical difference, so a pitched car climbs or dives along its own axis, and a ramp's lip launches it with no special rule.
- **Pushes.** The slot forces (`0x4727b0`/`0x472890`) are added. Human cars scale the motion by `+0x163`. Manager phases 1 and `0xc` zero it.
- **The fall.** min(`+0x8a7`² × `m_gravity` `+0x98`, 100) (`0x510414`) comes off the motion's y. That is a displacement per update, not an integrated velocity. `+0x98` is 9, and `0x4e45e0` doubles it to 18 for robots. Airborne for t updates, the fall is 9, 36, 81, then 100 per update.
- **Airtime `+0x8a7`.** 1 while any contact holds; +1 per update with none. Past 10 updates, the pitch `+0x267` and roll `+0x26f` are × `0x512a38`.

## Ground contact (after the move)

- **Human cars.** Each probe (FL `+0x40b`, FR `+0x417`, RL `+0x423`, RR `+0x42f`) casts a ray, mask `0x18000ff` (`0x4a4830`), from the probe + up × `0x512a3c` down along the car's down axis × `+0x93b`.
  - A hit stores the point in `+0x43b`, `+0x44f`, `+0x463` or `+0x477`, and its flags in `+0x447..`; a miss marks 123.456.
  - The first hit object is remembered (`+0x89f`, `+0x8a3`).
  - Flags `0x1000001` set `+0x157` (1..4, the probe) and store the normal in `+0x23b`.
- **Robots** (`+0xc0` = 1, not battle, `+0x156` = 0). One ray from the centre. On a hit, the four contacts are rebuilt around it with the template spreads `+0x15b`/`+0x15f`. The car is set at the hit + normal × `0x511894`, oriented by the normal (`0x452ed0`, `0x43c170`), and the case is 20.
- **The solve.** A `switch` on the hit mask (bits per probe) recomputes the pitch `+0x267`, roll `+0x26f`, the position and the contact points, through `0x472070`, `0x472030`, `0x471ff0`, `0x4572a0` and `0x457310` (vector helpers, not yet identified).
  - It also sets `+0x8ab`/`+0x8af` (front/rear on ground, ±1) and `+0x17f`.
  - 0 contacts: airborne (`+0x8a7` + 1).
  - 1 (FL): `+0x8ab` = `+0x8af` = 1.
  - 3 (FL+FR): the roll from the two front points.
  - …and so on up to all four.
- **Other details:**
  - Above airtime 1, a ray (mask `0x1e000ff`, from the position + `0x512a44`) lifts the car out of a surface it went into.
  - Contacts on flags `0x600000` with airtime > 4 set `+0x157`.
  - `+0x237` counts updates with `+0x157` contacts.

## Car-to-car (found here)

- After the fall, `0x4a60c0` queries other cars (mask `0x100`) with the car's sphere, unless the hit car's WackyAutoBomba runs type `0x200`.
- **On a hit:**
  - the sound (`+0x3d7`);
  - slot rotation reset when airborne;
  - no downward push;
  - the position += push;
  - the turn through `0x452ed0`, clamped to ±0.07;
  - speed × `0x512a4c`;
  - TurboSpeed cancelled when the push's y < `0x512a48`;
  - `+0x157` and the normal on flags `0x600000`.
- A second block follows a car linked in `+0x2d1`:
  - it limits the speed to the other car's along the shared axis;
  - it sets `+0x89b` (10 for robots, 2 otherwise);
  - it adds ±6° of turn and halves the motion.
- Not ported.

## Port plan

1. Identify the five vector helpers and the per-mask cases.
2. Port the motion along the probe axis, the t² fall and the airtime.
3. Port the four-ray contact (the player) and the single-ray contact (robots).
4. Port the car-to-car query response.
5. Then remove the native guesses:
   - the 0.05 × 45 gravity;
   - the 40-unit climb cap;
   - the steep-ground stop (removed 2026-10-31, see the glide in
     [wall contacts](wall-contacts-20261029.md));
   - the fall respawn.
6. Re-measure the four courses. The current test calibrations rest on the native rules.

## Helpers and the contact cases (20261030, second pass)

- **The five helpers** (Ghidra export): `0x471ff0` = atan2(a, b), `0x472030` = vector length, `0x472070` = a − b, `0x4572a0` = a + b, `0x457310` = a × s.
- **Mask bits:** FL 1 (`0x46cf..`), FR 2, RL 4, RR 8. The hit points are at `0x6f12c4` + `0xa0` × probe (FL `0x6f12c4`, FR `0x6f1364`, RL `0x6f1404`, RR `0x6f14a4`). Robots use 20. Every angle below is atan2(Δy, horizontal distance):

  | Mask | Pitch `+0x267` | Roll `+0x26f` | Position |
  |---|---|---|---|
  | 1, 2, 4, 8 | | | kept (`+0x8ab`/`+0x8af` ±1, `+0x17f` = 1) |
  | 3 | | FL − FR | kept |
  | 5 | FL − RL | | kept |
  | 10 | FR − RR | | kept |
  | 12 | | RL − RR | kept |
  | 6 | | | midpoint of the diagonal |
  | 9 | | | midpoint of the diagonal |
  | 7 | FL − RL | FL − FR | midpoint of FR, RL |
  | 11 | FR − RR | FL − FR | midpoint of a diagonal |
  | 13 | FL − RL | RL − RR | midpoint of a diagonal |
  | 14 | FR − RR | RL − RR | midpoint of FR, RL |
  | 15 | FL − RL | RL − RR | midpoint of FR, RL |

  The three- and four-contact cases then pull the contact points toward their pair midpoints by the template spreads `+0x15b`/`+0x15f` (set at `0x469cb0`).
- **Constants:**
  - ray: from 80 above (`0x512a3c`) to `+0x93b` = 30 below (`0x46610b`); the car's up axis is × 1.3 (`0x512a40`) for the robots' ray;
  - robots sit 3 (`0x511894`) along the normal;
  - airborne: past 10 updates, the pitch and roll are × 0.85 (`0x512a38`);
  - the lift ray starts 200 above (`0x512a44`);
  - mode-2 probes: ±52 across, ± half the stretched span along.
- **The original return.** On surface `0x800000` (`+0x8bb`), after 40 updates (`+0x8e7` = `0x28`), `0x4a6730` gives the frame. The car is lifted by 50 (`0x512a5c`) and gets 25 updates of `+0x8e3`.

## First port attempt (reverted)

A port of the probe-axis step, the t² fall (9, and 18 for robots), the four rays (vertical, at the probes) and the cases above was tried and reverted:
- on `cc_t1` the test driver took 119.9 s (112.4 s before);
- on `sh_t1` neither the test driver nor any robot cleared the pyramid jumps: the launch along an 18° ramp is ~32 up per update against a fall of 9/36/81 (×2 for robots), and the cars landed in the water.

So something is still missing, for example:
- rays along the car's down axis rather than vertical, with the ×1.3;
- the exact probe-axis step;
- the slot forces at take-off.

**Next:** a Wine capture of a `sh_t1` jump (position per update), to check the trajectory against these formulas before porting again.

## Wine capture and the port (20261030, third pass)

**Method.**
- The original ran from a scratch copy of the prefix on Xvfb `:77`, with Wine's audio drivers disabled: practice mode, Dingo-Limousine, LES COLLINES JOYEUSES 1.
- A small MinGW probe (`wmem.exe`, scratchpad only) read and wrote the player car through `ReadProcessMemory`/`WriteProcessMemory`. The car is at `*(*0x6f2d00 + 0x38)`.
  - **Fields:** `+0x288` is the position (world coordinates), `+0x267/+0x26b/+0x26f` the angles, `+0x3ef` the speed and `+0x8a7` the airtime.
  - **The frame:** a 3×3 at `+0x2c`, with rows (cos y, 0, sin y), (0, 1, 0), (−sin y, 0, cos y), and its translation at `+0x54`; both are copied at `+0x98` and `+0xc0`.
  - **Placing the car** means writing all of these. It works only within the car's current section, so the car was driven across each portal.
- winedbg's attach killed the game and was not used again.

**What the capture shows** (from rest at (23495.31, 2951.10, −16018.58), heading 3.05, holding Up):
- The ramp climb follows the car's pitched axis: rise / run = 0.3375 = tan 0.3254.
- The first ramp sends the car ~30 updates into the air, peaking ~510 above the ground; the second ~40 updates, peaking ~900 above.
- The pitch holds for 10 updates in the air, then drops × 0.85 per update.
- The speed stops changing from airtime 7.
- The fall fits ≈ 0.043 t² per update: m_gravity 0.05 (vehicle 3) × the frame factor. `+0x98` is the template's m_gravity (property `0xef`, per vehicle 0.09, 0.09, 0.04, 0.05, 0.09, 0.07, 0.09); 9 is only the constructor default.
- About 25 updates per second under Wine.

**Native.**
- **`move`:** the step along `car_forward` (the heading in the plane normal to the up axis), and the fall min(t² × gravity, 100) × the frame factor.
- **`settle`:**
  - the player casts four probe rays (vertical), the robots one;
  - the mask cases set the pitch, roll and height;
  - the airtime and the × 0.85 decay;
  - `+0x8bb` from the last ray that hit;
  - drowning only with a contact.
- **Speed:** no throttle past airtime 6 (`0x466f91`).
- **Gravity:** `VehicleTuning::gravity` per vehicle, × 2 for robots, × `+0xb4` while a robot catches up.
- **Removed native rules:** the 0.05 × 45 gravity, the 40-unit climb carry and the 0.35 up-axis blend.

**Same start, native vs Wine:**

| | Wine | Native |
|---|---|---|
| Approach pitch | 0.325 | 0.325 |
| First ramp, take-off | z ≈ −19160 | z ≈ −19240 |
| First ramp, peak | y ≈ 3463 | y ≈ 3558 |
| First ramp, landing | z ≈ −22190 | z ≈ −22420 |
| Speed at the lip | 0.49 | 0.51 |
| Second ramp, pitch | 0.437 | 0.491 |
| Second ramp, peak | y ≈ 3849 | y ≈ 4009 |

The higher second jump is attributed to the vertical rays.

**Results** (`native_drive_test`): `sh_t1` 1 lap in 110.1 s with 0 respawns (178.8 s with 5 before); `cc_t1` 110.6 s; `dt_t1` 101.0 s; `ct_t1` 101.8 s. All robots lap all four courses.

## Car-to-car contacts ported (20261030)

- **The query.** `0x4a60c0`, mask `0x100`, runs after the fall. It returns the first other car whose sphere touches this one's: radius 100 each, the `0x482190` box test. A car whose WackyAutoBomba runs type `0x200` is skipped.
- **On a hit:**
  - the collision sound `+0x3cf` (30-update timer) and voice event 4 (manager `+0x2c`, not ported);
  - with d = this car's rear-to-front axis, when the other car is ahead (d · (there − here) > 0):
    - the robot action's `+0x2d1` = the other car (reader not traced);
    - the speed becomes min(speed, (d_other · d) × the other's speed), at least 0;
    - `+0x89b` = 10 for robots, 2 for the player (`0x46b643` reads the template's m_isrobotauto `+0xc0`), the follow lock below;
    - a ±6° (0.1047) yaw slot: + when the other car's unit RL − RR (`+0x423` − `+0x42f`, `0x46b662`) · d ≤ 0, else −. The port had the sign inverted until 2026-11-01;
    - the update's motion is halved;
  - always, a slot force (here − there) × 0.025 × 2.1 (`0x512a68`, `0x512a64`), type 3, decay 0.6 (`0x3f19999a`).

**Native.** `RaceSession::car_contacts`, in `move` before the slot sum. `Body::Push` carries the yaw step. Not compared with Wine.

**Effects:**
- the test driver now loses time in the pack: `cc_t1` 115.3 s, `sh_t1` 132.4 s, `dt_t1` 102.4 s, `ct_t1` 103.4 s;
- with car contacts, seed 0 (the test's) also strands a robot at `sh_t1`'s first hairpin, as seed 1 does without them (see TODO). The `sh_t1` check now asks one lap per robot.

## The follow lock `+0x89b` (2026-11-01)

**Set and count.**
- `0x46b63c..0x46b658`, only in the car-ahead branch (`0x46b548` skips it otherwise): `+0x89b` = 10 when the template's m_isrobotauto (`+0xc0`) is 1, else 2.
- `0x46b894`/`0x46b8a0`: with no contact, or with a hit car of type `0x200`, it drops by one while above 0.
- A contact from behind leaves it as it is.
- `0x469f10` clears it at setup.

**Readers, the speed update.**
- Robot branch, throttle 1:
  - below the limit (and `+0x157` clear), `0x466ee9` skips the convergence step;
  - at or above the limit, `0x466f35` skips the coast step. The clamp to the limit at `0x466f4f` then has nothing to do.
- Player branch, forward acceleration from speed ≥ 0: `0x4671f7` drops the step; `+0x257` = 0 still.
- The reverse, braking and coasting branches do not read it.

**Native.**
- `Body::follow_lock`, set in `car_contacts` and passed to `update_robot_speed` and `update_speed`.
- `native_drive_test` passes on cc_t1, sh_t1, dt_t1 and ct_t1. The test driver's sh_t1 lap went from 248.4 s to 99.3 s.
- The hit-hold check now allows 100 units of slope creep: at update 400, robot 0 is on cc_t1's bank at (−14516, 1123, 15418).
- Not compared with Wine.
