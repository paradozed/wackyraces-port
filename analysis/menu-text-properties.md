# Main-menu text properties

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

The user-supplied `analysis/wpc-mainmenu.png` is a 640x480 French main-menu
reference. Its eight visible labels match the language-3 strings linked from
the `WMenuText` objects under `WMenuPage` object `0x4` in `menu.WGM`. This
supports identifying page `0x4` as the main menu in this installed resource
set. The image is not a capture from the Wine run described in
`runtime-capture.md`.

## Observed resource values

A prior scene inspection recorded raw properties for `WMenuText`,
`WMenuItem`, and `WMenuSetValueItem` nodes, in addition to sound-node data. Inspecting the eight text objects (`0x7`, `0x26`,
`0xff`, `0x116`, `0x226`, `0x223`, `0x376`, and `0x21d`) shows:

- Property `0x4a` holds the expected string keys: 14, 15, 16, 17, 235, 153,
  245, and 244.
- Property `0x35` links each object to `FILM 0x7d1`.
- Property `0x36` is `42 00 18 00` for all eight, or little-endian values 66 and
  24.
- Properties `0x37` through `0x44` have the same values on all eight objects.
- Their property `0x1b` transforms are identity. The corresponding parent
  `WMenuItem` transforms are also identity.

These shared values do not describe the distinct label positions visible in
the screenshot. In particular, `0x36` is not treated as a screen coordinate;
its 66x24 interpretation is only the direct decoding of two little-endian
16-bit values. The runtime layout mechanism, selected-item tint, and relation
between text-node properties and final placement remain unknown.

## Main-page layout properties

A bounded BRTR walk of `menu.WGM` reveals layout data on main `WMenuPage`
object `0x4`: property `0x197` is 160, `0x198` is 70, `0x199` is 27, and
`0x19a` is 2. The registrations at `0x0048b180`–`0x0048b2a0` name the
corresponding page members `m_nPosX`, `m_nPosY`, `m_BackGroundBrickId`, and
`m_nHelpTextType` in the same order. This is a strong member/property
association, not a captured runtime object. Six direct `WMenuItem` children
(`0x5`, `0x9`, `0xfe`, `0x115`, `0x21c`, `0x375`) each serialize properties
`0x18a = 0` and `0x18b = 18`; the page iteration path below uses these values
as per-item coordinate advances.

The main-page renderer uses a 640x480 layout: center X=320 and row step=36
pixels. The C port reads page X/Y and item advance values from the WGM and maps
them through the observed 2x viewport scale. Thus page X=160 yields center
X=320 and item Y advance `18` yields a 36-pixel row advance. The live capture
resolved the client transform: this page uses 2x scale from 320x240 to
640x480, with no clip-origin subtraction.

Static analysis recovers the row-layout call path. WMenuItem's vtable at
`0x00513550`, slot `+0x40`, points through thunk `0x004057a9` to
`FUN_00488c70`. That routine walks child nodes, assigns WMenuText positions
from two input coordinates, and advances those coordinates using child text
fields at `+0x1da`/`+0x1e2`. It also assigns positions to WMenuSetValueItem
children. Its caller `FUN_0048bab0` reads the page's coordinate values at
`+0x122/+0x12a`, walks WMenuItem children, invokes slot `+0x40` with the
current coordinates, then advances by each item object's `+0x122/+0x12a`
values. The WGM property order maps page properties `0x197/0x198` to
`m_nPosX/m_nPosY`, and item properties `0x18a/0x18b` to those X/Y values;
this mapping is supported by registrations, constructors, and the caller's
field accesses. This confirms that page Y=70 and item Y=18 drive the original
logical layout. The active viewport, clip origin, and glyph destination/bearing
semantics have since been observed live. See
`analysis/disassembly/wpc-menu-item-layout.txt`.

A Wine/GDB runtime capture at the glyph loop (`0x0042e0a6`, first character of
each string) observed the eight main-menu glyph-loop cursors as `(246,140)`, `(265,176)`,
`(209,212)`, `(174,248)`, `(281,284)`, `(140,320)`, `(266,356)`, and
`(265,392)`. The footer begins at `(130,440)`. These cursor positions are
before signed glyph placement. A live capture recovered the viewport mapping
from logical 320x240 to client 640x480, with zero transform offset and zero
clip-origin subtraction for page-4 text. At `DEMARRER`, WMenuText logical
position `(160,70)` maps to cursor `(246,140)` after centering; every 18-unit
row step becomes 36 client pixels. See
`analysis/live-mainmenu-transform-20261023.md` for runtime addresses and
values.

