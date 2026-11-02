# Native vehicle-selection page

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

The C port now selects the vehicle preview from the seven `WMenuCarItem`
children serialized under `WMenuPage` object `0x1c` in `menu.WGM`. The table
below records their order in the BRTR child list, the value of property
`0x18c`, the text keys and model links. Model links and 48-byte transforms
were decoded from each item's `GDModel`/`GDHModel` descendants. The renderer
uses those model resources and material links from `menu.WGM` itself.

| Order | Item | Property `0x18c` | Card title key | Subtitle key | Traction / speed keys | Body / other meshes |
|---:|---:|---:|---:|---:|---:|---|
| 0 | `0x9d` | 6 | 172 | 54 | 53 / 53 | `GHMS 0x7a5`; `GMDL 0x7a6`–`0x7a9` |
| 1 | `0xb3` | 3 | 55 | 173 | 52 / 50 | `GHMS 0x7ae`; `GMDL 0x7aa`–`0x7ad` |
| 2 | `0xc5` | 5 | 174 | 56 | 53 / 49 | `GHMS 0x7b0`; `GMDL 0x7af`, `0x7b1`, `0x7b2` |
| 3 | `0xd5` | 1 | 175 | 57 | 52 / 51 | `GHMS 0x7b3`; `GMDL 0x7b4`–`0x7b7` |
| 4 | `0xe6` | 2 | 176 | 58 | 52 / 49 | `GHMS 0x7bc`; `GMDL 0x7b8`–`0x7bb` |
| 5 | `0xf7` | 4 | 177 | 59 | 49 / 53 | `GHMS 0x7bd`; `GMDL 0x7be`–`0x7c1` |
| 6 | `0x108` | 0 | 178 | 60 | 49 / 53 | `GHMS 0x7c6`; `GMDL 0x7c2`–`0x7c5` |

The first row is the locked character: its language-3 label is
`SATANAS ET DIABOLO`, and its serialized vehicle ID is `6`. The other six rows
map to the currently playable character/vehicle pairs below. A fresh native car page
still exposes all seven rows; it does not yet reproduce WPC's reported lock on
Satanas and Diabolo. This identifies which row needs gating, but not the
condition: the save prefix's five candidate championship bytes have not been
mapped conclusively to this character unlock. The reported unlock after
championship completion must not be implemented by guessing a save-byte index.
The inspected `WMenuCarItem` factory (`FUN_00489140`) and initializer
(`FUN_004892f0`) only allocate the `0x9c`-byte object, run the base
initializers, and set its vtable/selection field; they show no lock check.
Likewise, the decompiled references to the five-byte save block at
`DAT_006f4668` are file reads and writes only. The lock condition is not
present in these recovered paths, so a runtime trace of the original
character-select accept action and the corresponding profile state is needed
before adding native gating.

The current French text table maps key 55 to **DINGO-LIMOUSINE** and key 173
to **PIQUE ET COLLEGRAMME**. Keys 50, 52, 53, 49, and 51 provide the speed
and traction values. The UI order follows serialized child order and is
different from the `0x18c` vehicle enumeration, so the renderer does not use
that property as the menu index.

The native text reader now groups each page-28 `WMenuText` record by its
owning `WMenuCarItem` and reads the serialized string key and height property
`0x4c`. Each item has four records. The first two are its paired names and the
last two are its displayed traction and speed values. For item `0xb3`, the
name keys serialize as 173 then 55, while their heights are 13 and 15; height
therefore selects DINGO-LIMOUSINE (55) as the title and PIQUE ET COLLEGRAMME
(173) as subtitle. For the other six items, the two names have equal height
and their serialized order matches the previously recorded card mapping. This
replaces the hardcoded seven-row detail-key table in the page-28 runtime path.
The inspector now shows each text node's owning menu item and height. Page 61
has the same broad four-text grouping but is not yet used for independent
multiplayer card details.

## Vehicle enumeration and handling response

