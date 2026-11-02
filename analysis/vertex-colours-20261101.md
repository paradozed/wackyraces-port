# Mesh vertex colours (VCOL), 2026-11-01

## Loading (`0x43da50`, VERT field `VCOL` at `0x43dd67`)

- The field holds 3 floats per vertex. They are stored at mesh `+0x30` (POSI `+0x24`, UVUV `+0x2c`).
- If no vertex has r + g + b > 0 (`_DAT_00510124` = 0.0), the array is freed and `+0x30` stays null.

## Use, software path (`0x422110`, per TSET of a model)

- Without the per-object colour flag (draw flags bit 23, `param_2[4] >> 0x17`), the source colour per vertex is:
  - `VCOL × 255` (`_DAT_00510418` = 255.0);
  - or 255, 255, 255 when `+0x30` is null.
- With bit 23 it is the object's `+0x50` colour instead (not ported).
- Material TRAN (`+0x60`, loaded by `0x43a710` from the `TRAN` chunk) selects how the colour is written:
  - TRAN = 0: rgb = the colour, alpha 255;
  - TRAN ≠ 0: rgb = 255, alpha = (r + g + b) × TRAN × 1/3 (`_DAT_00510afc`).
- The material colours (COLS) do not appear here. The texture is modulated by the vertex colour through the backend (vtable `+0xe4`).

## Use, Direct3D path (mesh cache `0x4b1f30`, colours at `0x4b21ae`)

Wine runs this path (renderer vtable `0x514c74`).

**Vertex format.** XYZ (2), plus DIFFUSE (`0x40`) when the mesh keeps VCOL, plus TEX1 (`0x100`) when a part is textured.

**Per material**, with k = TRAN:
- k is replaced by 1.0 (`0x510214`) when the part is translucent (cache flag `0x10`) and TRAN is 0.
- k = 0: the colour is ftol(VCOL × 255). The alpha byte is the low byte of `0x4e7`, so 0xe7, unused by opaque parts.
- k ≠ 0: the colour is ftol(k × VCOL × 255) and the alpha is ftol(k × 255).
- An all-zero colour gets alpha 0.
- The values are truncated, not clamped.

**Without VCOL** (`0x4b2633`): every vertex gets `ftol(TRAN × 255) << 24 | 0xffffff`, that is white with alpha TRAN.

**Texture stage** (`0x4b3630`):
- a 32-bit texture with TRAN ≠ 0 modulates its alpha by this diffuse alpha;
- otherwise the alpha is the texture's (32-bit) or the diffuse (8-bit, colour key);
- untextured parts set ALPHAOP DISABLE.

So the alpha is **TRAN, not 1 − TRAN**. The 1 − TRAN constant alpha belongs to the Glide updater `0x433d40` (`analysis/native-world-material-loading.md`).

**Translucent textured materials in use:**
- the cars' ground shadows `0x102d6`..`0x102dd` (TRAN 0.2);
- the splash `0x102f4` (0.35, additive);
- WalkOnWater `0x102f9` (0.49, additive);
- GHMS `0x10317` (0.75);
- GMDL `0x1031a` (0.3).

The two translucent meshes of `ct_t1` (nodes 94 and 614, untextured) carry the brick flag `0x100` and are hidden.

## Texture filtering (`0x4b1ad0`)

The device setup sets:
- stage 0 MAGFILTER, MINFILTER and MIPFILTER (TSS `0x10`..`0x12`) to 2, LINEAR;
- render states CULLMODE 1, DITHERENABLE 1, ALPHABLENDENABLE 0, SPECULARENABLE 0, FOGENABLE 1, CLIPPING 1, LIGHTING 0;
- SRCBLEND 5 and DESTBLEND 6.

Natively, `wpc_vk_draw_material_triangles` uses samplers 4–7, which are bilinear with the same clamp masks. The 2D draws keep nearest filtering; whether the original filters its 2D draws is not traced.

Wine comparison at `ct_t1`'s start: the snow, the road and the sprite edges now look as smooth as in Wine.

## Corpus

| Course | Meshes | With VCOL | Vertices not white |
|---|---|---|---|
| `cc_t1` | 394 | 319 | 10556 / 18245 |
| `sh_t1` | 198 | 162 | 11650 / 18028 |
| `dt_t1` | 585 | 565 | 11102 / 16802 |
| `ct_t1` | 194 | 162 | 9222 / 15259 |
| `general.wgm` | 108 | 80 | 4870 / 11138 |

Values lie in 0..1 except for three meshes: `ct_t1` GMDL 289 and 361, and `dt_t1` GMDL 786. Their values look like positions (up to ±23000).

## Native

`textured_vertex_color` (`game_material.h`) implements the Direct3D rule for textured materials in `game_world` and `menu_models`:
- TRAN 0: rgb = VCOL, or white;
- TRAN t: rgb = t × VCOL, or white, and alpha t.

`Mesh::vertex_colors` records whether VCOL was kept.

Earlier state (first pass, superseded):

- `load_mesh` reads `VCOL` into `MeshVertex::r,g,b` (default 1) with the `0x43dd67` drop rule. GHMS remapping carries the colours along.
- Truncated fields are rejected (`native_mesh_test`).
- `game_world` and `menu_models` multiply opaque textured materials by the clamped vertex colour.
- Untextured materials keep COLS[1], and translucent ones keep 1 − TRAN. Both are unchanged and marked as not ported.
- The clamp to 0..1 is provisional: how the backend packs out-of-range values is not traced.

## Verified

Wine (scratch prefix, Xvfb :77) and native (Xvfb :79) were compared at `ct_t1`'s start, mean colours of the same 10–40 px boxes:

| Spot | Wine | Native before | Native after |
|---|---|---|---|
| Snow mound, left | 211,231,250 | 248,254,254 | 212,235,254 |
| Fence and ground | 146,152,141 | 203,189,160 | 146,151,142 |
| Road | 189,202,221 | 219,220,227 | 181,192,227 |

- The pines are unchanged and already matched.
- The player car (vehicle 3, the house car) matches within 10/255 on the roof, wheels and windows.
- Menu car previews were checked natively only; no Wine comparison was made.
- Ground shadow of vehicle 3 at `ct_t1`'s start, mean of a 50 × 15 box behind the car:

  | Wine | Native, 1 − TRAN | Native, Direct3D rule |
  |---|---|---|
  | 231,234,239 | 154,158,159 | 222,228,228 |

- Not compared: the splash and WalkOnWater (additive, now darker by TRAN × VCOL), and the colour of untextured parts under Direct3D.
