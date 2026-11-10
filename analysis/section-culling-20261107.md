# Section culling and the loop seam (2026-11-07)

Native: `RaceTrack::node_sections` (`race_track.cpp`), `RaceView::draw_view` (`race_view.cpp`).

## Recovered

- **Enable (`0x4a8680`, thunk `0x404e35`)**, for one row of a tracker slot (`[slot+0xc]`):
  - Bit 0x2 of `+0xc` is set on the row's `+0x58` node and on every descendant (walk `0x4046f6`:
    first child `+0x60`, then the next one).
    - `+0x58` is the row's ground object, the section's WGndBrick (`ModeRow::ground_object_id`).
    - The brick itself is skipped only when it is `[this+0x18]+0x2c` and the game mode
      (`[0x5472b8]+0x1a60`) is neither 0 nor 3. Its descendants are still flagged.
  - The same is done for each kind-zero link of `+0xce4`/`+0xce8` naming a node with flag 0x10 whose
    parent is the world (`[0x5472b8]+0xc`). It is skipped for the player's vehicle when `0x6f2cc0` is set.
  - The same is done for the 32 node pointers at `+0x5e4..+0x664` (the row's WPowerUps).
  - Then `0x4032b5` (`0x4a89c0`) copies row `+0x1c..+0x30` into the slot (`+0x78..+0x8c`).
- **Disable (`0x4a8850`, thunk `0x40247d`)**: the same three groups (links and table only when
  they have 0x10 and 0x2) go through `0x405a6a` → `0x41ba90`. If the node is active (0x1000), it calls
  vtable `+0x14` and clears 0x1000 and 0x2. Enabling a node is `0x41ba60`: if it is not active,
  it sets 0x1002 and calls vtable `+0x10`.
- **Tracker** (per car, slots `+0x830`, `+0x8c4`, `+0x958`):
  - At reset (`0x49e261..0x49e2ae`), it disables the three slots, then enables them.
  - On a section change (`0x4a0bbf`, `0x4a0d1b`, `0x4a14b5`, `0x4a15c4`), it does the same for the
    moved slot. The enable is taken only when the stack flag `[esp+0xa8]` is set; otherwise only the
    copy is done.
- **Draw walk (`0x432490`, thunk `0x402ee1`)**:
  - It returns at once if the node lacks 0x1000 or has 0x100, or if its layer `+0x90` misses the
    camera's.
  - Otherwise it runs the node's actions (`+0x94`, those with 0x5000), draws the node (vtable
    `+0xc`, flag 0x4000, culler `0x6abaf0`), and recurses into the children.
  - An inactive node is not drawn, and neither is anything below it.

## Checked under Wine

- **`dr_t3`, player on the grid:** the grid lies before the start line, so the player's `+0x104` is 3.
  - The bricks of sections 0, 2 and 3 (#21, #391, #126) were active (`0x9000101a`, `0x900d901a`).
  - Section 1's brick #303 was `0x10000018`: no 0x1000 and no 0x2.
  - The robots were in sections 1 and 2 and enabled nothing.
  - So the drawn sections are the player's section and the two beside it, in a loop.
- **The file's flags:** property `0x1a` holds the initial flags (`0x100d9010` for brick #391 →
  `0x100d901a` at run time). On the 20 courses, every linked top-level node and every brick has 0x10.
  `cc_t2` links 2 nodes that are not top-level; the original ignores them.

## Native

- **A node's sections:** those whose enabling activates it.
  - The rows' `+0x58` brick, with its subtree.
  - The top-level nodes the rows' kind-zero links name, with their subtrees.