WPC registers `m_nVehicleId` for `WMenuCarItem` at `0x004892b0`, with runtime
field offset `+0x94`. Its seven serialized `0x18c` values are therefore
executable-backed vehicle IDs, in menu order `[6, 3, 5, 1, 2, 4, 0]`.
Separately, `WackyAuto` registers `m_iVehicle` at `0x00464580`, field offset
`+0x84`. Its constructor `FUN_004660b0` selects one of seven handling-response
constants from the integer at its linked vehicle object's `+0x88`. The native
port now uses each car's serialized `m_nVehicleId` to select that response.
The native WGM scene walker now reads each `WMenuCarItem`'s `PRPS 0x18c`
value, validates that all seven IDs are present, unique, and in the `0..6`
range, and retains the value with the matching vehicle asset. It also collects
all linked `GMDL`/`GHMS` descendants and their serialized 48-byte transforms,
then builds the preview mesh groups from those records in serialized child
order. The loader no longer depends on fixed car-item object IDs. The race
handling response lookup receives the parsed ID rather than using a
separate hard-coded menu-order conversion. This is still not proof that
`WackyAuto.m_iVehicle` uses the same enum: the actual runtime assignment from
`WMenuCarItem.m_nVehicleId` to `WackyAuto.m_iVehicle` has not been captured.
See
[`disassembly/wpc-directinput-state-20261023.md`](disassembly/wpc-directinput-state-20261023.md).


### Runtime-link trace status (2026-10-23)

I checked the registration sites and the available decompilation around the
menu-item factory and constructors. The menu-item factory at `FUN_00489140`
allocates a `0x9c`-byte item and calls `FUN_004892f0`; the latter installs the
menu-item vtable. Neither function reads `m_nVehicleId` or creates a
`WackyAuto`. Separately, factory `FUN_004643e0` requests class ID `0x953` from
the runtime class factory and initializes a `WackyAuto` through
`FUN_004660b0`. The `m_iVehicle` registration establishes its own field
offset, but the available static path does not show the selected menu item
being passed to that factory or copied into the race object.

A candidate executable reference at `0x00480923` reads `[ecx+0x94]`, but the
surrounding code operates on transformed 3D vectors and the decompilation does
not identify `ecx` as a `WMenuCarItem`. It is therefore not evidence of the
menu-to-race assignment and was not used to change the native mapping. To
resolve the relationship, the next useful trace is the event/page transition
that starts a race, following its selected menu item through object creation
and into the `WackyAuto` constructor. Until that path is identified, the native
mapping stays explicitly provisional.

### Follow-up reference audit (2026-10-23)

I checked direct code references in the installed executable after identifying
the unresolved handoff. `0x004892b0` is a reflection-registration stub: it
passes field offset `0x94` and the string `m_nVehicleId` to the common property
registrar. `0x00464580` independently registers offset `0x84` under
`m_iVehicle`. This confirms two distinct reflected fields, but the names and
offsets do not establish a shared enum.

The class factory `0x004643e0` allocates class ID `0x953` and invokes the
`WackyAuto` constructor. Its only direct executable reference is a jump thunk
used by generic class dispatch; the constructor itself has no direct call from
the menu-item factory. The selection factory at `0x00489140` creates a
`WMenuCarItem`, while the race actor is instantiated through the separate
class factory. This reference audit finds no statically direct menu-to-race
copy site. The native race lookup remains provisional and must not be treated
as a recovered runtime assignment. A capture of the page transition / race
initialization path, or a trace of actual reflected object values at runtime,
is still needed to establish the mapping.

### Route-cursor copy is unrelated to vehicle identity (2026-10-24)

`FUN_004a8630` is not the missing `m_nVehicleId` to `m_iVehicle` handoff.
Its argument is a race route-cursor object: it follows `this+0x0c` to the
active route/segment record, then copies six dwords from record offsets
`+0x1c..+0x30` into cursor offsets `+0x78..+0x8c`. Race setup calls it for
manager cursors at `+0x8c4` and `+0x958`. It does not write either
`WackyAuto+0x84` or the linked vehicle object's `+0x88`. This removes a nearby
address/offset coincidence from the vehicle-type trace but does not change the
unresolved mapping above.

