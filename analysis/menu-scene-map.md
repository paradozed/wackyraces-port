# Main menu scene map

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

This note records the structural route through the installed `menu.WGM` scene
that is needed to replace the screenshot-backed preview with WGM rendering.
It is based on bounded BRTR/CLAS/RSRC parsing, `wpc.exe` static analysis, and
the available Wine menu capture. Static findings below do not claim that every
backend or runtime state has been observed.

## Observed scene structure

- The top-level scene root is `GDStdBrick` object `0x1a`; it contains
  `GDStdBrick` object `0x145`, which contains a common visual group
  `GDStdBrick` object `0x1b`.
- That common group contains 11 `GDSprite` nodes, including two `320x111`
  textures (`GTEX 0x2` and `0x1`) at serialized positions `(0,0)` and
  `(160,0)`, seven smaller vehicle-sized textures, and two tall textures.
  The first seven positions are `(-280,28)`, `(-210,25)`, `(-80,22)`,
  `(60,20)`, `(120,16)`, `(170,23)`, and `(320,17)`.
- Three `GDFilm2` nodes are children of that visual group. They reference
  `FILQ 0x7cf`; their serialized queue indices are 0, 13, and 3. Queues 0 and
  13 each contain one frame with four placements, composing the static
  Satanas and Muttley character layers. Queue 3 contains 26 frames and
  replaces Satanas's head during the animation.
- A separate sibling `GDStdBrick` object `0x23d` contains `GDFilm` object
  `0x18`, linked to `FILM 0x7ce` -> `REEL 0x790`. That reel has nine material
  placements. Its serialized film properties include the `FILM 0x7ce` link;
  parent `0x23d` has an identity matrix and no explicit position fields. The
  nine placements span x=`-43..40`, y=`-46..40`. A decoded material sheet
  shows entries 0 through 6 are different driver portrait pieces and entries
  7 and 8 have zero-width textures. Placing the composition at Satanas's
  head anchor only duplicates his head and does not add the raised claw seen
  in the Wine capture. This sibling's runtime visibility remains unresolved;
  it is not drawn on top of the live animation. The decoded materials are
  preserved in `analysis/gdfilm-reel-790-materials-20261023.png`.
- `WMenuPage` object `0x4` contains the six menu items, two value items, and the
  eight menu text nodes. The `WMenuSprite` population belongs to other nodes
  and pages as well; drawing every one of the 252 nodes would include inactive
  UI states.

## Executable draw-path findings

- `GDSprite::Draw` is `FUN_0044d660` (instance vtable slot 3). It obtains the
  active graphics object through `DAT_005472b8 + 0x1838`, uses its viewport
  object at `+0xbc`, maps the X and Y object origins through virtual methods at
  `+0x38` and `+0x3c`, subtracts the inherited clip origin (`EBP`/`EDI`), then
  submits a textured-quad descriptor through the graphics object's slot
  `+0x44`. The coordinate mapping scales an offset coordinate by the viewport
  scale and divides by its logical extent (assembly at `0x00454130` and
  `0x004541a0`).
- The draw descriptor includes source placement coordinates, three tint
  channels, and float fields initialized to `1.0` and `0.0`. Their exact
  backend quad semantics and active backend implementation are still open.
- The native renderer keeps the banner textures at `320x111` and retains
  native pixel extents for the road cars. It maps their serialized origins
  through the active 2x viewport; four car origins lie outside the 640-pixel
  viewport and are clipped. The two banner tiles and seven vehicles are
  assigned layers by their serialized WGM object IDs. Character
  rendering now uses the three GDFilm2 queues instead of the two tall GDSprite
  cutouts. Each fragment destination is `2 * GDFilm2 object position + FRMQ
  local position`, with its decoded source dimensions unchanged. For queue 0
  this yields origins `(70,181)`, `(56,203)`, `(47,273)`, `(26,384)`; queue 13
  yields `(492,346)`, `(487,297)`, `(497,321)`, `(477,394)`. This rule matches
  the supplied capture substantially better than the old scale-fit cutouts;
  it is still awaiting a live GDFilm2 vertex capture.
- 32-bit TGAF raster bytes start at byte 38 (embedded TGA header plus ID
  length); they are BGRA and the fourth byte is alpha. The supposed key at byte
  38 is raster data, not a separate color-key field.