- **Coverage:** these cover 93–99% of the world batches (`dr_t3`: 367 brick and 27 link batches of 397).
  The rest have no section and are always drawn: the start banner (a GDModel, e.g. #728 on `dr_t3`,
  #905 on `sh_t1`) and the backdrop dome.
- **Each frame:** a batch is drawn if it has no section, or if one of its sections is the player's
  section or one beside it.
  - With two players, both sets are drawn (provisional: the second tracker is not traced).
  - Pickups already followed this rule (`RaceSession::update_power_ups`).
- **Loop seam draws:** see the next section (replaces the distance rule and the ground fallback).

## The loop seam (traced the same day)

- **Group:** each node carries a group at `+0x104`, its section. In Wine on `dr_t3`, the bricks #21, #303,
  #391 and #126 have groups 0–3, and #391's child #392 has 2. A car's `+0x104` is its section too.
  A node with no section has 0xffff: the start banner #728 (GDModel, parent the world).
- **Two caches** (`0x420110`, `native-wmod-loading.md`): `+0x98` composes the hierarchy under an
  identity root, `+0xcc` under the root's local matrix.
  - At run time that matrix is the WMOD loop transform. Wine, `dr_t3`, root `045ab9d4`: rotation
    (0.009, 0, −1 / 0, 1, 0 / 1, 0, 0.009), translation (−6329.461, −0.589, 10225.567), the port's
    `wrap()`. The file's root has none.
- **Choice (`0x4200e0`):** a node's matrix is `node + [0x5472d0 + 4 × (group & 7)]` (flag 0x20 takes
  `0x405204`).
  - `0x432660` sets all ten entries to `0x98`.
  - `0x432690` (thunk `0x40441c`) rewrites entries 0–3 from `0x5472bc` (1 while drawing: `0x432710`
    sets it, `0x432970` clears it for the update) and `0x5472c0`. The tracker stores the player's
    section there at `0x4a0b99`.
  - Wine: player in section 0 → table `98 98 98 cc`; player in section 3 (the last) → `98 98 cc cc`.
    So: the last group when the player is in section 0, the last two when the player is in the last
    one, none otherwise. `0x4201f0` (the lazy path) has the same condition.
  - Group 7 (0xffff & 7) is never rewritten, so the banner always uses `+0x98`.
- **Hence:** a section is drawn in one place only, and nothing goes through the transform when the
  player is in another section. Wine on `sh_t1` agrees: with the tracker left on section 1 at the end
  of the lap, nothing was drawn past the road's end.
- **Native (`RaceView::draw_view`):**
  - The camera is in the player's group. From it, a batch, car or shadow whose group is marked
    differently goes through `wrap()` (camera unmarked) or `unwrap()` (camera marked); the others are
    drawn in place.
  - Batches with no section are group −1, never marked. Culling still applies.
  - In player 2's view, the group of player 2's section is used.
- **Checked:**
  - At the end-of-lap poses (player in the last section), the views equal the earlier version's to
    within 0.06% of pixels on `sh_t1`, `dr_t3`, `dt_t1`, `cc_t1` and `ct_t1`. That version was
    Wine-checked on `sh_t1` and `dr_t3`.
  - At `dr_t3`'s crane pose (section 0), only the robots differ.
  - Natively, with the player held at `dr_t3`'s end of lap, robots that have crossed into section 0
    are now seen past the banner (01:28, 01:53). Before, cars were never drawn across the seam.
- **Course objects and pickups** (`submit_object`) take the group of the node their batches come from.
  - At `dr_t3`'s end of lap, the WJunkyard crane (#2, linked from section 0) is now drawn behind the
    banner. Wine shows it there; the arm was at another point of its cycle.
  - The moving objects keep their node's group: `sh_t3`'s log ride (section 0, at x 176278), `dt_t1`'s
    crossing tram (2) and mover (1). The mover is inactive whenever its section is not enabled.
- **Not compared with Wine:** robots across the seam; the groups of linked nodes other than the
  crane, which are inferred from their links.

## Weapons and effects across the seam (traced the same day, statically)

Every writer of `+0x104` was listed (57 sites). Besides the section enabling, the tracker and the node
copy (`0x425bf0`, which copies the group of a cloned template), they fall into three groups:

- **Car effects** copy their car's group, with the subtree (`0x469ba0`, thunk `0x403a8a`):
  - WSplash `0x472d00`, WKBumm `0x472dc0`, WalkOnWater `0x47c680`;
  - WackyAutoBomba `0x47e220`;
  - the power-up actions `0x4d5510`..`0x4e2650`, e.g. FlyPower `0x4d55df`, from `[action+0x28]+0x104`.
