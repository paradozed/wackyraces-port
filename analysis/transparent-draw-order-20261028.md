# Transparent draw ordering (Direct3D path)

Recovered from the executable. The menu page (`MenuModels::draw`) and the
race world (`RaceView::draw`) follow it through `TranslucentQueue`
(`game_world`), including the additive and no-depth-write draw flags.

## Which parts are translucent (`0x4b1f30`)

The mesh cache builder checks each material of a mesh. A part is translucent
(cache flag `0x10`, part byte `+4` = 1) when:

- the material's `TRAN` (`+0x60`, loader `0x43a9e0` area) is not 0.0
  (`0x510124`), or
- its texture has surface flag 8.

**Surface flag 8 (`0x4b0550`):** set only on the format path 4, which converts
to the alpha texture format (`0x26fa630`, found by `0x4b14b0`). The jump table
`0x4b0e40`/`0x4b0e50` sends 24- and 32-bit sources (TGA depth, image `+0x10`)
to `0x4b06db`. That path would use a 32-bit format (`0x26fa478`, bit count
`0x26fa484`), but its block (`0x26fa430`) is only ever zeroed (`0x4b14d6`).
So every 24/32-bit texture gets the alpha format and flag 8. 8-bit textures go
to the 16-bit format and use a D3D colour key (`0x4b0df3`), not blending.

When the texture also has flag 8 and `TRAN` ≠ 0, the texture gets `0x20`.
`0x4b3630` then modulates alpha (TSS 4 = 4) instead of selecting it (2).

## Queueing (`0x4afab0`, `0x4afcc2`)

- **Opaque parts** draw as the model is traversed (or join list `+0xd8`).
- **Translucent parts:** one entry (0x48 bytes) per model goes to a list
  chosen by the model's draw flags (`+0x12a`):

  | Flags | List | Drawn |
  |---|---|---|
  | `& 4` | `+0x3f8` | first |
  | otherwise | `+0x3f4` | second |
  | `& 0x20000` (checked last) | `+0x3fc` | third |

- **Key:** `0x4affe0` takes the model matrix's translation (`0x43bf60`, `+0x28`)
  through the view matrix `0x26fa530` (`0x43b880`). The view-space z is
  truncated by `ftol` and stored at `+0x48`.
- **Insertion:** the new entry goes before the first entry whose key is not
  larger (an unsigned compare). Each list therefore runs far to near, and among
  equal keys the latest model comes first.
- **Draw flags (`+0x12a`):** `GDModel::m_RenderSetting`, property `0x68`.
  See "Property registration" below. The constructor (`0x43f1d0`) stores the
  default `0x80062`.

## Sprites (`GD3dSprite`, `WGndSprite`)

- Both classes (factories `0x4192d0`, size `0x162`, and `0x484570`, size
  `0x18a`) draw with `0x419b90` (vtable slot `+0xc` of `0x510464` and
  `0x5131c4`).
- That routine builds a quad in the sprite's own model (`+0x15e`), copies the
  sprite's `+0x15a` into the model's draw flags `+0x12a` (`0x419f0e`) and
  `+0x68`, then submits the model through the renderer's slot `+0x40`. The
  renderer vtable is `0x514c74`, and `+0x40` is `0x4afab0`, the model queue
  above.
- So sprites join the same per-model lists as meshes. `+0x15a` is
  `GD3dSprite::m_RenderSetting`, property `0x10`. The constructor
  (`0x41981a`) stores `0x80062`, and every course and `general.wgm` sprite
  serializes that value.
- The routine also computes the facing; its matrix work (`0x419d37`, three
  cases of `+0x12a` of the linked camera object) is not traced here.
- **Not the sprite list:** `+0x234fd` (`0x4b50e0`, called from `0x42d184`)
  holds copies of `GD3DFilm` objects, not of `GD3dSprite`:
  - The class has size `0x1bc`, vtable `0x510e74`, constructor `0x42c500`,
    factory `0x42b5b0`, and is registered at `0x42b560` under the name at
    `0x529900`. `WKBumm` (`0x4866c4`) derives from it.
  - The draw `0x42d090` queues a film there on the Direct3D path
    (`0x6f5958` = 1).
  - Entries are kept nearest first by squared distance to the camera, at most
    200 (`0xc8`). The flush draws them in reverse, with `+0x64` set during the
    draw.