The native renderer now centers labels using WGM glyph advances rather than
fixed French X anchors. A live capture of the first `D` in `DEMARRER` showed
cursor `(246,140)`, texture size `22x22`, and descriptor offsets `(+2,-6)`.
The sprite renderer subtracts those descriptor offsets, so the bitmap origin
is cursor plus the signed WGM bearing `(-2,+6)`. The C port now applies that
rule directly and no longer adds the former seven-pixel conversion. Its main
label cursor anchors are `(246,140)`, `(265,176)`, `(209,212)`, `(174,248)`,
`(281,284)`, `(140,320)`, `(266,356)`, and `(265,392)`; each glyph's own Y
bearing can make its visible top edge differ slightly within a row. The footer
uses cursor Y=440 and cursor X positions 130, 274, 298, and 332; arrow glyphs
are WGM bytes `0x7f` and `0x7e`. The footer currently uses 0.75 horizontal,
0.62 vertical texture scales and 1.11 glyph advance. Updated native capture:
`analysis/native-mainmenu-bearing-20261023.png`; side-by-side comparison:
`analysis/menu-text-bearing-comparison-20261023.png`. Binary word masks from
the Wine and native halves were compared over integer translations from -5 to
+5 pixels on each axis. All eight labels had their strongest overlap at
`(dx=0, dy=+2)`: the earlier native frame was uniformly two pixels too low.
That correction matched its specific Wine capture. A later fresh build was
captured against the exact 640x480 reference `wine-mainmenu-20261022.png` with
`DEMARRER` selected in both. That run found the current native text one pixel
left and two pixels above Wine. The page-4 renderer now adds one pixel to the
centered X origin and uses the recovered cursor Y directly, removing the old
two-pixel Y correction. The fresh comparison has its strongest mask overlap at
`(0,0)` for all eight labels (IoU 0.877–0.934). See
`analysis/menu-current-capture-comparison-20261023.png` and
`analysis/native-mainmenu-current-20261023.png`. Road and character animation
phases are not synchronized, so this comparison establishes text placement
only. Footer width and animated selection behavior still need comparison.

## Per-glyph vertical check

The controlled comparison image was also segmented into connected glyph-mask
components within each label's bounding box. For each native component, the
best vertical overlap against Wine was checked over shifts from -4 to +4
pixels. Every matched component in `DEMARRER`, `OPTIONS`, `CHARGER PARTIE`,
`SAUVEGARDER PARTIE`, `DEMO`, `CREDITS`, and `QUITTER` preferred `dy=0`;
27 of 28 components in the long `SEQUENCE D'INTRODUCTION` row matched at
`dy=0` (one component had no above-threshold match). This older component
audit gives no evidence for glyph-specific Y offsets. In the latest exact-size
pair, all eight row masks also match best at `(0,0)` after the global X/Y
placement update above. The tiled screenshot's alternating apparent letter
heights do not reproduce in either controlled comparison; do not tune
individual bearings from that screenshot alone.

The coordinate helpers used by the glyph path are now understood algebraically.
`FUN_00454130` maps X and `FUN_004541a0` maps Y. The live viewport wrapper
selects per-axis transform objects at `+0x10` and `+0x14`; each helper obtains
the output extent from the active graphics object's virtual slot `+0xa4`,
adds the axis offset at transform `+0x142` (X) or `+0x14a` (Y), multiplies
the input by the output extent, and divides by logical extent at `+0x122` (X)
or `+0x12a` (Y). In shorthand, each axis maps as
`((logical + offset) * output_extent) / logical_extent`; callers then subtract
the active clip origin. The live page-4 state uses a 640x480 viewport over
logical 320x240, with zero X/Y transform offset and zero text clip origin. Main
page Y values map directly to 140, 176, ..., 392; footer logical Y=220 maps to
440. These observations resolve the viewport and clipping questions for this
menu page. The unresolved part is how the backend applies the descriptor's
signed glyph placement and clips the bitmap.