The single-player selection page also draws the selected item's two direct
`GDSprite` children, which supply its weapon and gadget icons. The seven
page-28 model groups and their material textures load without errors. GHMS
geometry now uses the executable-derived HMTS mapping and static SEGS
hierarchy. Camera fit and a fresh image comparison remain open. The Dingo item
`0xb3` has no linked `MTNL` resource, so its fragmentary preview is not
explained by a missing motion library. A later bounded BRTR walk found motion
links on page-28 items `0xc5`, `0xd5`, and `0xf7`, with libraries `0x7f8`,
`0x7f9`, and `0x7fa`; the same libraries appear on multiplayer page 61. Their
target segment mapping and playback remain unresolved; see
[`menu-motion-associations-20261023.md`](menu-motion-associations-20261023.md).
Page 61's separate item set and preview rendering are now mapped independently.
A bounded BRTR traversal finds seven items on each page with the same vehicle
ID order `[6, 3, 5, 1, 2, 4, 0]` and the same per-row GMDL/GHMS resource sets.
Their serialized transforms differ, so the native loader keeps separate model
groups for the two pages. Page `0x1c` has banner sprites `0x378` and `0x91`,
while page `0x3d` has `0x415` and `0x414`; each page's selected item owns its
own weapon/gadget sprites. The renderer now selects those page-specific sprites
and transforms from the active page ID. This is static WGM evidence and code
coverage; a native multiplayer-page visual comparison remains open. This note
records resource wiring and format evidence, not a completed visual match.

## Multiplayer-page native capture (2026-10-23)

The native executable was launched in a private 640x480 Xvfb display and
navigated through the two-player path to page `0x3d`. The selected vehicle card
rendered the page-61 banner, vehicle model, item icons, French text, and
footer. The capture is
`native-multiplayer-car-selection-20261023.png`.
This confirms that the page-specific records load and draw without a startup or
selection-page failure. No same-state Wine capture was available, so it does
not establish pixel parity or validate the page-61 model transforms against
the original renderer.

An additional bounded BRTR dump of `WMenuCarItem 0xb3` confirms its model
hierarchy is one empty `GDModel 0xb4` grouping node, four wheel `GDModel`
children (`GMDL 0x7aa`, `0x7ab`, `0x7ad`, `0x7ac`), and one `GDHModel 0xa7`
child (`GHMS 0x7ae`). All five model child transforms are identity rotations
with translations: the wheels sit at approximately `(35.95, 8.60, 85.94)`,
`(-44.83, 8.60, 85.94)`, `(35.95, 8.60, -78.43)`, and
`(-44.83, 8.60, -78.43)`; the GHMS root is at `(-12.46, -3.65, -0.57)`.
The group contains no additional linked tower or driver mesh and no MTNL link.
This is direct serialized-data evidence; it does not establish whether the
single GHMS contains the complete static car/driver composition or needs an
engine pose/render state that the native preview still lacks.

## Fresh Dingo preview capture and remaining scene content

An isolated 640x480 run navigated through the native start and single-player
pages to the second car item, `DINGO-LIMOUSINE`. Its real renderer output is
`native-dingo-selection-current-20261023.png`; the original Wine comparison
frame is `wine-character-select-dingo-limousine-20261023.png`. The native card
text, banner, purple field, and two icons appear, but the car geometry is
fragmentary and its composition does not match Wine. The capture confirms that
successfully loading the five model resources is not enough to establish a
correct preview. In the current yaw sweep, a +90-degree view produces a more
coherent side silhouette than the page's -90-degree preview, but its front
faces the opposite direction from the Wine reference; neither view restores
the tower/driver composition.

The serialized page also contains information the current preview path does
not consume. `WMenuPage 0x1c` owns a container `GDModel 0x31`, whose linked
children are `GMDL 0x7ca` and `GMDL 0x7c9`; their nested transforms are
non-identity. Two root `GDCamera` nodes, objects `0x2` and `0x3c`, serialize
`1.04719758`, `100`, `2`, and `20000` in properties `0x2c`–`0x2f`. Only
property `0x2c` is registered as `m_nFOV`; the other three meanings remain
unresolved.
Object `0x2` has matrix property `0x1b`:

