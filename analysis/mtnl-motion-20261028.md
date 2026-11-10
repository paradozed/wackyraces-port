# MTNL motion: channel mapping, playback and the menu preview pose

Recovered from the executable (static analysis of the installed `WPC.EXE`).
Native code: `src/port/game_motion.*`; tests: `tests/native_motion.cpp`.

## Library layout (`GDMotionLib`, loader `0x43fa60`)

After the resource id and the version `0x1310988`, the header has five words:

| Field | Runtime | Meaning |
|---|---|---|
| word 0 | `+0x18` | not used by the paths below (2, 48, 1268 in the menu) |
| word 1 | `+0x28` | clip count |
| word 2 | — | channel-index count (array `+0x20`) |
| word 3 | — | key count (array `+0x24`, 16 bytes each) |
| word 4 | `+0x2c` | segment count |

**Clips (20 serialized bytes, 24 at runtime):**

| Serialized | Runtime | Meaning |
|---|---|---|
| w0 | `+0x04` | controller: clip runs on controller w0 + 1 (`0x440900`) |
| w1 | `+0x08` | next clip when this one ends, −1 for none |
| w2 | `+0x14` | duration |
| w3 | `+0x0c` | channel count |
| w4 | `+0x10` | first entry in the channel-index array |

A key is a local time followed by x, y, z.

## Channel to segment

Each clip has 3 × segments channels, so in the menu libraries w3 is 21, 15 and 27 for 7, 5 and 9 segments.

**Controller update (`0x43feb0`):**
- Channel c starts at key `indices[w4 + c]`.
- Its three components accumulate into pairs 3c..3c+2 of the GDHModel's buffer (`+0x172`).
- That buffer holds nine pairs per segment (`0x437d00` allocates 0x48 bytes per segment).

**Pose (`0x440650`):** the nine pairs of segment s are read as follows.

| Pairs | Use |
|---|---|
| 0–2 | translation, added to the SEGS translation (`+0x18`) |
| 3–5 | rotations in degrees × `0.0174533` (`0x511518`): X `0x43b2b0`, Y `0x43b4a0`, Z `0x43b690` |
| 6–8 | scale: the basis rows × (1 + v) (`0x43b190`), applied first |

So channel c drives segment c / 3, kind c % 3 = translation, rotation, scale.

**Segment matrix:**
- Start from identity (`0x43b090`), then apply the scale, then X, Y and Z. Each helper mixes the matrix rows:

  | Helper | Axis | Rows mixed |
  |---|---|---|
  | `0x43b2b0` | X | 1 and 2 |
  | `0x43b4a0` | Y | 0 and 2 |
  | `0x43b690` | Z | 0 and 1 |

- Then set the translation, and multiply by the parent's matrix (`0x43c620`: this × parent, row vectors).
- Vertices are v × basis + translation (`0x43b900`).

**Corpus check (`native_motion_test`):**
- The segment count equals the linked GHMS's for every animated `GDHModel`:
  - `menu.WGM` and `SWMenu.WGM`: 6 models each;
  - `general.wgm` and `swgeneral.wgm`: 18 each;
  - seven disc courses.
- All 47 MTNL parse, and every clip plays to its end with finite poses.
- The scale channels are zero or uniform, for example (−0.206, −0.206, −0.206) in `0x7f8`.
- The rotation channels range over tens of degrees, and the translation channels over tens of units.

**Pair rule:**
- A controller keeps the largest positive value in a and the smallest non-positive one in b.
- `0x440650` resolves each slot to b, a, or a + b.
- a = −1 locks a slot. The overrides `0x437f70`, `0x4381a0` and `0x4383b0` set those locks in a template (`+0x176`, flag `+0x17a & 1`). They are not ported.

## Playback (`0x43fc80` controller)

**Starting a clip:**
- `0x440970(clip, offset, weight)` → `0x440240` sets the weight (`+0x20`), arms the clip (end = time − 1, start = time + offset), sets active (flag 1), and makes a zero step.

