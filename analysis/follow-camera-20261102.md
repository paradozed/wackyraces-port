# Follow camera: `WFollowCamera_2` update `0x478070` (2026-11-02)

Ported in `src/port/race_camera.*` (`FollowCamera`), driven by `RaceSession::update_cameras` once per race update. Ghidra had no function at `0x478070` (body to `0x47973e`); it was decompiled headless from a scratch copy of `analysis/ghidra` (`CreateFunctionCmd` + `DecompInterface`), checked against `objdump` where the decompiler dropped `this` pointers.

## The target frame `+0x8eb` (`0x468620`)

- `0x468620` builds the car frame from the angles `+0x267/+0x26b/+0x26f` and the position `+0x288` (`0x43c250`, `0x43bf90`) and copies it to the node frame `+0x2c`.
- Argument 1 copies the whole 13-word affine into `+0x8eb`; argument 0 only moves its translation. The car update calls it with `+0x3eb > 0` (`0x470535`): the frame keeps its orientation only while a hit holds the car (motion counter ≤ 0).
- The alternative yaw `+0x283` (used when `+0x287` = 1) is dead: `+0x287` is only ever written 0 (`0x4678f8`, `0x469eb6`).
- `+0x91f` = 1 adds `+0x923` to the translation; only cleared (`0x469e5f`), not ported.
- Native: `RaceSession::follow_frame()` is `RaceCar::frame()` (pitch and roll included), its rotation kept while `Body::motion` ≤ 0. The former lagged yaw (× 0.2, a native stand-in) is gone.

## Members (brick `+0x18`, 8 bytes apart from `+0x2c`)

| Offset | Member (ID) | general.wgm | after `0x477ea0` (1 player / 2 players) |
|---|---|---|---|
| `+0x2c..+0x3c` | m_rOrigPos (0x13b..0x13d) | 0,200,−450 | 0,300,−550 / 0,100,−800 |
| `+0x44`, `+0x4c` | m_rLookY/Z | 180, 200 | 110, 200 / 150, 200 |
| `+0x54..+0x64` | m_rOrigPos*_2 | 0,490,−720 | 0,400,−900 / 0,350,−900 |
| `+0x6c`, `+0x74` | m_rLook*_2 | 300, 0 | 300, 0 / 150, 0 |
| `+0x7c..+0x8c` | m_rOrigPos*_Near | 0,700,0 | 0,100,100 |
| `+0x94`, `+0x9c` | m_rLook*_Near | 0, 100 | 100, 200 |
| `+0xa4` | m_rSpeedRotZ (0x14a) | 0.05 | kept |
| `+0xac` | m_rDistSpeed (0x14b) | 0.1 | 0.2 |
| `+0xb4` | m_rS (0x14c) | 0.85 | 0.6 |
| `+0xbc` | m_rNearRadius (0x14d) | 400 | kept |
| `+0xc4` | m_rInertia (0x14e) | 0 | kept |

The camera copies the looks to `+0x28/+0x38`, `+0x30/+0x40`, `+0x48/+0x50`, and halves the three Y looks (`0x51026c`).

## Update `0x478070`

Camera fields: offset `+0x5c`, look targets `+0x68/+0x70` and current `+0x6c/+0x74`, eye `+0x78`, up `+0x84`, last eye motion `+0x90`, flags `+0x9c` (1 view _2, 0x10 rear view, 0x100 near), toggle wait `+0xa0`, near updates `+0xa4`, last car position `+0xa8`, last yaw `+0xb4`.

1. Actor `+0x8e7` nonzero (the drowning counter, 40 at `0x4700ac`) re-runs `0x477b60` every update.
2. Brick `+0xc0` (the driver, `0x49eb20`: 1 robot, 0 player, 2 past the finish) nonzero: the rear view is cleared and, unless the flags are exactly 1, view _2's offset and looks are rewritten (ported). Otherwise keys (unless `0x6e457f`, unidentified): input `0x4` (`0x52aad4`, device bit `0x1000000` = control 5, '.') held sets 0x10; input `0x8` pressed (`0x52aad0`, `0x100000` = control 4, Return) toggles view 1 / view _2 with a 3-update wait.
3. Not near: when `|eye − last car position|` (each axis truncated, >> 4) is below `m_rNearRadius >> 4` and actor `+0x8e3` is 0 (25 after a return, `0x4701cf`), enter near mode: offset _Near with z − 200, the _Near looks; the car's parts `+0x273..+0x27f`, `+0x24f` and the template part `+0x2e0` take the camera node in `+0x24`.
4. Near: two rays (`0x4a4830`, mask `0x018000ff`) from `pos + 0.1 (W − pos)` ± 40 × X to `W + 200 dir` ± 108 × X, W the view's offset point. A miss counts 1, a hit beyond `(r>>4)² + 12500` counts 10; a still car (speed < 0.1, same yaw) counts 0. With 2 or > 10 and more than 5 near updates the eye jumps to `last + dir × (r + 1)` and the view's offset returns.
5. States 1/0xc (manager `+0xf30`, before the countdown; battle starts in 9): with counter n = `+0xf34`, n = 0 stores the offset; up to 40 the offset is `base + (sin a × R, (40 − n) × 15, (cos a + 1) × R)`, a = n × 0.15708 + π, R = (40 − n) × 20 + 1000; past 40 the stored offset.
6. Eye += (pos − last) × m_rS + (W − eye) × m_rDistSpeed − m_rInertia × last motion; the looks ease × m_rDistSpeed; aim = pos + y × (U + world up) + z × Z.
7. Not near and not in states 1/0xc: two rays from `pos + 0.1 (eye − pos)` ± 40 X past the eye (+200, ± 108 X); the first hit puts the eye at `hit − 200 dir − X (1 + 1.7 f)`, scales the looks by f (the hit fraction) and zeroes the motion; a second hit averages. Then a vertical ray from 100 below to 100 above the eye (one-sided WGNR: a ceiling) holds the eye 100 under it.
8. Up += (U − up) × m_rSpeedRotZ; right = |up × dir|, up' = |dir × right| (`0x452ed0`, `0x452e20`).
9. Rear view: the basis and eye are taken into the frame (`0x43c930`), x and z negated, and back (`0x43b980`/`0x43b880`): a half turn about the car's up axis.
10. Matrix (right, up', dir, eye) into the camera node (`0x43aec0`); last position = pos; motion = eye − old eye.

## Native

- Return/'.' (MenuPrefs controls 4 and 5, both players) toggle and hold the views.
- The near mode hides the car (body and shadow) in its own view (the `+0x24` reading is provisional).
- The camera crosses the WMOD loop seam with its car (native; the original's handling is not traced).
- Synthetic checks in `native_drive_test` (`test_camera`): the initial pose, the still eye, the view toggle, the rear view, a wall pull-in, the end of the swing, the near mode.

## Checked / not checked

- Headless captures of `cc_t1`: the swing round the grid, then the view behind the car through PRET/PARTEZ.
- `cc_t1` test driver 115.9 s (116.6 before; the camera does not drive the car, the difference is the race runs).
- Not compared with Wine: the swing, the spring lag, the wall and ceiling pull-ins, the near mode, the rear view.