- **Fired or thrown weapons** are registered with WModManager (`0x4a6b30`, thunk `0x402117`). The
  callers are WBreath's init `0x474e30`, `0x4990f0` (WPaintShot), WPU_Tnt `0x4dcac0` and WPU_Storm
  `0x4e15f0`.
  - The object goes into the manager's list `+0x60` (12 bytes: object, section, row; count `+0x44`,
    at most 0x7e). Its section and row come from the first of the player's three tracker slots whose
    box (`+0x8a8..+0x8bc`, the row's `+0x1c..+0x30`) holds its position.
  - Its group and its subtree's become that section. It returns −1 when no slot holds the object.
- **Each frame** (`0x4a1c40`, WModManager vtable `0x514b70` slot 2), for the objects past the robots
  (`0x4a2298..`) whose section and row are one of the player's slots:
  - `0x4a1a20` tests the position in the row's exit WPortal (`+0x50`): within ±`+0x122` across,
    within `+0x12a` deep and past the plane gives 1. The same with the entry portal (`+0x48`) gives −1.
  - On 1, the row moves on (to row 0 of the next section after the last one). The group follows
    (`0x4a2389`).
    - Into section 0, row 0 (`0x4a23c2`): flag `0x100000`, and the object's local matrix `+0x2c` is
      multiplied (`0x402c61`) by WMOD matrix `count + 3` (`+0x44 + 0x34 × i`).
  - On −1 the row moves back (`0x4a24f0`).
    - Into the last row of the last section (`0x4a25b5`): flag `0x200000`, and matrix `count + 5`.
  - Car-class objects (`0x4054b1`) also refresh their contact angles and `+0x288`.
- **Dropped weapons** go into the power-up table of the row whose slot box holds them (`0x4a6d20`,
  thunk `0x4050f1` → `0x49c890`, which sets the group). Leaving it (`0x49c960`) sets 0xffff.
  - The callers are WPU_OilSpill `0x4ddcf0`, WPU_BeeHive `0x4de9b0`, WPU_Landmine `0x4df670`,
    WPU_FallingTree `0x4e09e0`, WPU_BrickWall `0x4e2650` and WPU_Detour `0x4e33c0`, plus course
    objects (`0x4803c0`, `0x4c..`, `0x4d1a90`).
  - Being in the table, they are enabled and disabled with that row (`0x4a8680`, `0x4a8850`).

### Native

- **`RaceSession::carry_across_seam`:** projectiles, TNT and storm clouds cross the seam through
  `wrap()` or `unwrap()`. Their direction is turned too, and a storm's waypoint is moved.
  - The detection is **provisional**, the far robots' rule: no route under the object, but a route
    through the transform, from the last section or from section 0.
  - The original's portal test (`0x4a1a20`) is not ported.
- **`RaceView::effects`:**
  - Weapons are drawn by their section's group: projectiles, TNT, the storm (its billboard anchor is
    moved, then faced), mines, oil, hives, detour signs, trees and walls.
  - Splashes and ripples are drawn by their car's current section. The original copies the group
    when the effect starts; the two differ only if the car crosses the seam meanwhile.
  - Dropped weapons are culled with the enabled sections.
  - The skid marks (`0x47a7c0`) stay in place: they go through `0x40250e` with a matrix that was not
    identified.
- **Checks:**
  - `native_drive_test`, on every course: a missile fired 1500 before the last exit is in section 0
    on the 5th update and still flying 55 updates later. With the carry disabled, it crosses on no
    course.
  - On screen, `sh_t1` (a half-turn seam): from 2500 before the exit, the missile flies under the
    banner and into section 0's tunnel without a jump (native capture only).
- **Not compared with Wine.** Not ported: a dropped weapon outside every slot box keeps 0xffff and
  is always drawn in the original; natively it takes the dropper's section and is culled with it.
  A disabled dropped weapon's update was not traced either; natively it keeps updating.