**Each step:**
- The step adds ftol(dt × speed) to the integer time, where speed `+0x24` = 1.
- The GDHModel draw (`0x437d00`) zeroes the buffer and calls every active controller with dt = 10, so time advances 10 per drawn frame.

**When the time passes the clip's end:**
- The next clip in the chain starts; at the end of the chain the controller goes inactive.
- Command 1 (flag 2) stops at the end instead; command 2 (flag 8) freezes on the last values.
- Each channel restarts one unit before the clip start.

**Interpolation:**
- When the local time passes the channel's next key time, the channel steps to the first key at or after it.
- It then interpolates linearly from its current value, at the previous update, to weight × key over that interval. So the first segment of a clip spans one extra unit.

**Port deviations:**
- `MTNL 0x7f9`'s second clip has duration −570, and its channels point at index 86, the key count. The port accepts this and reads no key there; the original would read past the array.
- The port also stops a channel at the library's last key.

## Controller 0 (`0x440e30`)

`0x4403f0` creates one controller per clip controller index, plus controller 0, a 0x140-byte object (vtable `0x51150c`). `0x437cb0` activates it. Controller 0 is procedural:

**Inputs:**
- It differentiates the model's world position twice: velocity `+0x128`, acceleration `+0x134`.
- It turns the acceleration into model space (`0x43ba80`).

**Filters:** seven direct-form biquads (`0x440b40`, coefficients from `0x440ab0`: 2r cos θ, −r², b0 = gain (1 − a1 − a2) × 0.2899, b1 = 1.8 b0, b2 = 0.65 b0; unit DC gain × gain).

| Value | Input | Clamp | Gain |
|---|---|---|---|
| translation | −acceleration | ±15 | 0.1, 0.8, 0.1 |
| rotation | (0, 1, 0.2) × −acceleration (`0x452ed0`) | ±15 | from `+0x17e` (not traced) |
| y scale | −0.01 × acceleration y | ±20 | 0.1 |

The same nine values go to every segment.

## Menu preview pose

No menu code starts a clip:
- The callers of `0x440970` and `0x4409f0` are:
  - the `GDMotionTest`/`Viewer` tools (`0x4418a3`, `0x441bda`, `0x442702`);
  - a test class registered as `Kosza` (`0x457db8`);
  - `WAnimViewer` (`0x47394b`, a keyboard-driven viewer registered at `0x472ed0`);
  - the power-up actions (`0x4d4f08`…).
- `menu.WGM` instantiates none of these classes (its classes and actions were listed).

So only controller 0 runs on the car-selection page:
- The car `GDHModel`s sit 2.34 (`0xb8`), 0.05 (`0xca`) and 0 (`0xea`) units from their `WRot` pivot.
- At 5° per frame, `0xb8`'s centripetal acceleration is 0.0178 units per frame², which gives at most about 0.015 units of translation.
- The rotation gains are not traced, but their input is of the same size.

**The original preview pose is therefore the static SEGS pose**, to within a few hundredths of a unit (inferred from the magnitudes above; controller 0 is not ported). That is what the port draws, and the earlier Wine card sheet matched all six cars (`menu-character-selection-reverse-20261027.md`). The animated cars are:

| Car | GDHModel | GHMS | MTNL | Segments |
|---|---|---|---|---|
| Tocard Tank | `0xb8` | `0x7b0` | `0x7f8` | 7 |
| Compact Pussycat | `0xca` | `0x7b3` | `0x7f9` | 5 |
| Caraverne | `0xea` | `0x7bd` | `0x7fa` | 9 |

`0xea` is identified as Caraverne from the debug capture; the other two follow from the serialized item order (Satanas, Dingo, Tocard, Compact, Bolide, Caraverne, Turbo).

## Native

