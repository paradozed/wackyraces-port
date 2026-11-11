# Car effects: WCarFx, dust, ripples and the drowning splash

Recovered from the executable (static analysis); native code in
`src/port/race_effects.*` (logic) and `RaceView::effects` (drawing).

## WCarFx and WCarFxStorage

`WCarFx` is the parameterless action on every `WackyAuto` template in `general.wgm` (registered at `0x6f23a0`, object size `0xe1c`, vtable `0x512e04`).

| Routine | Role |
|---|---|
| `0x47a420` (init) | clears the counter `+0xe10`, the tyre tracks (`0x47a500`, 4 wheels × 16 segments) and the 32 dust slots (`0x47bae0`), and sets per-vehicle shadow sizes (`0x47c070`) |
| `0x47a460` (update) | counter + 1; tyre tracks (`0x47a590`); the POUSSIERE puffs (`0x47b750`, Direct3D only and when the option `0x6f4650` is set); dust and ripples (`0x47bb10`); the shadow ray (`0x47c180`) |

`WCarFxStorage` (global `0x6f2360`) holds the templates:
- Its children list starts at `+0x60` and each child's next sibling is at `+0x14` (`0x41e740`). Each child's hierarchy template is at `+0x18`; `0x41fd00` (CreateHierarchy) clones that template.
- Children in order:

  | Child | Class | Content |
  |---|---|---|
  | 0 | `GD3DFilm` 410 | FILM `0x102e0` (24 frames): the POUSSIERE puffs |
  | 1 | `GD3DFilm` 411 | FILM `0x102e6`, 4 reels × 4 frames, 18×18, 8-bit: the dust |
  | 2 | `WSplash` 412 | GDModels 414/415 (both `GMDL 0x102f4`, draw flags `0x88062` additive, GMAT `0x101b9`, 32-bit, TRAN 0.35) and GDSound 416 (`GSMP 0x1033d`) |
  | 3 | `WKBumm` 417 | a `GD3DFilm` subclass, FILM `0x102b4` (7 frames), GDSound 418 (`GSMP 0x1033e`) |
  | 4 | `WalkOnWater` 419 | a GDModel, `GMDL 0x102f9`: a 50-unit quad, additive, GMAT `0x101bf`, 32-bit, TRAN 0.49 |

- Clones are re-parented under the world (`0x6f2c40`) and take the car's player id (`+0x104`).

## Wheels

The template's `m_monstertires11/12/21/22` (`0x11c..0x11f`) are matrices whose translations put wheels 0–3 at (−x, 0, z), (x, 0, z), (−x, 0, −z) and (x, 0, −z) in the actor frame (X right, Z forward). In general.wgm, by `m_iVehicle` (`0xed`), (x, z) is:
- 0: (80, 60); 1: (80, 60); 2: (45, 60); 3: (60, 60);
- 4: (70, 60); 5: (88, 66); 6: (60, 60).

Earlier notes read vehicle 4's template, the first in the file, as the rule for every car. `vehicle_wheels` (2026-11-01) uses each car's own values.

`0x46d8xx` probes each wheel with the ground mask `0x018000ff`:
- the contact position goes to `+0x43b + 0x14w`;
- the hit's masked flags go to `+0x447 + 0x14w`;
- the x component is `0x42f6e979` when there is no contact.

Natively the four contacts come from `RaceTrack::ground` at the tyre offsets, only while the car is grounded, and are frozen while it drowns.

## Dust (`0x47bb10`, drawn by `0x47bec0`)

There are 32 slots of 0x20 bytes at `+0xa28`: age, frame, surface, offset and velocity.

**Spawning:**
- One free slot at most per update. Speed > 0.3 uses rear wheel (r & 1) + 2; speed < −0.1 uses front wheel r & 1. Between those speeds nothing spawns.
- Each free slot visited while nothing has spawned draws a new random number.
- The wheel needs a contact on surface `0x20`, `0x80` (reel 0), `0x800000` (reel 1), `4` (reel 2) or `0x10` (reel 3). Road type 2 and bank type 1 give none.
- Offset = contact − car position.
- Velocity = ±X axis × (3u + 2): left wheels go along −X, right wheels along +X. Then y = 2u + 2, from a single u.
- Frame = r & 3.

**Life:** age 1 to 23. Each update the offset adds the velocity once (`0x5299b4` / 2 = 1) and y velocity loses 0.3.

**Drawing (`0x47bec0`):**
- The puff is drawn at the car's current position plus its offset, using child 1 with scale `+0x152` = 0.3, reel `+0x15a` and frame `+0x162`.
- Its draw (`0x42d090`) queues the film in the renderer's `+0x234fd` list (`0x4b50e0`):
  - nearest first by squared distance to the camera, at most 200;
  - flushed in reverse after the translucent lists, with `+0x1b6` set.
