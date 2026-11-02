# `WMenuSprite` property survey

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

A recursive scene-property survey of `menu.WGM` and `SWMenu.WGM` recorded
each `WMenuSprite` property ID, payload length, occurrence count, and sample
values. Full raw summaries are preserved in
`wmenusprite-properties-menu.txt` and `wmenusprite-properties-swmenu.txt`.

## Direct observations

- Both files contain 252 `WMenuSprite` objects, and every object has the same
  23-property layout.
- Property `0x9e` is an 8-byte `GMAT` tag plus a little-endian resource ID.
  There are 65 distinct material references in each file.
- Properties `0x9f` and `0xa0` are 32-bit values. Across the 252 objects,
  property `0x9f` has 7 distinct values and `0xa0` has 26.
- Property `0xa4` is the 32-bit word `0x00180042` in every object. Interpreted
  as two 16-bit halves, this is 66 by 24 pixels.
- Properties `0xa5` through `0xa7` each have two values: `0x80` and `0xff` in
  the low byte, with the other bytes zero. Their RGB/tint role is inferred from
  the matching `GDSprite` fields, not confirmed against original output.
- Property `0x1b` is a 48-byte identity matrix in all objects. Its use as a
  transform follows the convention inferred for other scene objects.
- Additional properties `0x1a`, `0x1f`, and `0xa1`–`0xa3`, `0xa8`–`0xaa`, and
  `0x184`–`0x189` are present on every object. Several vary between two values;
  their meanings are unresolved.
- The schemas match `GDSprite` placement/material properties closely enough to
  share the experimental preview path. The preview now collects both classes.
  On the installed full menu scene it finds 46 `GDSprite` and 252
  `WMenuSprite` nodes. An earlier experimental preview emitted pixels for all 298 nodes, including
  three referenced indexed textures with a footer-bearing 8/8 descriptor. Their
  placement and compositing were not validated against the original game, and
  the current native C port does not render these nodes.

## Placement status

The `0x9f` and `0xa0` values are candidates for position or frame-related
fields, but their small integer values alone do not identify their meaning.
The 48-byte identity matrix also does not establish whether the renderer uses
it directly, combines it with a parent transform, or bypasses it for menu
sprites. The supplied `analysis/wpc-mainmenu.png` establishes the intended
visible composition and rough screen positions. The separate Wine capture has
a black render field, and the experimental all-sprite preview has unresolved
texture colors and placement; neither verifies how these serialized fields
produce the reference image. Static disassembly notes cover the menu text path
and page selection methods but have not identified a sprite draw call site.
Keep the experimental preview diagnostic until the draw path or comparable
runtime frames establish the mapping.

## Limits

These observations do not explain the extra state fields, page visibility,
animation/frame selection, scene-transform application, draw order, or the
original pixel layouts. The Wine capture remains black and cannot validate the
screen coordinates or colors. The property report is structural evidence, not
a complete `WMenuSprite` implementation.