```text
1, 0, 0, 0, 0.956305, 0.292372, 0, -0.292372, 0.956305,
186.456909, 307.654297, -578.945251
```

These are direct WGM observations. Their attachment to the selected car draw,
the two models' visual roles, and the camera matrix convention in this page
remain unresolved; do not treat them as a recovered camera solution yet. The
next reconstruction step is to trace page-28's original draw path, determine
which camera and page-level model nodes it submits, then render those nodes
with their transforms and compare a fresh same-state Wine/native frame.

The executable's projection-matrix builder `FUN_00435410` reads four camera
fields: FOV, an integer projection/aspect parameter, near distance, and far
distance. It constructs the perspective matrix with `tan(FOV/2)`, uses
`sqrt(parameter * 0.0075)` for the horizontal/vertical scale pair, and places
the near/far values into the depth terms. This is consistent with the page
camera values `(1.04719758, 100, 2, 20000)` and suggests that properties
`0x2d`–`0x2f` feed those projection fields, though their serialized property
names remain unverified. The native `wpc_mesh_draw_camera_preview()` path
implements the corresponding perspective factors for the race camera, but the
selection renderer does not call it: `wgm_anim.c` calls
`wpc_mesh_draw_material_preview()` with a bounds-fitted camera. It therefore
does not yet consume any `GDCamera` properties on this page.

`FUN_004354a0` obtains the active camera through the active viewport's `+0x186`
field, builds projection with `FUN_00435410`, and builds the view transform
with `FUN_00435290`. The latter negates the camera position before composing
the view matrix, so the serialized camera transform is not itself the view
matrix. These functions describe WPC's general 3D path, but the exported
decompilation does not identify which viewport is bound while page `0x1c` draws
its car. The two camera nodes' presence in `menu.WGM` alone cannot establish
that binding. Recover that viewport/camera selection before replacing the
bounds-fitted selection preview with a serialized camera.

## Viewport binding call path

The viewport binder `FUN_0044a1f0` stores its viewport argument in the render
context at `+0xbc`. It reads the active camera at the viewport object's
`+0x186`, passes that camera to the graphics context's virtual method at
`+0x50`, and composes camera-dependent matrices. The static call path is
`FUN_004ae060` -> thunk `0x0040100f` -> `FUN_0044a1f0`.

`FUN_004ae060` accepts a viewport, invokes the binder, obtains the same camera,
sets its near and far distances to 50 and 20,000, computes a viewport rectangle
scaled to the render target, updates the camera's projection scale, and sets
graphics projection state. The thunk for this function (`0x00402720`) appears
as a function pointer in `.rdata` at file offset `0x114d24`, consistent with a
virtual method dispatch. The constructors `FUN_004addf0` and `FUN_004adef0`
install vtable `0x00514c74` on their object; the pointer at `0x00514c74 +
0xb0` is the `0x00402720` thunk. Thus `FUN_004ae060` is virtual slot 44 of
that renderer/backend class. The class identity and static initialization path
are known, but static data still does not tie this method or a specific
viewport instance to page `0x1c`. Trace where the page's render submission
calls this slot and which viewport argument it supplies.

## Page-level model classification

The page-level models are not the missing driver or tower parts of the Dingo
vehicle group. A bounded native mesh load gives `GMDL 0x7ca` local bounds
`(-53.01, 0, -64.31)..(53.01, 0, 64.31)` and material `0x2b4`; that material
decodes as a black star-shaped mask. `GMDL 0x7c9` is a flat `300x300` plane
using the same `0x2b3` checker-pattern material already tiled by the current
page renderer. Both model nodes occur beneath the page-level container on
vehicle page `0x1c` and multiplayer page `0x3d`, with the same parent transform
and nearly identical child transforms. This supports interpreting `0x7c9` as
the patterned page plane and `0x7ca` as a floor/shadow graphic. The plane's
serialized scene transform and the renderer's projection are still needed to
match the reference; these nodes do not explain the missing tower/driver
geometry in the Dingo model preview.