## Draw flags in the race data

| Source | Value | Nodes |
|---|---|---|
| Championship I courses | `0x80062` | all 123 models and 593 sprites |
| `general.wgm` | `0x80062` | most models |
| | `0x80066` (list `+0x3f8`) | GDHModel 73 under GDModel 134, GDHModel 141 under 246 |
| | `0x88062` (additive) | GDModels 414, 415 of `WCarFxStorage`/`WSplash` 412 |
| | `0x2080062` (no depth write) | GDModel 194 under `GDStdBrick` 804 / GDModel 809 |

`GD3DFilm`'s own `m_RenderSetting` (`GDFilmBase`, property `0x36`) is
`0x180042` for `WCarFxStorage`'s films 410 and 411 and for 906/925, and
`0x2180042` (no depth write) for 447 under `GDStdBrick` 446. None of these
objects, nor `WSplash` 412 (models 414/415, additive), is drawn by the port
yet.

## Property registration

**Property IDs come from the archive's member table:**
- Each WGM's `CLAS` chunk has a `PRIM` table: a count, then per entry the member name and its class name.
- When `0x54433c & 1` is set, the `PRPS` loader (`0x41bd20`) remaps a file property ID through `0x6efa7c` before looking up the registered member.
- The member descriptors form the list `0x6efa30`. `0x45c300` adds one per registration, and `0x45c4a0` reverses the list into registration order.

**Relevant entries:**

| ID | Member | Class |
|---|---|---|
| `0x09` | `m_MaterialID` | `GD3dSprite` |
| `0x10` | `m_RenderSetting` | `GD3dSprite` |
| `0x1a` | `m_nFlags` | `GDBrick` |
| `0x36` | `m_RenderSetting` | `GDFilmBase` |
| `0x65` | `m_MotionLib` | `GDHModel` |
| `0x67` | `m_3dModelID` | `GDModel` |
| `0x68` | `m_RenderSetting` | `GDModel` |
| `0x6a` | `m_VcRadius` | `GDModel` |

**Field offsets:**
- The registrations (`0x43ee70` for `GDModel` with offset `0x8c`; `0x419710` for `GD3dSprite` with `0xbc`) give offsets from a property base at `+0x9e`. That gives `+0x12a` and `+0x15a`.
- The same base takes `m_3dModelID` (`0x84`) to `+0x122`, the mesh link that `0x4403f0` and `0x419bae` read.

**Consistency across archives (`native_scene_test`):**
- `menu.WGM`, `SWMenu.WGM`, `general.wgm`, `swgeneral.wgm` and every disc course share the same 764 entries.
- The seven disc `Win/*.WGM` have 757 entries: they lack `Zartter::geom`, `Eso::m_EsoTexture` and five `Kosza` members. Their IDs from `0xd5` on are therefore shifted, including `WRot` `0x1cf`/`0x1d5` and `WPowerUp` `0x28b`.
- The port reads fixed IDs and does not load those archives yet.

`0x1a` (`GDBrick::m_nFlags`) is the property whose bit `0x100` hides `cc_t1`'s grey occluder slabs (`RaceView`).

## Flush (`0x4b2d70`, end of frame)

1. List `+0x400`.
2. Lists `+0x3f8`, `+0x3f4` and `+0x3fc`, each wrapped in ALPHABLENDENABLE
   on/off.
   - Entries of `+0x3f4` whose mesh is in the table at `+0x23501` are
     deferred: they are redrawn after the 0x1000-flagged objects.
3. Objects with flag `0x1000`.
4. The deferred `+0x23501` entries.
5. The sprite list (`+0x234fd`), in reverse.
6. The 2D sprites, bubble-sorted by `+0x30` and drawn last to first
   (`0x4af1a0`).

**Per-entry state (`0x4b3630`):**
- Blend factors stay SRCALPHA / INVSRCALPHA (`0x4b1ad0` defaults), and depth
  writes stay on.
