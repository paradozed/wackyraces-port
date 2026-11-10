# WRocketTail and the breath's film (2026-11-08)

Native:
- `Projectile::tail`, `update_tail` (`race_weapons.*`), with the init in `RaceSession::fire` and the
  update in `update_weapons`;
- the tail draw in `RaceView::effects`, and the breath film among the film quads of `draw_view`.

## The class

- **Registration:** at `0x4ea000` (record `0x2ab1a30`, name `0x52e860`, class id `0xb6840fcd`). The
  method table is `0x516b64`; the factory `0x4ea050` makes 0xd3b-byte instances with vtable
  `0x516b8c`.
- **Slots:** init `+0x10` = `0x4ea7d0`, update `+0x8` = `0x4ea960`, matrix and draw `+0xc` = `0x4eacc0`.
- **Members** (template record, `+0x24` + 8 × (id − `0x2ec`)):

  | Member | Offset | Missile `0x38c` | Breath `0x39a` | Boomerang `0x3a1` |
  |---|---|---|---|---|
  | `m_Length` | `+0x24` | 16 | 16 | 16 |
  | `m_Width` | `+0x2c` | 100 | 100 | 100 |
  | `m_Type` | `+0x34` | (default) | −1 | 1 |
  | `m_Red/Green/Blue` | `+0x3c..+0x4c` | 128, 128, 0 | 128, 10, 0 | 32, 32, 32 |
  | `m_Tick` | `+0x54` | 0 | 0 | 0 |
  | `m_Glow` | `+0x5c` | 300 | 300 | 0 |
  | `m_GRed/GGreen/GBlue` | `+0x64..+0x74` | 128, 10, 0 | 128, 10, 0 | 0 |
  | `segments` | `+0x7c` | 0x0f | 0x0f | 0x0a |

  The tank gun's DragonBreath `0x3a8` has no tail. Neither the update nor the draw reads `m_Type` or
  the colour members.

## Init (`0x4ea7d0`)

- **Points:** `m_Length` records of 0x24 bytes from `+0x6b`: the owner's translation, its up axis
  (`0x43bf00`) × `m_Width` at `+0x77`, and its x axis (`0x43bed0`) × `m_Width` at `+0x83`.
- **The rest:** `+0xd37` = 0 (tick), `+0x67` = 200, `+0x68` = `m_Glow` (a word), `+0x6a` = 8, `+0xceb`
  = 30, `+0xcef` = 0 (phase), `+0x28` = 0, `+0x60` = 0.

## Update (`0x4ea960`)

- **Tick:** `+0xd37` + 1. When it reaches `m_Tick`, it returns to 0 and the points shift back one
  (`rep movsl` from the end).
  - `+0x60` loses 1 when the last two differed, and gains 1 when the first two do. The draw does not
    read it.
- **Point 0:** the owner's translation; up × `m_Width` × 3 (`0x511894`) and x × `m_Width` × 3.
- **Phase:** `+0xcef` + 0.3 (`0x516c30`). With s = sin and c = cos of the phase:
  - `+0x68` = ftol(300 + 30 c);
  - `+0x64` = ftol(240 + 15 s) (R), `+0x65` = ftol(150 + 50 s) (G), `+0x66` = 120 (B),
    `+0x67` = ftol(200 + 50 c) (A);
  - `+0x28` = 12.

## Draw (`0x4eacc0` with `+0x20` ≠ 0, Direct3D path `0x6f5958` = 1)

- **Points 1..L−1, relative to point 0:** the centre c and the corners c ± s·up, c ± s·x
  (records of 0x3c bytes from `+0x527`). The taper s is 1 at point 1 and loses 1/L a point
  (`0x510ae0` / L), down to 0.
- **The glow** (`0x4b44e0`), when `+0x28` = 12 and `+0x68` ≠ 0:
  - a fan facing the camera (the camera's rotation, `0x23965`) at point 0;
  - `+0x6a` = 8 segments at angles 2k·π/8, radius `+0x68`;
  - centre colour `+0x64` (read as R, G, B, A), rim alpha 0; blending on, no depth write
    (FVF `0x42`, a triangle fan).
- **Fins:** with blending on and no culling, and only when `segments & 1` (all four calls test the
  same bit), `0x4eb260` is called with corners 1..4.
  - Each call builds, from the centre line (alpha 255, minus 255/L per segment) to the corner
    (alpha 0), the triangles (b_i, b_i+1, a_i) and (b_i+1, a_i+1, a_i), in `0xffe8dfac`
    (232, 223, 172).
  - It passes L as the triangle count to `0x4b5550` (mode 9: default blend, no depth write), so
    only L / 2 segments, points 1..9, are drawn.
- **Hence:** the missile and the breath get the fins and the glow; the boomerang gets only the glow.

## The breath's film

- **The film:** DragonBreath `0x39a` has no GDModel; its GD3DFilm `0x39d` uses FILM `0x102e2` (8
  frames of 24 × 23), `m_AnimType` 2, `m_Speed` 1, `m_LastFrame` 7, white, `m_Scale` 1, render
  setting `0x180042`.
- **Playback** (`0x430110`, mode 2): the frame is trunc(accumulator), back to 0 past 7.
- **Draw:** its frames have SILL 1 and SMOO 0, so the default blend, moved a fifth of the way toward
  the camera (`0x42d2b4`).

## Native

- **The tail:** `Projectile::tail` holds the 16 points (position, up, x, the factor 1 or 3);
  `update_tail` runs after the projectile's motion.
- **The view:** it reads each template's `m_Length`, `m_Width` and `segments`, and draws the fins and
  the glow with the white texture.
- **The breath film:** drawn among the film quads, at the projectile.
- **At the loop seam:** the points cross with the projectile. This is **provisional**: the original
  keeps world positions there (`0x4a23c2` moves only the object's matrix), so it may draw a long
  streak for 16 updates. Not compared.

## Compared with Wine (`cc_t1`, the player's slot written)

- **The setup:** `+0x375` = `0x2ab078c[icon]` (the table `0x467f6b` uses), `+0x391` = the icon,
  `+0x39f` = 3, `+0x399` = 3, then the weapon key. The effect became 0x19 for the missile and 0xe for
  the breath.
  - Writing `0x6f2c50[icon]` into `+0x375` instead crashed the original.
- **Missile:** the same pale wedge of fins behind it and an orange-red glow at its head. It struck a
  robot within half a second, so the trail's length was not compared.
- **Breath:** the same red fireball film with a pale trail.
  - Wine also shows a large flaming dragon head on the car as it fires. That is WPU_Breath's own
    visual, not ported.
- **Not compared:** the boomerang's glow; the tail across the seam.