- `parse_motion`, `MotionController` and `pose_mesh` follow the routines above.
- `load_mesh` keeps the GHMS segment table, each vertex's segment and its unposed position.
- `WPC_MENU_MOTION=N` (debug) plays clip N on the menu previews. Caraverne's clip 0 raises both brothers' clubs overhead, with the arms staying attached.
- Without the variable, the car-selection captures are pixel-identical to the previous build.

## Not verified

- No clip has been compared with Wine; the original plays none in the menu.
- The rotation signs and order are taken from the helpers. A Wine capture of a clip started by a power-up action (`0x4d4f08`…) would confirm them.
- The car names for `0xb8` and `0xca` are inferred from the row order.

## Controller 0 in the race (2026-11-08)

### Recovered

- **The constructor (`0x440be0`).** It takes the model's world position, with the velocity at 0 and
  the weight `+0x20` = 1. It configures the filters (`0x440d40`) with these gains:
  - translation (0.1, 0.8, 0.1);
  - rotation: the GDHModel's `+0x17e`. Its constructor `0x437720` sets (0.25, 0.25, 0.5), and the car
    init `0x469bf0` overwrites a body's by vehicle (jump table `0x46a294`, cases `0x469f87..0x46a126`,
    before the body's vtable `+0x4c` builds the controllers):

    | Vehicle | Gains | `+0x937` |
    |---|---|---|
    | 0 | (0.6, 0.25, 0.3) | 0.0489 |
    | 1 | (0.7, 0.25, 0.4) | 0.0436 |
    | 2 | (0.7, 0.25, 0.4) | 0.0524 |
    | 3 | (0.7, 0.25, 0.25) | 0.0559 |
    | 4 | (0.7, 0.25, 0.6) | 0.0524 |
    | 5 | (0.7, 0.25, 0.4) | 0.0471 |
    | 6 | (0.7, 0.25, 0.3) | 0.0506 |

    The same cases write the steer step `+0x937`, which matches the port's table for each vehicle.
    Read under Wine on vehicle 3's body: (0.7, 0.25, 0.25);
  - scale (0, 0.1, 0).
- **The filters (`0x440d40`).** Each is set by `0x440ab0(gain, alpha, omega)`:

  | Filter | Alpha | Omega |
  |---|---|---|
  | translation x | 0.2 | 0.35 |
  | translation y | 0.6 | 0.3 |
  | translation z | 0.3 | 0.3 |
  | rotation x | 0.7 | 0.4 |
  | rotation y | 0.9 | 0.5 |
  | rotation z | 0.9 | 0.4 |
  | scale y | 0.9 | 0.6 |

  - Coefficients: c0 = 2a cos w, c1 = −a², b0 = (1 − c0 − c1) × gain × 0.28985509,
    b1 = 1.8 b0, b2 = 0.65 b0.
  - The step (`0x440b40`): w = c1 s1 + c0 s0 + x, y = b1 s0 + b2 s1 + b0 w.
- **The update (`0x440e30`).**
  1. v = p − p', a = v − v'; with the model's flag `0x40000000` the velocity is kept.
  2. l = (x·a, y·a, z·a) with the frame's axes (`0x43ba80`).
  3. The inputs:
     - translation −l, clamped to ±15;
     - rotation (0, 1, 0.2) × l (`0x452ed0`: arg1 × arg2), clamped to ±15;
     - y scale −0.01 l.y, clamped to ±20.
  4. The nine values (t, r, 0, s, 0) × the weight go into every segment's pairs (`0x4410f1`): the
     largest positive in a, the smallest non-positive in b, locked slots left alone.
- **The time base.** `frame_time_factor` keeps at least two units (33 ms), so the original runs one
  update per drawn frame at 30 Hz. Stepping once per race update is equivalent to its per-draw step.

### Native

- `SwayController` (`game_motion`) implements the update.
- `RaceView::bodies_` holds each vehicle's first GDHModel: its transform in the group (composed as
  `WorldBuilder` does) and a mask of the group's other batches.
- `sways_` holds one posed instance per car, stepped once per race update with the matrix it is
  drawn at. The hit tint is applied to the posed copy.

