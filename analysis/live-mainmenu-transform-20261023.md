# Live main-menu viewport and text transform

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

This note records Wine/GDB values observed from the installed WPC executable
(`wpc.exe`, SHA-256
`e770a5ea14d19f8d9b3692645f22e6ea83468dc0e14565b7dc7e45cf3640850b`). The
program was run from the configured temporary copy described in
`runtime-capture.md`; the installed executable and assets were not modified.

## Runtime observations

- At `FUN_0042de80`'s glyph loop, the first character of `DEMARRER` had a
  `WMenuText` logical position `(160,70)`. The mapped glyph cursor was
  `(246,140)`. The footer cursor was `(130,440)` for logical Y=220. The eight
  main-label cursor Ys advance exactly `36` pixels per WGM item row.
- At the glyph loop, local `[ESP+0x10]` was zero. This is the active clip
  origin subtracted from the mapped position, so the main menu text has no
  clip-origin displacement in this run.
- The live object chain was
  `DAT_005472b8 + 0x1838` (active graphics object), graphics object `+0xbc`
  (viewport), viewport's first child (view transform), then that transform's
  `+0x10` child for X and `+0x14` child for Y.
- The active X transform at `0x00454130` reported logical extent 320 at
  transform `+0x122`, offset 0 at `+0x142`, and output extent 640 from the
  graphics object's virtual method `+0xa4`. Its mapping is therefore
  `((x + 0) * 640) / 320`, yielding the observed 2x scale.
- The Y counterpart at `0x004541a0` is structurally parallel. Direct runtime
  observations confirm `70 -> 140`, each 18-unit row step -> 36 pixels, and
  `220 -> 440`; the resulting main-menu text also has zero clip-origin
  subtraction.
- Static inspection of the active text renderer at `0x0042de80` shows the
  main-page labels use centered alignment around mapped page X=320. Their
  cursor starts are calculated from the WGM glyph advances, not individually
  serialized screen anchors. `wpc_menu_draw` now derives these X positions
  from the loaded glyph widths using that rule.

## Glyph destination recovered (2026-10-23)

A live breakpoint at the queue insertion path captured the descriptor for the
first `D` in `DEMARRER`: texture `0x04661d5c`, cursor `(246,140)`, size
`22x22`, and signed descriptor offsets `(+2,-6)`. The corresponding WGM font
entry in `analysis/menu-font-map.txt` gives bearing `(-2,+6)` and the same
dimensions. Static inspection of `FUN_004af1a0` in
`analysis/decompilation/wpc/functions-036.c` shows the destination quad starts
at `cursor - descriptor_offset`; therefore the final glyph origin is
`cursor + WGM_bearing`. The C port had incorrectly subtracted the WGM bearing
on both axes and then compensated the main labels with a seven-pixel fit.
`wgm_anim.c` now adds both bearings, and `menu_render.c` uses the captured
cursor anchor directly without the extra fit.

## Port implications and remaining gap

The logical-to-client viewport scale, main-page clip origin, and glyph bearing
rule are recovered in the native renderer. An initial controlled capture found
the native label masks two pixels above the Wine masks. Removing that fit
correction produced a best integer overlap at zero X/Y translation for every
label in a fresh 640x480 comparison. The native renderer now uses the cursor
anchors and signed WGM bearings directly, without a backend raster offset. See
`menu-user-reference-mask-audit-20261023.md` and
`menu-current-capture-comparison-20261023.png`. The footer uses measured cursor
anchors and its own horizontal scaling/advance settings.

The menu renderer's navy clear color, car placements, character animation
anchor, and sprite scales are separate visual fits; this viewport capture does
not establish those values.
