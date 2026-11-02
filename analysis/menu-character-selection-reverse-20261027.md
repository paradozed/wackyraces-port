# Character-selection page: recovered behavior and native match (2026-10-27)

Static analysis of the installed `WPC.EXE` plus a fresh same-state Wine run
(Wine 11.20, private Xvfb display, scratch copy of the prefix with `D:` on the
mounted disc, 1024x768 client). Screenshots are game content and are kept out
of git; the procedure below reproduces them.

## Class layout

| Class | Runtime vtable | Overrides that matter here |
|---|---|---|
| `WMenuNodeCarSelection` (descriptor `0x513ccc`, ctor `0x48dd60`) | `0x513d00` | slot 16 `0x48de20`, slot 18 `0x48dde0` |
| base `WMenuNode` | `0x5139a4` | slot 18 `0x48c0a0` (child `+0x90` availability) |
| `WMenuCarItem` runtime (`0x12e` bytes, factory `0x4890c0`) | `0x5135f4` | slot 19 `0x4893c0` |
| `WMenuItem` select / deselect | `0x488ba0` / `0x488b20` | — |

## Lock and cursor

`0x48dde0` (slot 18, availability by item index): index 0 returns
`profile.byte[+0x105] != 0` via `0x496b20` on the profile object `0x6f4568`;
every other index returns 1. Row 0 is `WMenuCarItem 0x9d`, Satanas et Diabolo.
The generic cursor code skips unavailable rows, so the page opens on row 1
(Dingo-Limousine), and Left/Right wrap past row 0. Wine confirms the order
Turbo Terrifique ← Dingo → Tocard Tank → Compact Pussycat → Bolide Ecarlate →
Caraverne → Turbo Terrifique.

The unlock byte is serialized at `0x6f466d` with the other profile bytes
(`0x4b7c0a`). Writers:

- `0x496b80(course_set, championship)` (called at `0x491da0` when a
  championship ends): if the player-1 vehicle (`game+0x1a68`) equals the
  points leader (`0x496d90`, max of the seven `+0x10d` scores), it unlocks that
  championship's rows (`+0x101+n`, `+0xec+n+4i`); for championship 3 it also
  sets `+0x105` (Satanas).
- `0x496e10` sets every unlock flag and fills `+0x164..` with `0x7f`. Its only
  caller is `0x48f180`, which `WMenu` update `0x48fac0` runs only while the
  current node has code 44 (Credits). It matches the 11 DirectInput scancodes at
  `0x52bfc4` as rising edges: `W A C K Y space R U L E Z`.

Page 61 uses another node, whose availability method `0x48e2d0` also returns 0
for the row held by both cursors (`+0x12a == +0x132`). Independent two-player
cursors are not ported.

## Selection effects

`0x4893c0` (car item slot 19) handles event `0x800000` by storing the item's
`m_nVehicleId` into `game+0x1a68` (player 1), and `0x1000000` into
`game+0x1a6c` (player 2). `0x488ba0`/`0x488b20` OR or clear the same selection
mask into each direct child's flags at `+0x90`. `WMenuText`/`WMenuSprite`
children use their own handlers. Plain `GDSprite` icons and `GDModel` groups are
therefore visible only while their item is selected. The native view had drawn
all seven items' icons, so Turbo Terrifique's (last) always showed.

## Preview rotation: `WRot`

Every car group (`GDModel 0x9e, 0xb4, ...`, identity transform) owns a
`WRot` action. Registration `0x497b00` names properties `0x1cf..0x1d1`
`m_MaxRotDeg{X,Y,Z}`, `0x1d2..0x1d4` `m_StepRotDeg{X,Y,Z}` and `0x1d5`
`m_lType`. All 14 menu instances are `type 2`, `StepY = 0.0872665` (5°).

`0x498170` (per frame): for type > 1, add each nonzero step to the Euler angle
and wrap it, with no frame-time scale. Then save the position (`0x43bf60`) and
reset to identity (`0x43b090`). Apply `rotate_basis_y` (`0x43b4a0`, local X ->
`(cos, 0, sin)`), then the Z and X rotations, restore the uniform scale, and
restore the position. The main tick `0x4ad540` spins until at least 33 ms have
passed since the frame started, so the nominal rate is 5° per 33 ms frame.
Wine measured a 2.667 s period (27 frames/s), and per-frame screen changes
matched the rotation step. The native preview previously spun at 0.35 rad/s
in the opposite direction, around the bounds centre.

## GHMS segment pass (tower/driver fix)

`0x437040` walks segments in descending order. Within one iteration it fills
that segment's HMTS span and then rewrites that segment's TSET indices. A
segment therefore resolves its own span before lower segments overwrite the
overlap; only references outside its span see the earlier, higher-numbered
segments. In `GHMS 0x7ae` each body-part TSET references exactly its own span
(tower set 3: `[20,183]`; set 4: `[10,86]`; set 5: `[0,29]`). The root seam
skin (seg 0, processed last) references `[0,64]` across all owners. The
native loader filled the whole map before rewriting, so slots 31..183
resolved to seg-1 body vertices and the tower collapsed into the cab. With
the interleaved order, all 58 GHMS in `menu.WGM`, `SWMenu.WGM`,
`general.wgm` and `swgeneral.wgm` load, and every reference hits an
already-filled slot (now enforced).

## Native comparison

A 3 s burst from each runtime was rescaled to 640x480. Each Wine frame was
paired with the closest native frame (mean absolute difference over the car
region). The best Dingo pair differs by 10.5/255, mostly text glyph spacing.
The car silhouette, tower, driver, wheels and icons coincide. The matched
native index advances monotonically with the Wine index across a full
revolution, confirming direction and rate. All six playable cars match the
Wine card sheet in order, model and icons.

The star shadow (`GMDL 0x7ca`, `GMAT 0x2b4`: black texture at ~35% alpha)
serializes before the page plane `GMDL 0x7c9`. Drawn in that order with depth
writes, it blended against the clear colour (teal). Wine shows it blended over
the purple plane. The recovered ordering (`analysis/transparent-draw-order-20261028.md`)
explains it: 24/32-bit textures are drawn after all opaque geometry. Lighting, Gouraud colour and exact pixel parity remain unverified.