## Slot rows and dropped weapons (2026-11-08)

### Recovered

- **The slots:** the manager keeps three slot records of 0x94 bytes per player from `+0x830`
  (six with two players, `0x5472b8+0x1a64`). Each holds:
  - `+0x0` the section, `+0x4` its route, `+0x8` the row index, `+0xc` the row;
  - `+0x10` a WMOD matrix, `+0x44` the inverse of the row brick's frame (`0x43c930`);
  - `+0x78..+0x8c` the box: the row's `+0x1c..+0x30` (min x, y, z; max x, y, z), copied by
    `0x4a89c0` (thunk `0x4032b5`) at the end of the enabling `0x4a8680`.
- **Order:** previous, current and next row. At the start (`0x49dc90`): the last section's last
  row, section 0's row 0, then the next row. A move back (`0x4a0880`) builds {previous − 1,
  previous, current} in the same order.
- **`0x4a6d20` (node, flag):** for each slot in order whose row has a brick (`+0x58`), the node's
  world x and z within the box (bounds included). On the first hit:
  - the node's frame is taken into the row (`0x402c61` with the slot's `+0x44`);
  - with the flag, `0x800000` is set on the node and its parent;
  - `0x40429b` files it in that row's power-up table and sets the group (`0x49c890`);
  - it returns section << 16 | row. With no hit it returns −1.
- **`0x4a6b30`** (a fired weapon's tracker) runs the same box test and takes the slot's section
  as the group `+0x104`.

### Native

- `RaceTrack::row_box_holds` reads the row box; `RaceSession::slot_section` tests the player's
  previous, current and next sections, then player 2's.
- Mines, oil, hives, detour signs, trees and walls take that section at the drop. A tree or wall
  filed in a slot keeps it at landing.
- Projectiles (WBreath and its kin), TNT and storm clouds take it at fire (`0x4a6b30`). The storm's
  field is then rewritten by its route polygon's section, as before.
- When no box holds the drop, the dropper's or thrower's section is kept (the original keeps 0xffff
  for a dropped weapon and registers no tracker for a fired one; not ported).

### Checks

- `native_drive_test`, every course: a mine dropped after 120 updates lies in a slot row's box, of
  the player's previous, current or next section, and takes it; a point off every box gives −1.
  All 20 pass.
- Scratch harness, points from each section's entry to its exit (21 per section): the previous
  row's box holds 0 to 16 of them per course, near the entry, where the boxes overlap.
  - `cc_t2`: 16 of 42; `dt_t4`: 8 of 55; `dr_t1`: 7 of 60; `ct_t1`, `dr_t3`, `sh_t4`: none.
  - A wall or tree filed there in the previous section misses the cars of the current one (the
    `0x4a7e70` section test), if the original does the same; not compared with Wine.
- Not compared with Wine.

## Compared on screen

- **`dr_t3`, end of lap 1, pose (11845, 4956), yaw 1.5708:** Wine shows pale blue buildings fading into
  the sky behind the junk piles past the banner. The new view draws them; the earlier ground rule did not.
- **`sh_t1`, pose (13300, −45378), yaw 1.5708, with Wine's tracker in section 2:** Wine shows a lit
  green cliff with a tunnel behind the banner. The new view matches; the earlier rule drew a purple
  cliff.
  - In an earlier hop chain, the tracker stayed on section 1 at this pose (steps of 2000 across the
    1→2 boundary at (22604, −11391)). Wine then drew nothing past the road's end.
  - That is the original's rule, traced below: nothing goes through the transform unless the player
    is in section 0 or in the last one.
- **Culling alone:** at the poses with the most hidden geometry within 20000 and 40° ahead, the views
  with and without culling differ in 0.01–0.2% of pixels. The hidden sections are behind hills or in the fog.
  The poses: `dt_t1` (626, 23135), `dr_t1` (−132567, 30495), `dr_t3` (12107, 12476) and the `dr_t3` grid.

**Not compared:** `cc_t1`, `dt_t1` and `ct_t1` across the seam (they changed by 0.6–1.4% of pixels
behind the banner); two players.