- Native `GDFilm2` rendering draws both four-fragment single-frame queues,
  then the selected 26-frame queue over Satanas. Applying FRMQ x/y directly
  as screen offsets placed the animated head in the top-left corner,
  contradicting the Wine capture. Mapping the node through the 2x viewport and
  adding its FRMQ local position now aligns the head with its static layer.
  This coordinate rule is visually checked but still awaits a live GDFilm2
  vertex capture. The selected GDFilm2 node's properties `0x5a`–`0x5d` are
  `2, 0, 24, 0.5`; static member registration and runtime offsets map these to
  mode, first frame, last frame, and speed. The port advances the mode-2
  accumulator at its 16 ms native scheduler interval. The original engine tick
  duration remains unknown, and frame 25 of the 26-frame resource is outside
  the configured playback bounds.
  Exact backend scale, clip origin, tint blending, and filtering still need
  validation against matching original frames.

The common group's background, road cars, three GDFilm2 queues, and page 4
labels are connected to the native preview. The raised arm is supplied by
`FILQ 0x7cf` sequence 12 (`REEQ 0x79d`), a 19-frame set of Satanas body/arm
fragments at the same `(66,60)` anchor. Its frame art matches the supplied
raised-claw pose. Six successive Wine menu captures show the arm extended in
the first image and lowered in the next five. The serialized scene has no
separate node for this queue, so the port layers it as an inferred startup
gesture alongside the explicit head animation, then removes the overlay after
an estimated 1,064 ms. A 30 fps Wine capture bounds the raised pose to about
1.0-1.1 seconds after the menu transition begins; the port uses a fitted 28 ms
gesture tick. The original update code, trigger, and timer origin remain
unrecovered. See
`analysis/menu-satanas-idle-comparison-20261023.png` and
`analysis/mainmenu-satanas-gesture-20261023.md`. The earlier raised-pose full
comparison is `analysis/menu-gesture-comparison-20261023.png`.

Main-page labels use
the live-captured 2x viewport mapping and 36-pixel row spacing; their X
positions are centered from decoded WGM glyph advances. The footer arrows use
the original WGM font glyphs. Character size and palette are closer after
using their serialized film compositions. Inspection of all 26 selected
GDFilm2 frames found one 79x72–76 placement per frame at the same local origin,
so this queue animates Satanas's head only; the separate sequence supplies the
arm gesture. Road sprites now move left at the rate measured in the Wine
sequence. The native six-second contact sheet
`analysis/menu-animation-times/native-road-review-contact.png` shows this
motion; matched samples 6–12 from Wine and 1–7 native have zero horizontal
translation preferred in the vehicle-colored pixels; see
`analysis/menu-road-phase-alignment-20261023.png`. The native road cycle now
uses a 1320-pixel wrap and matches the Wine capture's 8.233-second repeat within
one 30 fps frame. Exact menu-entry phase, the inferred 60-unit wrap margin,
and per-sprite edge-visibility rules remain unverified. WMenuSprite state and layout,
sibling GDFilm, page transitions, and menu actions remain incomplete.
Keep `analysis/wpc-mainmenu.png` as a comparison reference only; it must not be
loaded by the game at runtime.

The main page now clears to RGB `(25, 53, 90)`, measured at an unobstructed
field point in all twelve frames of the six-second Wine road capture and in the
separate matched 640x480 Wine menu frame. The earlier RGB `(16, 52, 90)` came
from the supplied reference PNG and does not match those runtime captures. An
older Wine screenshot with a black field is superseded by these later live
captures. The color is painted by SDL before WGM draws; the reference PNG is
not a runtime asset.

## Character/vehicle selection capture

The user-provided Wine screenshot of the French selection page is runtime
evidence, not a static mockup. It shows a purple patterned background, an
orange wooden title banner, and vehicle **DINGO-LIMOUSINE** with the subtitle
**PIQUE ET COLLEGARAMME**. The rendered vehicle/model occupies the left side;
the right side lists **VITESSE MAXIMALE — RAPIDE** and **TRACTION — BONNE**,
then an **UTILISER UNE ARME** row with an icon and an **ACTIONNER LE SUPER
GADGET** row with a second icon. The footer shows selection, Enter-to-accept,
and Escape-to-return hints. The client area is 640x480; the supplied image
also includes the Wine desktop title bar.