A prior structural survey recorded payload shapes and distinct values for all
`WMenuText` properties. In `menu.WGM`, all 204 text objects have 4-byte values
for properties `0x4a`–`0x4e`; the count of distinct values is 105, 3, 3, 1, and
3 respectively. `SWMenu.WGM` has 192 text objects with the same property shape
and distinct counts 103, 3, 3, 1, and 2. This across-scene evidence is
consistent with the ordered member association `m_TextID`, `m_Align`,
`m_Height`, `m_Space`, `m_Narrow`, but does not by itself prove the runtime
class-table mapping. In the eight main-menu labels, properties `0x45` and
`0x46` are both zero, so those fields do not explain the different label
positions there. Full raw summaries are preserved in
`analysis/menu-text-property-summary.txt` and
`analysis/swmenu-text-property-summary.txt`.

The summaries also show six 4-byte properties at `0x17e`–`0x183` on every
text object in both files. Properties `0x17e`, `0x17f`, and `0x180` only take
the values 0 or 1 across these resources, making them boolean-shaped
candidates for the registered `m_bMustPosition` member. The other three
properties have small integer values as well. All eight main-menu labels share
the same sequence `0, 1, 0, 0, 0, 4`, so these nodes do not distinguish which
property, if any, selects `m_bMustPosition`. This is a value-shape lead, not a
member mapping.

## Static executable lead

Radare2 references the `WMenuText` class-name string from the registration
routine at `0x004871e0`. That routine stores a class descriptor with vtable
pointer `0x00513398`, class name `WMenuText`, and descriptor value
`0xeaea0002`. Its allocation wrapper at `0x00487230` requests `0x1f2` bytes
and calls the constructor at `0x004875f0` through thunk `0x00404953`.
The constructor calls a base initializer, sets up three fields with the helper
at `0x00405236` (object offsets `+0x1be`, `+0x1c6`, `+0x1ce`), installs
several pointer values at `+0x1d6`, `+0x1de`, `+0x1e6`, and initializes fields
at `+0x1ea` and `+0x1ee` to 4 and 10. These are observed writes; their relation
to serialized property IDs and their rendering meaning are not established.
The focused disassemblies are preserved in
`analysis/disassembly/wpc-menu-text-registration.txt` and
`analysis/disassembly/wpc-menu-text-constructor.txt`.

The BRTR loader follows class-specific member descriptors to deserialize each
property. In the main-menu `WMenuText` nodes, properties `0x4a`–`0x4e` strongly
correspond in order to executable registrations for `m_TextID`, `m_Align`,
`m_Height`, `m_Space`, and `m_Narrow`. Property `0x4a` varies with the string
key; the other four values are consistently 4, 13, 5, and 0 across all eight
labels. The class-table descriptor contents were not captured, so this is a
strong registration/order/data association rather than a runtime memory dump.
The exact draw effects of the four layout settings remain unverified. See
`analysis/wpc-brtr-property-loading.md` for the loader path and mapping
evidence.