- The second pass (`0x42d1e3` on) builds a quad in the camera's basis:
  - corners at ftol(6 × scale × the frame's x offset) and that minus ftol(−6 × scale × size);
  - the x offset is used for both axes.
- A dust frame (−9, 18×18) therefore gives a 32-unit square.

**Random numbers:** effects draw from their own MSVC stream (`0x430ef0`, state `0x5472c4`; `0x430f30` = r / 32768). `0x430e20` gives it the same seed as `rand()` (`0x5472c8`), so effects never perturb the power-up draws.

## Ripples (`0x47c680`, WalkOnWater `0x47c790`/`0x47c7d0`)

**Spawning:** each update draws r & 1, a front wheel. On even counter values, if that wheel is in contact with surface `0x800000`, `WalkOnWater` is cloned with the car's frame translated to that contact.

**Life:** the clone's scale starts at 1 and grows by 0.4 (`0x512a10`) per update for 15 updates, applied to its basis rows (`0x43b190`). Then it destroys itself.

## Splash (`WSplash`, `0x472d00` from `0x470052`, update `0x473df0`)

**Spawning:** the car update clones `WSplash` with the car's matrix when its surface state `+0x8bb` first becomes `0x800000`. The `+0x385` test before that call is not explained. Natively the splash starts with the drowning.

**Setup:** the clone's init (`0x473d80`) plays its sound child and sets the state to 0. State 0 sets:

| Field | Start |
|---|---|
| h | 0 |
| a, b | 0.1 |
| rise | 0.25 |
| grow a | 0.05 |
| grow b | 0.07 |
| `+0x13a` | 200 (counts down by 3 and is not read again) |

**States:**

| State | Behaviour |
|---|---|
| 1–10 | b += grow b |
| 11 | h += rise; a and b grow; rise −0.03 while positive, else −0.01; ends when h < 0 |
| 12–30 | a and b grow |
| 31 | stops the sound and destroys the clone |

After each state, grow a and grow b lose 0.0015 (`0x512b80`).

**Models:** the first model is set to diag(0.2a, 0.2h, 0.2a) and the second to diag(0.2b, 0.002, 0.2b) under the splash. The model is 2221 units across and 1226 high at scale 1. Natively the column peaks once at a height factor between 1 and 2, and the clone lives about 50 updates (`native_drive_test`).

## WKBumm (`0x472dc0`, init `0x486960`, update `0x486b20`, draw `0x486c40`)

**Trigger.** `WGndBrick` nodes carry `WPrimCylinder` primitives (members `0x1fa` base centre, `0x1fb` r, `0x1fc` yheight, `0x88` `m_iObj`). The groups:

| Sensor group | Where | Objects |
|---|---|---|
| `0x40000` | `cc_t1` only, 57 | the cacti; `m_iObj >> 16` is the cactus `WGndSprite`, for example 73 |
| `0x10000` | `cc_t1` 31, `dt_t1` 53 | rocks and start lines |

`0x482310` tests the car's sphere against a cylinder:
- the X/Z boxes overlap;
- the sphere's vertical extent meets [base, base + height];
- the integer X/Z squares are at most `int(rs + rc)²`.

**Effects of an `0x40000` hit:**
- The object query (`0x4a5bd6`) sets flag `0x100` on the linked object, so the cactus is no longer drawn, and clears the `0x40000` bit of the primitive's group. Each cactus breaks once.
- The car update (`0x46aa58`) clones `WKBumm` with its frame, copies its speed into `+0x2d4`, and plays a car sound (`+0x3cf`) unless its 30-update timer `+0x3e7` runs. The car is not slowed.

**The clone:**
- **Init:** it plays its GDSound (`GSMP 0x1033e`) and sets scale `+0x152` = 0.2. Each of 8 pieces i (effects stream) gets:

  | Field | Value |
  |---|---|
  | position | (0, 100u + 1, 0) |
  | velocity | x = sin(45i°) × (8u + 3), y = 18u + 20, z = speed × 100 + cos(45i°) × (8u + 3) |
  | spin | (7i − 30)° per update |
  | frame | rand() % 5 of FILM `0x102b4` |

- **Update:** a piece below local height 1 is hidden; otherwise it moves, loses 2 of upward speed and turns. Once every piece is down, the clone waits for its sound and ends.
- **Draw:** each live piece goes through the film draw at its local position in the clone's frame, with its angle `+0x14a`.

## POUSSIERE (`0x47b750`, draw `0x47b9d0`)

**The option.** Menu item 588 (`WMenuSetValueItem` type 11, `0x48a7ea`) toggles `0x6f4650`, a BSS word that is off by default, between INACTIVE and ACTIVE (strings `0x9a`/`0x9b`).

**Spawning:** 24 slots of 7 floats at `+0x764`.
- Each update only the first free slot is tried, above speed ±0.25.
- It uses rear wheel 3 on even counter values and wheel 2 on odd ones, with any contact.
- Velocity = (rand() % 20 − 10) × up + (rand() % 20 − 10) × right.
- Life: 23 updates.

**Draw:** two slots in three, with child 0 (`GD3DFilm` 410):

| Property | Value |
|---|---|
| frame | age − 1 |
| colour | (160, 128, 0) |
| alpha | (24 − age) × 3 |
| scale | 0.05 × age + 0.5 |

## The film draw's camera term (`0x42d2b4`)

When the frame material's `SMOO` (GMAT `+0x68`, chunk `0x4f4f4d53`) is 0, the quad's centre moves a fifth of the way toward the camera. The camera's rotation rows form the billboard basis. SMOO is 0 on the dust (`0x10196`…) and the WKBumm frames (`0x10144`…), and 1 on the POUSSIERE frames (`0x10168`…).

## Shadow (`0x47c180`)

**The ray** goes from 403.7 above the car to 1000 below:
- The mask is `0x818000ff`, or only `0x80000000` in the robots' far mode, where the shadow also gets flag `0x100`.
- The same query fills the car's route module.

**Placement.** When ground is hit below the car + 50 (f is the hit's fraction of the ray):
- right = n × forward and forward = right × n;
- right and forward are scaled by (1 − f) times the vehicle's sizes from `0x47c070`:

  | Vehicle | 0 | 1 | 2 | 3 | 4 | 5 | 6 |
  |---|---|---|---|---|---|---|---|
  | Sizes | 1.5, 0.7 | 1, 1 | 3.5, 1.5 | 3.5, 1.5 | 2, 2 | 2, 1 | 5, 2 |

- position = the hit + 5n.
- The model is `GMDL 0x102dc`, a 93 × 149 quad.

## Tyre tracks (`0x47a590`, draw `0x47a7c0`)

**Storage.** `0x47a500` (init) gives each wheel 16 segments of 0x1d bytes at `+0x24` (stride 0x1d0 per wheel):

| Offset | Content |
|---|---|
| `+0x00` | point A, three ints |
| `+0x0c` | point B, three ints |
| `+0x18` | active |
| `+0x19` | shade, 0x30 or 0x5c |
| `+0x1a` | fade step, 6 |
| `+0x1c` | fade, 100 |

**Update (`0x47a590`), per wheel:**
1. From segment 14 up to 0, an active segment is copied one down and the copy's fade loses 6; an inactive one clears the next.
2. Segment 0 comes from the wheel contact (`+0x43b + 0x14w`):
   - no hit (123.456): inactive;
   - surface bit 4 or 0xa0: shade 0x30;
   - surface bit 2 while `+0x25b & 1`: shade 0x5c;
   - otherwise inactive.
3. A = ftol(contact − w × X), B = ftol(contact + w × X), where X is the car's X axis (`0x405065`) and w is `+0x44b`, the vehicle's half width from the table `0x52b804` (24, 8, 6, 6, 7, 16, 8 for vehicles 0..6; `0x4d64e6` sets 0x18 during an item effect).

**`+0x25b`** = `+0x25f || +0x263` (`0x47060f`).

`+0x25f` (brake skid) comes from the speed code (`0x4670c3..0x4673d8`). Accelerate means the key or robot throttle 1; brake means the key or throttle 2.
- **Accelerate:** `+0x158` − 1; below 0 it is set to 0 and `+0x25f` is cleared.
- **Brake:**
  - in state `+0x257` = 1, `+0x25f` = 0 and the speed is 0;
  - `+0x158` = 0;
  - in state 0, with |speed| > 0.015 (`0x5129c8`) and `+0x8a7` < 5 (updates in the air), `+0x25f` = 1 and `+0x158` = 1.
- **Neither key:** both are cleared.
- So a brake skid lasts one update after the release while accelerating.

`+0x263` (slide) is set in `0x46ae42..0x46b0cc`, while `+0x883` is 0 or 1.
- x = speed × `+0x8cb` × |steer `+0x403`|, and 0 during item effect 0 (`+0x385`).
- **Start:** x > `m_farolspeedlimit` (`[template+0xa0]`, member 0xf0), with `+0x8a7` < 4 and `m_faroltime`'s slot (`+0xb8`) at 0. Then `+0x263` = 1, `+0x883` = 1, the slot = 3, and `m_farolstartbr`'s slot = 0. The slide physics are not ported.
- **Otherwise:** the slot counts down to 0, which resets `m_farolstartbr`'s slot to 1.0, and `+0x883` = 0. The slide ends (`+0x263` = 0) once x < 0.6 (`0x512a70`) × the limit and `+0x158` is 0.

`general.wgm` values:
- `m_farolspeedlimit`: 0.1, 0.1, 0.09, 0.095, 0.1, 0.13, 0.085 for vehicles 0..6;
- `m_faroltime`: 1;
- `m_farolbendmod`: 13, 13, 13, 13, 7, 8, 8;
- `m_farolstartbr`: 0.8.

`+0x8cb` is set by surface `+0x8bb` (`0x470218`):

| Surface | `+0x8cb` |
|---|---|
| 2 | 1 |
| 4 | `m_homokfrictionmod` |
| 8 | `m_foldutfrictionmod` |
| 0x10 | `m_fufrictionmod` |
| 0x20 | `m_hofrictionmod` |
| 0x80 | 1.5 |
| 0x40 and others | unchanged |

The four `m_*frictionmod` members are all 1. `m_jegfrictionmod` (0.5) is not used here.

**Native:**
- `SkidState`, `update_brake_skid` and `update_slide` in `game_drive`;
- the player and the physical robots update them after their speed;
- `RaceSession::skidding()`.

### Slide physics (20261029)

`+0xc0` on the template is `m_isrobotauto` (member `0xdb`, value at `0xbc + 4`), not an input channel. It is 0 in `general.wgm`; `0x49d580` sets it to 1 on every racer in `0x6f2ce0` but the player. The earlier robot note called it the input channel.

What a slide start does (`0x46aec3..0x46b084`), on the update it starts:
- **Yaw.** The update's yaw step (`[esp+0x7c]`, later × `+0x163` at `0x46b8f9`) is multiplied by x × `m_farolbendmod` (template `+0xa8`: 13, 13, 13, 13, 7, 8, 8).
- **Push, player only.** The force is the diagonal between two movement probes × x × row[4] of `0x6f2990` × 1.2 (`0x512a74`). Steering right (`+0x403` < 0) uses FL `+0x40b` − RR `+0x42f`, otherwise FR `+0x417` − RL `+0x423`, so the car is thrown outward and forward. The probes come from `0x468280` mode 2: ±52 across, ± the stretched +102 along.
  - row[4] by vehicle: 1.5, 1.0, 1.2, 1.1, 0.2, 0.3, 0.7.
  - It goes into a `+0x48b` slot (`0x4726c0`): flags 1, force decay `+0x18` = 0.8. The slot also carries `+0x10` = x × bendmod × yaw and `+0x1c` = 0.95, but without flag 2 they are never applied.
- **Robots** (`m_isrobotauto` = 1, `0x46b058`) skip the push. They put the timer back to 0 and `m_farolstartbr` to 1, so they restart (with the yaw kick) on every update with x above the limit.
- **`+0x883` = 1** for one update. The next speed update (`0x466db0`, keyboard branch only) scales the forward acceleration by 0.2 (`0x4671f1`, `0x51041c`) and the forward braking step by 0.1 (`0x467334`, `0x511624`). The robot branch (`0x466e17`) does not read it.

Related, from the same code:
- **Robot yaw.** Robots skip the |row[2] − speed| × 0.8 factor of the yaw step (`0x46ad04` tests `m_isrobotauto`): their yaw is clamp(steer × 0.7, ±5°) × `+0x8c7` × `+0x163`.
- **Unused members.** `m_*farolmaxmod`, `m_*faroldeltamod` and `m_*farolsidemod` are copied by surface into `+0x8cf`, `+0x8d3` and `+0x8d7` (`0x470218`), and nothing reads those slots. `m_farolstartbr`'s slot (`+0xb0`) is written but never read here.
- **Surface slots.** The surface switch also sets `+0x8bf` (acceleration), `+0x8c3` (top speed) and `+0x8c7` (yaw) from the `0x6f2990` rows. Ported on 20261029; see the next section.

### Surface modifiers (20261029)

`0x470218` runs in the ground contact on surface `+0x8bb`. It sets the slots from rows 6..23 of the vehicle's `0x52b558` row, copied to `0x6f2990` by `0x483a20` and never rewritten for the player. Each surface reads a block of three rows:

| Surface | `+0x8c3` top | `+0x8bf` accel | `+0x8c7` yaw | `+0x8cb` friction | Vehicle 0 (top, accel, yaw) |
|---|---|---|---|---|---|
| 2 (road) | 1 | 1 | 1 | 1 | 1, 1, 1 |
| 4 (sand) | row 6 | row 7 | row 8 | `m_homokfrictionmod` (1) | 0.85, 0.9, 0.8 |
| 0x10 (grass) | row 9 | row 10 | row 11 | `m_fufrictionmod` (1) | 0.85, 0.8, 0.9 |
| 0x20 (snow) | row 12 | row 13 | row 14 | `m_hofrictionmod` (1) | 0.85, 0.8, 0.8 |
| 8 (dirt) | row 15 | row 16 | row 17 | `m_foldutfrictionmod` (1) | 0.9, 0.9, 0.9 |
| 0x80 | row 18 | row 19 | row 20 | 1.5 | 1, 0.7, 1.1 |
| 0x40 (ice) | row 21 | row 22 | row 23 | unchanged | 0.9, 0.9, 0.9 |
| others | unchanged | unchanged | unchanged | unchanged | |

- The slots are 1 at construction (`0x469d70`).
- On 0x80, `+0x8cf`..`+0x8d7` are set to 1.3, and nothing reads them.
- The template's `m_*bendmod`, `m_*accelmod` and `m_*maxspeedmod` members are not used here; the rows differ from them (for example `m_hobendmod` is 0.6, row 14 is 0.8).
- `0x4e4fd7` rewrites rows 0 and 4 for robots (rank scaling, `scale_by_rank`).

Where the slots apply, in the speed update `0x466db0`:
- **Keyboard:** the limit is row[0] × `+0x8c3` (`0x467127`). The convergence still aims at the unscaled max(row[0], 0.8) and is × `+0x8bf`. The reverse step 0.023, the braking step, the reverse convergence and the −0.4 × `+0x8c3` reverse limit are also × `+0x8bf`.
- **Robots:** the target is row[0] × `+0x8c3`, at least 0.8 (`0x466eb2`). Each step is × `+0x8bf`, with the reverse limit −0.4 × `+0x8c3`.
- **Coasting** (0.0096) is not scaled.
- **Yaw:** × `+0x8c7` for both (`0x46ad5d`).
- `+0x8db`: see the next section.

### Recovery boost `+0x8db` (20261029)

- **Effect.** `0x466df6` runs first in the speed update of every car, before the player/robot test. While `+0x8db` ≠ 0, it doubles `+0x8bf` and decrements `+0x8db`. `0x470218` rewrites `+0x8bf` at each contact on a known surface, so the doubling (×2 on that update's acceleration and braking steps) only compounds on other surfaces.
- **Who sets it.** Only the `WackyAutoBomba` action (factory `0x47c950`, size `0x8e`, vtable `0x512fd8`, update `0x47e220`, registered at `0x47c900` with name `0x52b388`). Each of its branches (type `+0x2c`: `0x40`, `0x80`, `0x100`, `0x200`, `0x400`, …) holds the car for 25 updates, with speed `+0x3ef` = 0 and `+0x3eb` = −5. On the 25th (`+0x82` = 0x19) it sets `+0x8db` = 20 (`0x47ea25`, `0x47eb89`, `0x47ed3c`, `0x47ee81`, `0x47efa8`, `0x47f1ce`). So a bombed car pulls away with the doubled acceleration for 20 updates.
- **Reset.** `0x468820` is WackyAuto's vtable `0x51295c` slot 4 (`+0x10`, entry `0x51296c`), the activation hook that `0x41ba60` (thunk `0x404edf`) calls when an object without flag `0x1000` is activated (flags \|= `0x1002`); the base class's slot 4 is a bare `ret` (`0x4115d0`). It clears `+0x8db`, `+0x2f5..+0x305` and sets `+0x35d` = −1. No activation of the car object itself was found in the car, WackyAutoBomba or race-manager code (their `0x404edf` calls target `+0x369`, pieces or the actions), so during a race `+0x8db` only runs out by its countdown (2026-10-29 reading; the other `0x404edf` callers are not all checked).

Native:
- `update_boost` in `game_drive`, called for every physical car after `update_grip`, with `Body::boost`;
- `RaceSession::start_boost(car, 20)`. `WackyAutoBomba` and the weapons that clone it are not ported, so nothing calls this outside the tests;
- no reset on respawn (not traced).

Native:
- `SurfaceGrip` and `update_grip` in `game_drive`;
- `Body::grip` is updated from the previous contact's surface, while grounded, before the speed update;
- `update_speed`, `update_robot_speed`, `yaw_step` and `update_slide` take it.

Native:
- `SkidState::kick` and `started`;
- `update_slide(..., robot)`, `update_speed(..., slide_kick)`, `yaw_step(..., robot)`, `farol_bend_mod`, `slide_push`;
- `RaceSession::move` applies the kick and the player's push (`Body::Push` now carries its decay).

Provisional:
- **Robots' yaw kick** (20261029). It is now on, as in WPC: robots restart every update above the limit. Enabling it first sent `dt_t1`'s robots into orbits at section 0's exit polygon. The cause was a route bug, fixed in `race_robot` (`analysis/robocar-ai-20261028.md`): a link to `0xffff` must give goal 1 its point. With the kick, the robots on `sh_t1` get stuck more often against the tunnel walls (the provisional wall response); there the best robot does 1 lap in 120 s, down from 2.
- the probe diagonal is rebuilt from the car's current yaw and speed rather than the stored probes;
- the item-effect case (`+0x385` = 0 forces x = 0) is not handled;
- `+0x8a7` < 4 is taken as grounded.

Results of `native_drive_test` on the four courses (before the slide → with the slide → with the surface modifiers too; all checks pass):

| Course | Player (test driver) | Robots in 120 s |
|---|---|---|
| `cc_t1` | 121.5 → 108.4 → 112.3 s | 3 → 3 → 3 laps |
| `sh_t1` | 207.9 s, 2 respawns → 237.1 s, 5 → 232.3 s, 8 | 1 → 1 → 2 laps |
| `dt_t1` | 163.1 → 149.7 → 154.2 s | 2 → 3 → 3 laps |
| `ct_t1` | 122.2 → 117.1 → 120.5 s | 3 → 3 → 3 laps |

The test driver steers hard, so slide pushes now throw it wide on `sh_t1`'s bends.

Not compared with Wine.

**Draw (`0x47a7c0`):**
- When wheel 0's segment 0 lies more than 500 along +X, +Y or +Z (a positive difference only) from its segment 1, segments 1..15 of every wheel are cleared and nothing is drawn (`0x47a974`, `0x47b320`).
- The Direct3D path (`0x6f5958` = 2, renderer `+0xf0`) builds, for each pair of active segments, the triangles (A_i, B_i, A_i+1) and (A_i+1, B_i, B_i+1). Its vertices are 0x28 bytes, with colour (fade, shade, shade, shade) packed by `0x4b5550` as ARGB, FVF 0x42 (XYZ | DIFFUSE), untextured.
- Mode 0x18: bit 0x10 culls none, bit 0x08 keeps the source-alpha blend, and depth writes are off. The fade 100…10 is therefore alpha /255.
- The software path (`0x6f5958` = 0) projects the points itself and lifts them by 5 (`0x47aad6`).

**Native:** `CarEffects::update_tracks`, and `RaceView::effects` draws them after the cars, before the translucent queue, alpha blended without depth writes. Lifting the points by 5, taken from the software path, is provisional.

## Solid contacts (`0x10000`, `0x46a797`)

**Query.** The car's object query (`0x4a5730`, mask `0x79600`, `0x46a768`) runs at the start of its physical update. On `cc_t1` and `dt_t1` the `0x10000` cylinders are `WGndSprite` scenery: rocks, posts and lamps, none at the start line.

**Normal (`0x482403`).** (0, −1, 0) below the cylinder's base, (0, 1, 0) above its top, otherwise the unit horizontal vector from the axis to the car.

**On the ground (`+0x15a` clear):**
- s = |speed|; above 0.2, s × |n · m̂|, where m is the last update's motion (`+0x40b` − `+0x423`). s is at most 0.5.
- A force n × (25 + 270 s), its up part made positive, goes into a `+0x48b` slot (`0x4726c0`, 25 slots of 0x28 bytes, flags 1, decay 0.4).
- `0x4727b0` adds the force to the update's motion; `0x472890` scales it by 0.4 and drops it once its squared length is below 2.
- The speed becomes 0. A running item effect 8 is cancelled.

**In the air:** speed × 0.8, and the hit object gets flag 1.

**Sound:** `+0x3d3`, unless the 30-update timer `+0x3e7` runs. The cactus branch plays `+0x3cf` the same way. `+0x183c` → `+0x2c(vehicle, 5, 0xff)` is not identified.

**Native:** `RaceSession::solid_contacts` at the start of `move`. The pushes go through the wall sweep. The order of the push and its decay within an update is provisional.

## Options (`0x48a660`)

`WMenuSetValueItem` accept callback: the type indexes the byte table `0x48aa1c` and then the jump table `0x48aa04`. After a change, `0x405f10` sets the string (`+0x130`) of the item's second child.

**POUSSIERE.** Item 588, type 11, `0x6f4650` (BSS, off). It toggles between strings 0x9a INACTIVE and 0x9b ACTIVE.

**METEO.** Item 146, type 8, `0x6f464c`. It cycles 0, 1, 2 with strings 0x0a INACTIVE, 0x0b ACTIVE and 0xfd ALEATOIRE. The profile defaults (`0x45b990`) copy `0x52aae8` = 2, and Wine's first options page shows ALEATOIRE.

At course load, `0x432c7f` clears the rain flag `0x2a1cc74` and then sets it:
- for METEO 1;
- for METEO 2, when rand() % 100 > 25.

It only does so when the course name contains (strstr) `cc_t1`…`cc_t4`, `dr_t1`, `ct_`, `dr_t4` or `dr_b`. The rain draw (`0x4b3315`/`0x40505b`, `0x4565e0`, `0x4ae364`, `0x4b08b1`, `0x4b4327`) is ported in part (2026-11-01 section below).

**Native:**
- `MenuController` toggles both and shows the values through `value_text`.
- `start_race` passes them to `RaceSession::set_dust_option` and `set_weather`.
- `[game] dust` sets POUSSIERE's starting value.

## Every car

Every WackyAuto runs WCarFx.
- **Shared stream.** All the cars draw from the one effects stream `0x5472c4`: the session swaps the state through each car's `CarEffects` in turn, the player first and then the robots in grid order. This order is provisional.
- **Far robots** skip their physics and therefore their wheel contacts and solid contacts (provisional).
- **Cacti.** Robots break cacti too.
- **Sounds.** Only the player's sounds play.

## Native

- `CarEffects` runs the logic above once per game update, from `RaceSession::update`:
  - the splash flag is the update on which the drowning starts;
  - clones created in an update start updating on the next one (provisional: the original object order is not traced).
- `RaceView` loads:
  - the dust reels from child 1's FILM;
  - the splash models from `WSplash`'s two GDModels;
  - the ripple from `WalkOnWater`.
- The splash models and ripples go through the translucent queue, so their `0x88062` flags blend them additively.
- The puffs are drawn after the queue, far to near and at most 200, as two-sided textured quads in the camera's right/up axes.
- The `0.2 × position` term at `0x42d2b4` applies to frames whose material is not SMOO (`Glyph::smooth`).

## Wine comparison (20261029, `cc_t1`, scratch prefix)

- **Options page:** METEO ALEATOIRE and POUSSIERE INACTIVE at first; one Enter toggles POUSSIERE to ACTIVE. I set METEO to INACTIVE for the runs.
- **Tyre tracks:** right after PARTEZ!, grey curved tracks show on the sand behind the robots.
- **POUSSIERE:** faint yellowish puffs behind the rear wheels, near the limit of what the captures resolve.
- **Shadow:** a translucent silhouette of the car on the bank's sand, directly behind the car. Natively it is the same silhouette, but drawn further from the car while it is airborne over the bank.
- **WKBumm:** not compared. Key-scripted steering did not reach a cactus.
- **Bank:** the original car still enters the river before sinking.

## Verification (WKBumm, POUSSIERE, shadow)

- **`native_drive_test`:**
  - WKBumm's eight pieces (piece 0 straight ahead with speed × 100, up 20..38) all land within 60 updates;
  - POUSSIERE is off by default; when on, it alternates rear wheels 2 and 3.
- **In the game** (`cc_t1`, placed in front of cactus 73), each cactus driven through bursts into spinning pieces once and disappears, with its sound.

## Verification

- `native_drive_test`'s synthetic checks (38 in all) cover:
  - one puff per update from a rear wheel;
  - the reel by surface;
  - the outward and upward velocity;
  - integration and gravity;
  - the 23-update life;
  - no dust on road type 2;
  - ripples on even updates only, growing to 7 and ending after 15 updates;
  - the splash lifetime and peak.
- The four-course drive still completes its laps.
- **In the game:**
  - Driving on `ct_t1`'s snow throws speckled puffs behind both rear wheels.
  - Driving straight into `cc_t1`'s river shows the column rising and falling inside a spreading additive ring before the car returns.
- No ripple frame was looked at closely.
- **Wine comparison (`cc_t1`, holding Up into the river).** The original was run from the scratch prefix's own copy of the install, on Xvfb `:77`.
  - The original shows WalkOnWater ripples, then a thin column and a flat circular ring.
  - Its car rolls into the river before sinking; the native car drowns on the bank (the provisional drowning trigger). The native splash had taken the car's tilted frame on the slope.
  - The splash now uses the car's heading with a level up axis (provisional).
  - Dust on the road was absent in both, as the surface table says.
  - The shadow, the WKBumm pieces and POUSSIERE were not compared.

## Rain and snow (2026-11-01)

**Course choice** (`0x432bdc..0x432cc2`, course load).
- The course name is searched (strstr) for cc_t1..cc_t4 and dr_t1. A match keeps `0x2a1b2b0` = 0, the rain.
- Otherwise `0x2a1b2b0` = 1, the snow, and the flag `0x2a1cc74` needs ct_, dr_t4 or dr_b (the last strstr result).
- So sh and dt courses never get weather.
- METEO decides as before: 1 is always on, and 2 is on when `rand() % 100 > 25`.

**Drawing.** The draw `0x4b32f9` runs while the flag and `0x2a1cc70` are set. `0x4565e0` clears `0x2a1cc70` when `0x456180` finds the viewport camera (`app+0x1838` → `+0xbc` → `+0x186`) inside a `Zartter` box (traced 2026-11-02, below). It then calls rain `0x405c45` → `0x4b6190` or snow `0x40505b` → `0x4b6510`.

**Rain `0x4b6190`**, every frame:
- 2500 (`0x9c4`) line segments (`DrawPrimitiveUP`, line list, FVF `0x42`), with WORLD set to the camera's frame (`0x4200e0`, `0x43af80`);
- z = unit × 10000 (`0x430fc0`), y and x = `rand2(8000)` (`0x431010`: unit × 2a − a);
- grey c = `rand() % 100 + 50`;
- the top vertex has alpha 0; the bottom vertex is 500 (`0x514bb4`) lower and 30 (`0x512a60`) toward −x, with alpha = (−(⌊z⌋ / 8 + 1)) & 0xff;
- render states: fog off, alpha blend SRCALPHA/INVSRCALPHA, z-write off, no texture, cull none.

**Snow `0x4b6510`.** 1500 flakes at `0x2690c40`, 0x120 bytes each (six vertices of 0x30 bytes, then the state). Each frame, for each flake:
- an unset flake gets a velocity (`rand2(10)`, unit × 2 + 2, `rand2(10)`), an offset (`rand2(3000)`, `rand2(550)`, `rand2(3000)`) and a counter of 8;
- with `rand() % 100 > 90` and the counter > 0, a new velocity, and the counter drops by one;
- offset += velocity (y −=);
- below −150 (`0x514db0`) it respawns: offset (`rand2(3000)`, unit × 250 + 300 (`0x512610`), `rand2(3000)`) and counter 8;
- then camera position + offset goes through `0x4029b4` into view space, as a 10-unit (`0x510254`) quad with UVs (0, 0)–(1, 1). WORLD and VIEW are identity; it is drawn as a strided triangle list, FVF `0x102`, with texture renderer `+0x23969` → `+0x18`.

**Native.**
- `RaceSession::snowing()` reflects the course choice.
- `RaceView::weather` draws after the scene, before the HUD: rain as about-1-pixel strips, snow as plain white squares (since 2026-11-09; before, discs on a native 16 × 16 texture).
- **The flake texture (2026-11-09).** `0x4b6840` reads renderer `+0x23969`; no instruction writes that field (renderer 1 is the zeroed global `0x26fae78`; the nearby queue at `+0x2396d` is a separate array), so it stays null and `0x4b6888` calls SetTexture(0, NULL), then FOGENABLE 0 (`0x1c`), ALPHABLENDENABLE 1 (`0x1b`), SRCBLEND 5, DESTBLEND 6. The FVF `0x102` (XYZ, TEX1) has no diffuse colour, so each flake is a white square. A native capture (`ct_t1`, `WPC_WEATHER=1`) shows small white squares falling. Not compared with Wine.
- It steps once per race update, with its own MSVC generator.
- `WPC_WEATHER=0/1/2` forces METEO.
- Xvfb captures show the slanted grey streaks on cc_t1 and small flakes on ct_t1.
- Not compared with Wine. The lighting changes while it rains (`0x4ae364`, `0x4b08b1`, `0x4b4327`) are not ported.

## Weather shelters: `Zartter` (2026-11-02)

- `Zartter` (class `0xac11747a`, descriptor `0x6abba8`, names at `0x52aa68`: `Zartter`, `Eso`, `m_EsoTexture`, `Kosza`; Hungarian "zárt tér", enclosed space, and "eső", rain). Brick constructor `0x455c50` (box defaults 0 / 1000), init `0x455d40`, runtime 0x9e8 bytes.
- Properties: `0x8e..0x90` min x/y/z, `0x91..0x93` max (swapped per axis when reversed), `0xd5` geom (0 on every corpus course; nonzero goes through `0x459570` with a model's bounds, not ported).
- `0x456050` (world load) collects the top-level Zartter bricks into `0x6abc5c` (count `0x6abc30`, at most 100); `0x6abbc0` = 1 when one has children, in which case `0x456180` sets/clears flag 0x100 on the node (none has).
- `0x45a270` builds 6 planes per frame (matrix 0 and the wrapped matrix 1): normal ±axis/|axis|, the point being the frame origin with only component i moved by `axis[i] × bound` (exact for unrotated boxes, slightly off for rotated ones such as cc_t1 #14), and the 8 corners for the AABB `+0xdc..+0xf0`. A point is outside when a plane gives `n·p + d < 0` (`0x510124` = 0).
- `0x456180(camera)`: per box, the wrapped frame when `0x6abb40[brick+0xb4]` (the node's section `+0x104`) says the box lies across the seam from the camera's section; inside any box → 0 (no weather).
- Native (`RaceTrack::sheltered`, `RaceView::weather`): the normal, wrapped and unwrapped frames are all tested (provisional for the section rule). Corpus: cc_t1 2, cc_t2 2, cc_t3 3, cc_t4 3, ct_t2 6, ct_t3 2, ct_t4 1, dr_t4 3 (ct_t1, dr_t1, dr_b none). Not compared with Wine.

## Rain lighting (2026-11-08)

### Recovered

Three changes apply when rain is on (`0x2a1cc74` set and `0x2a1b2b0` = 0, i.e. not snow):

- **Clear and fog colour (`0x4ae364`, in the frame routine `0x4ae300`).** The colour built from the
  viewport is turned into (R >> 2, G >> 2, B × 192 >> 9). It is used for the device `Clear`
  (vtable `+0x28`, flags 3) and for `D3DRS_FOGCOLOR` (state `0x22`).
- **Textures (`0x4b08b1`, in the conversion `0x4b0550`).** The three channel divisors
  (`+0x2c`, `+0x38`, `+0x30`) are doubled, so every texture converted for the race comes out at
  half intensity. The HUD's are included.
- **The backdrop (`0x4b4347`, in `0x4b41a0`, called by `0x4ae300`).** Every vertex colour of the dome
  is written as bytes B `0xc0`, G `0x80`, R `0x80`; the alpha is kept.

### Wine (cc_t1, METEO forced to 1 by writing `0x6f464c` at the menu)

The flags read on, rain and cut (01, 0, 01). Against a clear run at the same place:

| Pixel | Clear | Rain |
|---|---|---|
| sand | (255, 206, 140) | (123, 97, 66) |
| water | (0, 146, 255) | (0, 69, 123) |
| sky | (255, 231, 115) | (58, 57, 41) |
| a HUD bead | (82, 49, 173) | (33, 16, 82) |

The sky follows halved texture × (0.5, 0.5, 0.75): (61, 55, 41).

### Native

- `RaceView::rain_dim_` halves each texture's RGB at upload (world, general and HUD sprites).
- The clear and fog colour take the shift rule; the backdrop's vertex colours become (128, 128, 192).

### Native capture (same place, `WPC_WEATHER=1`)

| Pixel | Native | Wine |
|---|---|---|
| sky | (64, 58, 43) | (58, 57, 41) |
| sand | (127, 100, 68) | (123, 97, 66) |
| water | (1, 72, 126) | (0, 69, 123) |
| far hills | (44, 29, 0) | (41, 28, 0) |

The differences match the original's quantisation to its 16-bit formats.

### Not ported

- The quantisation itself.
- Whether textures loaded later in the race (clones) are also halved: in the original, every
  conversion while the flag is set.
