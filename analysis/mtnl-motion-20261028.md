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