The native page now loads material `0x2b4` through the WGM texture decoder and
draws its transparent star mask beneath the selected-car preview. A fresh
640x480 native capture shows the mask at the same location and approximate
footprint as the Wine frame; see
`dingo-shadow-review-comparison.png`.
The screen-space placement is fitted from that comparison; the page model's
full 3D transform and camera path are still not ported. The model preview
remains fragmentary, so this restores the separate ground/shadow graphic
without claiming the vehicle geometry is resolved. A UV-triangle overlay of
`GHMS 0x7ae` on its decoded 256x256 `GMAT 0x301` atlas shows coordinates
spanning much of the atlas, including diagonal links across unrelated-looking
regions. The overlay is preserved as
`dingo-ghms-uv-atlas-overlay-20261023.png`.
This is a renderer/UV diagnostic, not proof of malformed source UVs: a
temporary V inversion made the preview visibly worse. Next compare the
executable's texture-coordinate upload and UV-origin handling with the native
sampler, then verify page camera/projection and model transforms against a
same-state Wine frame. Static analysis of `FUN_004354a0` confirms WPC submits
`1-T` for texture coordinates. The effective result still depends on the
texture upload's row orientation; details and the remaining evidence gap are
in [`wpc-glide-triangle-projection-20261023.md`](wpc-glide-triangle-projection-20261023.md).

## Camera and alternate-archive check

The model loader now composes ancestor and local 48-byte transforms using the
same row-major 3x3-plus-translation convention as the course scene loader. For
page 28's current serialized ancestors this does not resolve the visible
fragmentation.
The fresh frame after the composition fix is
`native-dingo-selection-parent-xform-20261023.png`;
the wheel and body pieces remain visibly miscomposed, so the remaining issue
lies in the page's model/camera draw path or animated pose.

I rendered the two page camera transforms directly through the software
preview. Both cameras serialize almost the same position and orientation; using
that pose and their 60-degree FOV makes the Dingo appear only about 60x75
pixels. That is much smaller than WPC's roughly 200x245-pixel car-and-driver
composition, so the camera's active viewport/projection conversion is still
missing and the direct-camera preview was removed.
The isolated trial frame is
`native-selection-direct-camera-20261023.png`.

I also tried `general.wgm` group `GDModel 0xbf` (`GHMS 0x102b5` and four
`GMDL` wheel meshes) as a replacement for vehicle ID 3. Its own serialized
transforms render a bright pink vehicle with `P` badges, not the gray
Dingo-Limousine in the Wine capture. The replacement was reverted. The
captured render is
`general-model-0xbf-preview-20261023.png`;
the group must remain unassigned until its actual vehicle is identified.

## Dingo GHMS segment data

## Selection background scale check (2026-11-02)

The native page previously repeated the 64x64 `GTEX 0x12` at native pixel
size. Comparing the supplied Wine screen with the isolated native Dingo page
showed that this made the purple pattern repeat about twice as often. The
renderer now scales each repeated tile to 128x128, following the observed 2x
logical menu viewport. The new full-client capture is
`native-selection-background-2x-20261102.png`.
The pattern scale looks closer to the Wine frame, but this remains a visual
inference: the page's `GMDL 0x7c9` plane and its camera projection are not yet
used for the backdrop. The Dingo model remains fragmented in this capture.

`GHMS 0x7ae` has 840 positions and 521 triangles. Its `HMTS` payload has an
eight-byte header `(0, 581)` followed by six eight-byte rows. The row words
are retained verbatim because their roles are not established. Its `SEGS`
payload contains six 36-byte rows. In each row, the u32 values at offsets
`+12` and `+16` match a vertex count and start that partition the mesh:
`(8,832)`, `(550,282)`, `(11,271)`, `(164,107)`, `(77,30)`, and `(30,0)`.
These source-vertex spans cover `0,840)` exactly; the `+20` words `(57,31,20,
20,10,0)` give their HMTS map starts. The first u32s
`0xffffffff, 0xffffffff, 1, 2, 1, 1` are parent segment indices. The other
fields identify TSET spans and local translations; see
`ghms-segment-pose-disassembly-20261023.md` for the executable call path and
corpus checks. The C loader applies that static mapping and hierarchy when it
loads the GHMS, including when combined with the four wheel meshes. `MTNL`
animation and visual verification against a fresh Wine frame remain open.

