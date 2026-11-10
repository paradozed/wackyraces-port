# WDicksTrick2, UVScroll, WPalAnim, APortal and ARoom (2026-11-07)

## WDicksTrick2 (cc_t3)

Vtable `0x515580`, init `0x4c7cd0`, update `0x4c7fe0`, choice `0x4c7e00`, end `0x4c7f40`. Three GDModel road pieces, members `m_88` (`0x236`, 2000) and `m_90` (`0x237`, 10000).

- **Choice:** `rand() % 3` (`+0x11e`). That piece clears flag 0x100 and keeps its frame; the others set it and sink 1000 (`0x512aa8`).
- **Update:** the query (`0x4a71e0`) ranges over `m_88`, or `m_90` once triggered (`+0x157`). A car sets the trigger; no car with the trigger set chooses again and clears it.
- **Route:** node 0x1c's first link, when it leads to node 0x1d (`+0x156`), weighs 0 for piece 0, else 0xffff.

Native: `DicksRoad`, with the chosen piece's GMDL triangles as ground (`log_ground`), as for WMisty's halves; the provisional reading that the sensor-group-2 primitive is the mesh is the same.

**Compared under Wine** (read at the start): section 3, fork set, chosen 1 with weight 0xffff, flags 0x108dd012 on the chosen and 0x108dd112 on the others, local y -381 against -1381. **Not compared:** driving onto a piece, or a falling car at a missing one; a redraw.

## UVScroll

Vtable `0x512734`, update `0x463e90`. Members `m_2c` (`0xea`), `m_34` (`0xeb`), and a GTEX reference (`0xe9`, not read by the update). On a model, each update adds `m_34` / texture size to its vertices: u over `+0x1c` with `m_2c` = 1, v over `+0x20` with 0. With `0x6f5958` = 0 it writes a texture matrix instead. Wine runs with `0x6f5958` = 1, the vertex path.

Native: `RaceView::animate_scrolls`, computed from the update count on static batches and course `WRot` models. Taken as width for `+0x1c` and height for `+0x20` (provisional). Actions on non-models (WGndBrick, WLogRide) have no batches of their own, as `0x404dea` rejects them (provisional reading).

Models moved out of the static world by other objects would not scroll: none is known to have the action except dr_t2's #986 (handled).

**Seen on screen, 2026-11-07 (`dr_t1`).** The action sits on the river models #357, #380, #382 and #383 (texture 59, 128 px, translucent, `m_34` = 5, v axis), plus GDWorld #1 (no batches of its own). The river is a drowning surface (`0x00800000`); the road passes it near (−108000, −4779, 35000), where the original's player was placed by hops along robot 1's line and native points (`WPC_PLACE=-108000,35000,-2.356`). The two framings match, with the water behind the fence on the right. In both builds the water's streaks move between captures 0.1 s apart. **Not measured:** the speed and direction (5/128 per update, about 1.17 texture per second at 30 Hz, so captures 1 s apart look almost alike); the original's frames were too small and the Wine camera drifted after the hops. `cc_t2`, `sh_t2`, `sh_t3` and `dr_t2`'s scrolls were not framed.

Native fix: the shift is now (units × updates) mod size in integers; the earlier `fmod` over `updates % 100000` jumped every 100000 updates.

Side note: the original's hops report section 3 at (−108000, 35000) where the native track says 2; not investigated.

## WPalAnim: nothing to port

Vtable `0x514f6c`, init `0x4bf600`, update `0x4bf670`. Past 2 updates it calls by `m_24` (`0x1fe`): 2 calls `0x4bf6b0`, which steps `+0x28` from 0 to 11 and back; 0 and 1 call empty routines (`0x4bf630`, `0x4bf650`). Nothing in the class reads `+0x28`. The instances I printed (`cc_t2`, `dr_t2`, `sh_t3`, first lines only) have mode 2. So no visible effect was found. **Limit:** I searched this class's own code, not every reader of an action's `+0x28`.

## APortal and ARoom: not ported

ARoom (vtable `0x5100a0`, `dt_t4` x20) holds a bounding box (`0x91..0x93`) and lists (`0x01`, `0x02`), APortal (`dt_t4` x32) a size and two room ids (`0x4..0x7`). A search finds their class ids (`0x2f17b17a`, `0x90187234`) only at registration, as `cmpl` immediates; other forms of comparison were not searched. ARoom has no update of its own (`0x41fff0` is the base's). They look like visibility data for the renderer, so skipping them should only draw more, not change the race. **That is a deduction:** their readers were not traced, and no capture of `dt_t4` was compared with the original.