The C menu table calls page ID `28` the car page and ID `61` the multiplayer
car page. The `WMenuPage` object is `0x1c` (decimal 28), matching the
single-player car-page ID. It contains seven `WMenuCarItem` choices;
Dingo-Limousine is item `0xb3` with property `0x18c = 3`.

That item has four wheel GMDLs (`0x7aa`, `0x7ab`, `0x7ad`, `0x7ac`), each
with 24 vertices and 20 triangles. All use `GMAT 0x301`, which resolves
through `GTEX 0x5e` to a 256x256 TGAF atlas containing the vehicle parts.
Their child model matrices place them at the four corners of the car.

`general.wgm` also has a separate car group: `GDHModel 0x61` links to
`GHMS 0x102b5` (637 vertices, 494 triangles, `GMAT 0x1013e`); its parent
`GDModel 0xbf` contains four wheel meshes `GMDL 0x102b6` through `0x102b9`.
An isolated render with its serialized transforms and textures produces a
bright pink car with `P` badges, so this group is not the Dingo-Limousine
shown in the supplied selection capture. Its exact vehicle association is
unconfirmed; do not substitute it for Dingo's menu.WGM model group. The render
is preserved at
`general-model-0xbf-preview-20261023.png`.
`GHMS 0x102b5` has a `MTNL` animation link; its runtime pose and the menu's
camera conversion are not yet decoded. The linked motion resource is
`MTNL 0x10350` (1,564 bytes). Its serialized header words are `48, 2, 30, 86,
5`; after the 8-byte resource identity/class prefix, the count-driven arrays
occupy exactly 1,536 bytes: 20 bytes per first-array row, four bytes per
second-array entry, and 16 bytes per third-array row. The executable's
`FUN_00429910` vector reader consumes 12 bytes, so each third row is one u32
plus a 3-component vector. All 47 installed and disc MTNL resources end exactly
at the end of these arrays; the previously reported 946-byte extension was a
misread of the vector read size. The third-row vector semantics and animation
association remain open.

The native preview loads the body and four transformed wheels into the
perspective rasterizer. A previous guessed transform incorrectly treated Z as
the vertical axis; the renderer now preserves Y-up and rotates the vehicle in
the X/Z plane. The output remains visibly different from the Wine capture, so
this correction does not recover the scene camera, MTNL pose, or animation.

The purple pattern is `GMAT 0x2b3` -> `GTEX 0x12`, a 64x64 TGAF. The two
banner halves are page-child GDSprites at `(12,5)` and `(160,5)`, using
materials `0x309` and `0x30a`, each resolving to a 297x100 TGAF. The Dingo
item's icon sprites use `GMAT 0x2a8` (Dingo face, `(236,134)`) and `GMAT
0x2a9` (green wings, `(236,174)`); their textures are 96x64. Doubling these
sprite positions agrees with the screenshot's 640x480 placement. This scale
is an observed-fit inference, not a recovered backend invariant.

The Dingo text keys and properties are: `55` DINGO-LIMOUSINE at `(160,18)`,
alignment value `4`, height 15 and spacing 5; `173` PIQUE ET COLLEGRAMME at
`(160,32)`, alignment `4`, height 13 and spacing 5; `50` RAPIDE at `(234,60)`
and `52` BONNE at `(234,100)`, each height 13 and spacing 5. Page labels use
keys `45` VITESSE MAXIMALE at `(230,60)`, `46` TRACTION at `(230,100)`, `40`
UTILISER UNE ARME at `(230,140)`, and `39` ACTIONNER LE SUPER GADGET at
`(230,180)`. These are serialized observations; current text placement uses
a visual fit.

The native C renderer uses the actual purple TGAF, banner sprites, selected
car icons, French WGM-font labels, and all seven page-28 `GDModel`/`GDHModel`
groups from `menu.WGM`. It chooses the group in serialized `WMenuCarItem`
order, while property `0x18c` supplies a separate vehicle enumeration. The
localized card title/subtitle and traction/speed text keys are wired to that
same order. Model links and direct-child 3x4 transforms are summarized in
[`menu-vehicle-selection-native-20261023.md`](menu-vehicle-selection-native-20261023.md).
The native loader applies the recovered static HMTS/SEGS pose. Camera fit,
linked MTNL animation, and page-61-specific models/icons remain incomplete or
unverified.
`wpc.exe` evidence for sprite placement is the `GDSprite::Draw` path described
above. Class strings and registration references establish the scene classes
but do not determine the model camera or MTNL animation.