## Fresh same-state Wine/native capture

On 2026-10-23, I launched the installed C binary in a private 640x480 Xvfb
display and navigated to the Dingo-Limousine item. A Wine run with the mounted
disc and the configured temporary prefix reached the same item. The fresh
client captures are
[`wine-dingo-selection-runtime-20261023.png`
and
`native-dingo-selection-fresh-20261023.png`;
the side-by-side pair is
`dingo-selection-fresh-comparison-20261023.png`.
Both are 640x480 and show the same card, text, icons, and car choice. This
confirms that the current native sources still render a fragmented, low
silhouette for `GHMS 0x7ae`, while Wine renders the complete tower/driver
composition. The native preview viewport and overall width are close enough
that another global fit change is not supported by this pair; segment pose,
camera orientation/projection, and draw composition remain candidates.

The menu material linked by `GHMS 0x7ae` is `GMAT 0x301`. Its serialized
`TWOS` chunk is the little-endian value `1`. The executable registers `TWOS`
as the material's face-culling flag, and the native material reader preserves
it as `two_sided`; therefore ordinary back-face culling of this GHMS material
does not explain the missing pieces. This excludes that material-state
hypothesis, but does not rule out other draw-state or pose differences.

## Current-source recheck

On 2026-10-23, the current `wpc-native` binary was launched under a private
Xvfb display. Keyboard events navigated from the main page through start and
mode selection to the car page, then selected Dingo-Limousine with Right. The
fresh client frame is
`native-dingo-selection-current-20261023.png`;
the side-by-side comparison with the same-card Wine capture is
`dingo-selection-current-live-comparison-20261023.png`.
It confirms the present render still lacks the tower/driver and has mismatched
body fragments.

## Azimuth sweep of the loaded Dingo group

To separate a simple camera-yaw error from missing pose or scene content, I
loaded the same Dingo mesh group and material set used by the game renderer and
rendered it at 16 evenly spaced azimuths (22.5-degree steps), keeping the
current fitted camera elevation, scale, viewport, and material state fixed. The
contact sheet is
`dingo-selection-yaw-sweep-20261023.png`.
The pink driver geometry becomes more visible at several angles, confirming it
is present in the loaded group, but none of the views reveals the tall tower
seen in Wine. This rules out azimuth alone as the explanation for the missing
tower. It does not distinguish a bad segment pose, a different camera
elevation/transform, incomplete draw submission, or a separate scene element.
The port keeps its existing menu view while that distinction remains open.

## Counterfactual HMTS map-order experiment

`SEGS` 0x7ae contains a high child segment whose translated source-vertex bounds
reach Y=285.3, but its HMTS map span overlaps spans from lower-numbered rows.
I rendered a temporary variant that changed only the map-population order from
descending to ascending. The counterfactual contact sheet is
`dingo-selection-ascending-map-experiment-20261023.png`.
It exposes a tall tower and driver across the azimuth sweep, while also creating
large stretched triangles and broken body surfaces. The executable's
`FUN_00437040` decompilation explicitly iterates the runtime SEGS array from its
last row to its first, so the ascending experiment is not evidence to change
the native implementation. The initial reader at `FUN_00436c30` and clone
path at `FUN_00436e90`/`FUN_00436f00` both preserve serialized row order. This
confirms the loop direction and serialized row order, but direct inspection of
the installed `GHMS 0x7ae` payload exposes an unresolved contradiction in the
resulting map ownership:

| Segment | Parent | Source vertices | HMTS span | Local translation | TSET / triangles |
|---:|---:|---:|---:|---|---:|
| 0 | root | 832 + 8 | 57 + 8 | `(0, 0, 0)` | 0 / 32 |
| 1 | root | 282 + 550 | 31 + 550 | `(12.27, 8.10, 4.92)` | 1 / 335 |
| 2 | 1 | 271 + 11 | 20 + 11 | `(0, -10.38, -37.64)` | 2 / 0 |
| 3 | 2 | 107 + 164 | 20 + 164 | `(0.18, 150.78, -58.32)` | 3 / 66 |
| 4 | 1 | 30 + 77 | 10 + 77 | `(14.65, 56.39, 12.72)` | 4 / 66 |
| 5 | 1 | 0 + 30 | 0 + 30 | `(-20.80, 48.49, 13.95)` | 5 / 22 |

Segment 3's hierarchical source bounds reach Y=285.28, consistent with the
upper tower geometry. The descending fill assigns it HMTS slots 20..183, then
segment 2 overwrites 20..30 and segment 1 overwrites 31..183. Segment 3
therefore owns no final map slot. All VIND references in TSET 3 lie in
20..183, and after those overwrites resolve to segments 1/2, not segment 3.
No VIND reference in the resource resolves to segment 3. This agrees with the
missing tower in the native render but conflicts with WPC's complete preview.

The native loader still matches the recovered loop and
`FUN_00436ff0` first-occurrence reuse rule; reversing map fill exposes the
tower but corrupts other triangles. Do not change precedence based on that
counterfactual alone. Next trace the runtime `GHMS 0x7ae` object and its
HMTS/SEGS arrays at `FUN_00437040`, then compare the runtime map pairs consumed
at `FUN_004385c0` with these archive-derived rows. This distinguishes a
different runtime resource/clone state from a field-interpretation error or
an omitted mapping stage.

## Runtime HMTS/SEGS trace attempt (2026-10-27)

The installed WoW64 Wine prefix launched WPC normally under Xvfb. It first
displayed `Anim Play Error`; an XTest mouse click dismissed that dialog and
the process proceeded through the Appaloosa splash and intro video. Escape
input was not usable in this run: it led to a WPC page fault at `0x004b5cf4`.

Attaching `winedbg --gdb --no-start` to the running WPC PID, both during
startup and after the game modules had loaded, stopped execution at
`krnl386.exe16:0x76eb3076` while reading `0xffbb11b0`. The stack was in
`krnl386.exe16` and WoW64 `ntdll`, before the breakpoint at `0x00437040`.
Continuing repeated the fault; WineDbg's native `pass` command terminated the
process. No HMTS or SEGS runtime values were captured. Attaching the native
WineDbg frontend reproduced the same exception.

A direct read of the host process's `/proc/<pid>/mem` was also denied by
Linux `kernel.yama.ptrace_scope=1`, so it could not serve as a fallback memory
reader. This is an environment/debugger limitation, not evidence about the
runtime mesh arrays. Resume the trace in a debugger-compatible Wine/Windows
environment: break at `FUN_00437040`, identify the `GDHMesh` with six SEGS
rows, dump its HMTS pairs and all 36-byte SEGS rows, then compare them against
the archive table above before changing the native mapping.

## Live Dingo minidump retry (2026-10-27)

While the user was on the Dingo selection screen in the desktop Wine process
(Windows PID `0x12c`), `winedbg --minidump /tmp/wpc-dingo-live.dmp 0x12c` was
attempted without sending any game input. WineDbg again faulted at
`krnl386.exe16:0x76eb3076` while reading `0xffbb11b0`. It produced no minidump;
after the failed attach, WPC and its window were no longer present. Do not
attempt another WineDbg attach/minidump on the user's live game in this WoW64
environment. A debugger-compatible Windows/Wine environment is required for
the runtime HMTS/SEGS capture.

I tested a temporary 0.58 viewport-height projection center instead of 0.50.
It moved the vehicle downward and clipped the wheels, while the missing upper
composition remained absent. I reverted that change and rebuilt the centered
baseline. This rules out that single screen-space center adjustment; it does
not distinguish a bad GHMS pose from incomplete scene submission, active
viewport state, or other render-state differences. The page camera link still
needs to be traced from the original runtime path before changing the native
camera again.