The `WMenuText` vtable at `0x00513398` points through a thunk to the update
method at `0x00487950`. It reads a mode at object offset `+0x1c2` and flags at
`+0x1ea`. In the mode-0 branch it copies the associated object's runtime
coordinates at `+0x98`, `+0xa0`, and `+0xa8` into the text position fields at
`+0x132`, `+0x13a`, and `+0x142`. The mode-1 branch either animates x/y within
20–240 using the step at `+0x1ee`, or sets x/y from screen-center globals.
Another branch sets the position to `(128,128,128)` or `(64,64,64)` from a
global flag; special bits in `+0x1ea` can override it with `(255,90,0)` or
`(0,255,0)`. These values are written to position fields; interpreting them as
color would be unsupported. The `WMenuText` class separately registers a
member named `m_bMustPosition` with a candidate field offset of `+0x15c` and an
additional numeric metadata argument `0x1c6`. That name suggests a positioning
role, but it has not been mapped to a BRTR property or to the update method's
mode at `+0x1c2`. The routine then calls `0x0042de80`, which iterates
the text bytes. Control bytes below `0x20`, and ASCII space (`0x20`), take the
fixed-advance path and add 10 to the integer cursor without drawing a glyph;
bytes above `0x20` are reduced by `0x20` at `0x0042e0b6` to index 12-byte glyph
placement records. In particular, the space entry's 16x4 TGAF is not rendered
as a dash. The native renderer now follows this behavior in both drawing and
width calculation; a fresh capture of the current native build again shows
`DEMO` without a leading mark (`analysis/native-mainmenu-fresh-20261023.png`).
The localized string is exactly `DEMO`, confirmed by bounded WGM string
inspection. The later tiled comparison has 639-pixel client widths and
different highlighted rows, so it does not establish a new font offset. For
each menu-frame draw in the 640x480 path, the method
temporarily subtracts 6 from the laid-out Y coordinate at `+0x1b2` before
calling the glyph renderer, then restores it. A different viewport branch
subtracts 126. The glyph renderer maps this coordinate through viewport slot
`+0x3c` and subtracts the active clip origin before placing glyphs. The meaning
The observed main-menu run uses the 640x480 viewport branch, with 2x scale and
zero clip origin. The alternate branch's runtime meaning remains unresolved.
For a glyph, the stored signed x/y offsets are negated and combined with scale
before the draw call. At `0x0042deb0`, the renderer loads `ebp` from
`[esi+0x18]`. At `0x0042e1de`, the next horizontal cursor coordinate is
computed from `(glyph texture width - [ebp+0x150] - 1) * [esi+0x152] *
[esp+0x20] + current cursor`, then converted to an integer by `0x004ee040`.
Thus trim comes from the object referenced by the text object's `+0x18`
pointer; one scale is on the text object and the second is passed in the
renderer call context. The renderer also reads other fields on the referenced
object (including `+0x138` and `+0x140`). The constructor delegates initialization
to a shared base thunk, but the target class and serialized-property mapping
have not been established. The helper `0x004ee040` temporarily sets x87
rounding control to truncate toward zero before `fistp`; its disassembly is
preserved in `analysis/disassembly/wpc-float-to-int.txt`.

The common object initializer at `0x0041e2f0` sets `+0x18` to null. The BRTR
parser's `PRIM` child path loads a container pointer from the parent object's
`+0x18` and inserts the child into its list; links are held at child offsets
`+0x10` and `+0x14`. Separately, generic factory helper `0x0041b180` stores its
explicit parent argument in a newly created object's `+0x18`, and its caller
links that object into the parent's list. This provides a possible assignment
mechanism for a leaf object's parent pointer: zero serialized children only
means the text node has no children of its own. The factory helper is present
in WMenuNode and the nearby 0xa4-byte object's vtables, but the exact path that
creates WMenuText and the parent object's class are not yet proven. Focused
disassembly confirms the base constructor writes null at `+0x18`, the
`WMenuText` constructor does not overwrite it, and the inspected `WMenuText`
update method only reads it. See `analysis/disassembly/wpc-object-child-link.txt`,
`analysis/disassembly/wpc-menu-text-base-constructor.txt`, and
`analysis/disassembly/wpc-menu-text-update.txt`.

The BRTR `CHBR` callback at `0x00404be7` reaches `0x0041fae0` and adds the
child to the parent's `+0x24` list. It does not assign the child's `+0x18`.
The `PRIM` child path is separate: it loads its list container from parent
`+0x18`. Thus ordinary `CHBR` attachment does not explain the text renderer's
reference pointer; the explicit-parent factory path remains unconnected to
WMenuText construction.

The `+0x24` virtual method in the WMenuText, WMenuItem, and WMenuPage vtables
is a transform accessor/copy path (`0x0041eef0`), not that factory. It selects
a transform record and copies three dwords from its `+0x28` area to an output
structure. This
rules out interpreting that shared menu-class slot as the missing context
assignment. See `analysis/disassembly/wpc-object-child-link.txt` for the
instruction-level path.

A scoped scan of direct stores to object displacement `+0x18` in the
`0x00487000`–`0x00488fff` class-code range found no assignment in the
`WMenuText` registration, constructor, or update implementation. The nearby
matches at `0x00484eae` and `0x00485119` belong to unrelated object
initialization and version parsing. This only rules out a direct class-local
store in that inspected range; an indirect helper or generic scene-tree setup
can still populate the field. The next trace target is therefore the runtime
tree adoption path.

