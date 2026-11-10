# WPU_Breath's model on the car (2026-11-08)

Seen under Wine on `cc_t1`: as the breath (weapon icon 14) fires, a model appears on the car. It is
now drawn natively; its height still differs from Wine's (see "Open").

## The template (`general.wgm`)

- **The brick:** GDStdBrick `0x35e` carries the WPU_Breath action. Its members are `m_idClonable`
  `0x39a` (the DragonBreath projectile), `m_nLifeTime` 60, `m_nScaleNum` 3 and `m_scale` 1.
- **Children:**
  - GDModel `0x360`: GMDL `0x102ef`, 52 vertices, identity matrix;
  - GDHModel `0x361`: GHMS `0x102ee`, 120 vertices, 6 segments, motion MTNL `0x10358`;
  - GDSound `0x362`.
- **MTNL `0x10358`:** 5 clips, 6 segments.
  - Clip 2 (duration 20) chains to clip 0 (270), which chains to clip 3 (140); then the chain ends.
  - At 10 time units per drawn frame (`0x437d00`), that is 43 frames.

## Init (`0x4dab40`)

- `+0x389` = 0xe and `+0x38` = 33 (the counter).
- On the GDHModel: vtable `+0x4c`, then `0x440970(2, 0, 1.0, −1)` on `+0x15e`. That plays clip 2
  with offset 0 and weight 1; −1 lets `0x40526d` pick the controller.
- The clone goes under `[0x6f2c40]`, and the object query `0x403d69` gives the target (`+0x24`).
- **The clone's frame:** `[car+0x24f]+0x15e` → `+0xc` → `+0x68`, times the world matrix of the car's
  current model (`car+0x14`, `0x402c61`).

## Update (`0x4dac70`)

- **Group:** the clone and its subtree take the car's group (`+0x104`).
- **Frame:** `[car+0x24f]+0x15e` → `+0xc` → `+0x34`, times the car model's world matrix. When the
  action's `+0x5c` is set, the frame turns by π about y.
- **At `+0x38` = 28** (`0x4dad15`):
  - it clones `m_idClonable` under `[0x6f2c40]`, at the brick's first child's world frame moved 100
    (`0x510414`) along its z axis;
  - the hit type `+0x12a` = 8; the clone and its WBreath (`+0x11e`) are activated, and WBreath
    `+0x24` = the car;
  - a sound starts (`0x405164`, kept in `+0x4c`).
- **The counter** loses 1 each update. Below 0, the sound stops, the action is deactivated,
  `+0x389` = −1 and the clone gets flag 1 (deleted).

## Vehicle 3's body (the breath is its own weapon, 11 + 3)

- **The chain:** `model_ids[0]` is GDModel `0xf6`, then GDModels `0xa8`, `0xa7`, `0xa5` and `0xa6`,
  then GDHModel `0x8d` (GHMS `0x102cd`, 860 vertices, 6 segments).
- **Segments:** 1 at (12.3, 8.1, 4.9) from the root; 2 and 4 under 1; 3 under 2; 5 under 1.
- **Checked under Wine** (next section): `+0x15e` → `+0xc` is the posed segment-matrix array, and
  `+0x34` and `+0x68` are segments 1 and 2.

## Read under Wine (`cc_t1`, vehicle 3 on the grid)

- **The segment array:** `[car+0x24f]+0x15e+0xc` points to the posed segment matrices of the car
  body, one per 0x34 bytes. Records 0..5 hold the identity rotation, with the SEGS translations
  summed along the parents:
  - (0, 0, 0), (12.3, 8.1, 4.9), (12.3, −2.3, −32.7), (12.4, 148.6, −91.0), (26.9, 64.5, 17.6) and
    (−8.5, 56.6, 18.9).
  - So `+0x34` (the update) is segment 1, and `+0x68` (the init) is segment 2.
- **`car+0x14`** (node id `0x2b5`, the model root) sits at the actor + 20 up (actor (3631, 1012.1,
  −6862.6), root (3631, 1032.1, −6862.6)), as `initial_vehicle_model_frame` gives.
- **`car+0x24f`** (the body GDHModel, `0x2bb`) sits about 12 units away. The clone multiplies segment
  1 by the root's frame, not by the body's.

## Native

- **Race-side animated model:** `RaceView::AnimatedModel` and `load_animated` take a GDHModel's
  mesh, its batches from `load_model_group` with a per-corner map back to the mesh vertices, and its
  MTNL.
  - `AnimatedInstance` holds a `MotionController`. `pose` steps it by 10 per race update since the
    last pose (the original steps once per drawn GDHModel; provisional), runs `pose_mesh`, and
    rewrites the batch positions.
- **`submit_breath_head`:** while the car's weapon is icon 14 with effect 0xe and its timer is
  ≥ 0, it draws GDModel `0x360` and the instance of `0x361` (clip 2 on the first draw) at
  `initial_vehicle_model_frame(frame)` × T(segment 1 of the body's first GDHModel, at rest).
  - The seam placement is the car's.
  - Since 2026-11-08 the body sways (controller 0), and the head sits on the posed segment 1
    (`pose_mesh` gives the segment matrices). At rest it is unchanged: 28 to 139 above the car.
- **Debug:** `WPC_DEBUG_BREATH=1` logs, on each draw, the instance's world box against the car.

## Open

- **Height:** natively the model spans 28 to 139 above the car and up to 124 ahead of it. From the
  follow camera it stays hidden behind the car's tower, while under Wine it rises above the tower top.
  - Not traced: the first child's `+0x14` and its vtable `+0x4c` call at init; the power-up action's
    `m_nScaleNum`/`m_scale`; whether the original draws the static GDModel `0x360` and the GDHModel
    `0x361` in the same frame.
- **Not compared:** the clip timing (10 per drawn frame under Wine, per update natively); robots of
  vehicle 3 firing.