### Checks

- At rest on the grid, the player's car is pixel-identical to the static rest pose (0 of 48000
  pixels differ).
- `native_drive_test`, synthetic: a steady drive settles; one lateral kick rolls by b0 × −15 on the
  next step, in every segment's b, then rings and settles.
- Scratch harness, robot 0 for 1200 updates, peaks of the values:

  | Course | Translation | Rotation (degrees) | Scale y | Updates with roll above 5° |
  |---|---|---|---|---|
  | `cc_t1` | (1.5, 7.2, 0.5) | (1.2, 1.9, 10.1) | 0.04 | 80 |
  | `dt_t1` | (1.5, 6.7, 0.9) | (1.7, 3.0, 9.5) | 0.03 | 69 |
  | `sh_t1` | (1.5, 8.1, 1.0) | (1.8, 1.5, 10.9) | 0.12 | 37 |
  | `ct_t2` | (1.5, 3.9, 0.9) | (2.2, 2.1, 10.2) | 0.02 | 67 |

- On screen (native), the player's tower leans in a turn.
- The breath head follows the posed segment 1 (`pose_mesh`'s segment matrices). It is unchanged
  at rest.
- **Compared with Wine (cc_t1, vehicle 3, the player driving with XTest keys).**
  - `wmem logsway` logs the body's world matrix (`+0x98`, `+0xc0`) and its six posed segment
    records (`[+0x15e] +0xc`, 0x34 bytes each: basis 9, one float, translation 3) every 4 ms.
  - The original drew 117 distinct frames in 6.3 s, about 18.6 frames a second under Xvfb.
  - Fed one frame at a time with the logged world matrices, `SwayController` with vehicle 3's gains
    reproduces segment 0's translation, basis and y scale to 10⁻³ for frames 0 to 91.
  - One sample (frame 75) straddles a frame. After a 1.3 s gap in the log (frame 92) the states
    differ, as the native saw one large jump.
  - With the constructor's gains (0.25, 0.25, 0.5) instead, the roll was 2.8 times smaller and the
    yaw twice larger; that is how the per-vehicle table was found.

## The gadgets' body animations (2026-11-09)

- **`0x4383b0(segment, rx, ry, rz, flag)`** (thunk `0x40275c`): flag −1 clears `+0x17a` bit 0;
  otherwise it sets the bit, allocates `+0x176` if needed, and locks the rotation slots 3..5 of the
  segment: with a flag, a = −1 and b = rx, ry, rz (degrees); with flag 0, a = b = 0. (The compiler
  reads the three arguments from the same stack slot between pops.) `0x437f70` does the same for the
  scale slots 6..8 (b = s − 1).
- **Callers on the car body** (`[car +0x24f]`), from FlyPower and TurboSpeed
  (`analysis/weapons-bomba-20261029.md`, `analysis/wfire-20261109.md`): FlyPower vehicles 0 and 1
  play clip 0 (`0x440970(0, 0, 1, −1)`; vehicle 1 first calls the vtable `+0x4c`); vehicle 2 locks
  segment 5's z rotation at 153 × `+0x64` degrees each update and clears the locks at its end;
  vehicle 5 locks segment 6's x rotation at 35° and clears them at its end. TurboSpeed: vehicle 1
  locks segment 1's scale at (1, 1, 1) (never cleared by its end); vehicle 4 plays clip 0 (MTNL
  `0x1034d`: clip 0 chains to clip 1, which loops) and clip 2 at the end.
- **Native:** `RaceView::body_gadgets` reads the session's gadget effect and FlyPower counter at each
  body step and drives the body instance's clip controller beside controller 0 (both accumulate into
  the same pairs, as `0x437d00` runs every active controller), then applies the locks.
- **Captures** (native, `cc_t1`, `WPC_GADGET`): vehicle 4's turbo swings both clubs; vehicle 1's
  FlyPower opens the umbrella over the car. Not compared with Wine.