This recovers the cursor formula and runtime position-selection branches, but
not which serialized fields select those branches, how runtime setup fills
`WMenuText +0x18`, the serialized meanings of trim and scale, or any tint
handling. An earlier experimental font preview applied the observed glyph
lookup/offset and cursor rules to byte input, coordinates, scale, and trim, but
it did not instantiate or position a `WMenuText` scene object. The native C
port renders the page-4 labels from the decoded WGM font and localized strings.
The sample `DEMARRER` preview is `analysis/font-run-demarrer.ppm`.
Disassembly is preserved in
`analysis/disassembly/wpc-menu-text-update.txt` and
`analysis/disassembly/wpc-menu-text-render.txt`.

The observed byte-to-index mapping applies to `FILM 0x7d1 -> REEL 0x7a3`. It lists byte code, glyph index, signed
placement offsets, GMAT ID, and the referenced TGAF dimensions/format. For this
resource it reports 102 entries, covering bytes `0x20` through `0x85`; this
connects the executable's lookup rule to the installed glyph-placement table.
The captured output is in `analysis/menu-font-map.txt`. An earlier diagnostic
rasterized explicit byte sequences using the recovered cursor formula, but
WMenuText node layout remains unimplemented in the C port.

The native C animation loader now reads and bounds-checks the 102 placement
rows in `REEL 0x7a3`, resolves each `GMAT` to its TGAF glyph surface, and
exposes a byte-string drawing API. The API takes explicit cursor position,
trim, scale, and tint because not all `WMenuText` render-context values are
mapped to serialized properties. Page-4 text integration and comparison
against the Wine frame are complete; other pages and selection animation
remain open. Eight known
zero-width placeholders have 0x106 indexed TGAF dimensions; the loader keeps
their placement/advance entries without trying to allocate a zero-width SDL
surface. A headless native-menu launch confirmed the film and glyph textures
load after this case was handled.

The French menu string key `175` contains bytes `PENELOPE JOLI C\x80UR`.
Glyph byte `0x80` (placement index `0x60`) is visibly an uppercase `Œ`-like
ligature in the installed font sheet, matching French “CŒUR”. The
The C text inspector preserves
the escaped bytes. The glyph-to-Unicode match is inferred from string context
and bitmap shape; it is not a general character-set definition. See
`analysis/wpc-strings.md`.

## Next analysis

- Confirm the five text-member/BRTR property associations against runtime
  class-table descriptors; trace whether factory helper `0x0041b180` assigns
  the parent pointer at `WMenuText +0x18` during scene loading. The base
  constructor sets it null, the text constructor leaves it unchanged, the
  inspected update method reads it, and the renderer dereferences it. A text
  node's zero child count does not rule out its own parent pointer. Identify
  the call path and target class, then map position, trim, scale,
  tint, and selection state to scene properties. The cursor formula's trim and
  two scale sources
  are located in memory/call context but are not mapped to serialized fields.
- The French menu byte `0x80` is mapped to `Œ` in diagnostic string output
  based on the installed glyph sheet and string context. Full character-set
  coverage remains open; page-4 cursor positions and glyph origins are
  validated against a Wine-captured frame.
- Compare animation frames and draw order with matching selection states; the
  current text comparison establishes label centers and row heights only.

### Serialized parent hierarchy evidence

A recursive walk of the installed `menu.WGM` BRTR tree found these immediate
serialized parent classes for its 204 WMenuText nodes: WMenuItem 71,
WMenuSetValueItem 20, WMenuPage 34, WMenuCarItem 56, WMenuSetVolumeItem 5,
WMenuControllerItem 16, and GDStdBrick 2. Main-menu text object `0x7` is nested
under WMenuItem object `0x5`, which is under WMenuPage object `0x4` (itself
nested under GDStdBrick objects `0x145` and `0x1a`). These are serialized BRTR
parent relationships, not direct observations of runtime `+0x18` values.

The generic factory helper `0x0041b180` can initialize a new object's `+0x18`
from an explicit parent argument, so the serialized hierarchy offers a
possible route for populating the text pointer. However, the helper is reached
through `0x0041e2a0`, a method present at vtable +0x24 on WMenuNode and the
nearby 0xa4-byte type. WMenuPage and WMenuItem use another method at that slot.
No call path yet shows the helper creating a WMenuText instance. The renderer
reads through referenced-object `+0x150`, while WMenuPage allocates `0x13a`
bytes and WMenuItem allocates `0x12e`. These common serialized parent classes
are too small for that direct read, so the runtime target may be a separate or
larger context object. The call path and target remain to be traced. The parser's post-child finalizer at `0x0041f6e0` still does not
write `+0x18`.