- Draw flag `0x8000` sets SRCBLEND and DESTBLEND (states 19 and 20) to ONE.
- Draw flag `0x2000000` sets ZWRITEENABLE (state 14) to 0.
- Both are restored to 5/6 and 1 after the entry.
- Material flag `0x6c & 1` selects cull none, otherwise CCW.
- **Texture stage 0** (`SetTextureStageState`, vtable `+0x94`), per surface
  flags:

  | State | Value |
  |---|---|
  | ALPHAOP | MODULATE (4) with flag `0x20`, else SELECTARG1 (2) |
  | ALPHAARG1 | TEXTURE (2) with flag 8, else DIFFUSE (0) |
  | ALPHAARG2 | DIFFUSE |
  | ALPHAOP, untextured | DISABLE (1) |

  Flag `0x20` is set only when the material's `TRAN` ≠ 0. Without it, a
  flag-8 texture selects its own alpha. Correction 2026-11-01: the Direct3D
  diffuse alpha is `TRAN` itself (cache `0x4b21ae`), not 1 − `TRAN`; see
  `vertex-colours-20261101.md`. An 8-bit texture selects the diffuse alpha, and its colour key removes
  keyed texels. Every reachable case therefore equals the native shader's
  texture × vertex alpha with zero-alpha discard. No change was needed.

## Native

- `Image::depth` records the TGA depth.
- `MenuModels` marks a batch translucent when `TRAN` ≠ 0 or the texture is
  24/32-bit; it replaces the previous partial-alpha heuristic (42 materials,
  now 103 in `menu.WGM`).
- Opaque batches draw first. Translucent batches are grouped by model into the
  three lists, keyed and inserted as above, and drawn list by list.
- Spinning models use their turned origin.
- **Race (`RaceView::draw`):**
  - `load_world` marks each batch translucent by the same rule and stores
    the node's draw flags (`0x68` for meshes, `0x10` for sprites) and its
    translation.
  - Every placement draws its opaque batches at once: the world, the two
    WMOD loop copies, the pickups, the shadows and the cars.
  - Each placement's translucent models are queued under its frame, keyed by
    the view z of the model origin. The queue is flushed after the cars.
  - The backdrop dome is still drawn first, outside the queue (its placement
    is native).
  - Per course: 225 to 290 queued models (`cc_t1` 225, `sh_t1` 290, `dt_t1`
    288, `ct_t1` 253, including cars and pickups).
- The Vulkan backend has eight material pipelines (cull × additive × depth
  write). `wpc_vk_set_draw_mode` selects them, and `draw_mode()` maps a
  queued entry's flags. `MenuModels` and `RaceView` set the mode per entry
  in the flush and reset it afterwards. No drawn model has either flag yet.

## Verification

- **Race:** start-grid captures of the four Championship I courses, and five
  `WPC_RACE_PLACE` views, were compared with a scratch build that left the
  queue empty. The scenery is pixel-identical; the only differences are the
  opponents' grid order, which changes between runs, and the pickups' spin.
  Two reasons: the fragment shader discards zero-alpha texels, and 8-bit
  textures use a colour key, so the order only shows on partial-alpha edges
  and `TRAN` materials that overlap. No view with overlapping translucent
  scenery has been compared, and no Wine capture was taken.
- The menu capture after moving it onto `TranslucentQueue` is pixel-identical
  to the one before.
- **Draw modes:** `native_render_test` checks the presented pixels.
  - Additive: blue at alpha 0.5 over red gives (255, 0, 255).
  - No depth write: a green layer at depth 0.25 does not hide a later blue one
    at 0.5.
  - The car-selection capture and an `sh_t1` placed view are pixel-identical
    after the change.

- Car-selection captures (Dingo, native, before and after) differ only by the
  car's rotation phase. The star shadow (GMAT `0x2b4`, 24/32-bit) is still
  blended over the page plane, as in Wine.
- No new Wine capture was taken.

## Not done

- `WKBumm` and `GD3DFilm` 410. The dust films, ripples and `WSplash` are drawn
  ([`car-effects-20261028.md`](car-effects-20261028.md)).
- List `+0x400`, the `+0x23501` deferral and the 0x1000 objects.
- The backdrop's place in the queue.
